// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Halo Launcher - Minecraft Launcher
 *  Copyright (C) 2026 introllmortal <introllmortal@users.noreply.github.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "ManagedPackManifest.h"

#include <QDesktopServices>
#include <QDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <type_traits>

#include "Application.h"
#include "BaseInstance.h"
#include "Markdown.h"
#include "StringUtils.h"
#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/modrinth/ModrinthAPI.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "translations/TranslationsModel.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {

QString uiLabel(const QString& translated, const char* englishLiteral, const QString& ruFallback)
{
    return TranslationsModel::trFallback(APPLICATION ? APPLICATION->translations() : nullptr, translated, englishLiteral, ruFallback);
}

void extractDeltaManifestFields(const QJsonObject& obj, HaloPackVersion& pv)
{
    for (const QString& key : { QStringLiteral("delta_manifest"), QStringLiteral("deltaManifest") }) {
        if (!obj.contains(key)) {
            continue;
        }
        if (obj[key].isObject()) {
            pv.deltaManifest = obj[key].toObject();
            pv.hasDeltaManifest = true;
            return;
        }
        if (obj[key].isString()) {
            pv.deltaManifestUrl = QUrl(obj[key].toString());
            pv.hasDeltaManifest = pv.deltaManifestUrl.isValid();
            return;
        }
    }
    if (obj.contains(QStringLiteral("delta_manifest_url"))) {
        pv.deltaManifestUrl = QUrl(obj[QStringLiteral("delta_manifest_url")].toString());
        pv.hasDeltaManifest = pv.deltaManifestUrl.isValid();
        return;
    }
    if (obj.contains(QStringLiteral("assets"))) {
        for (const auto& a : obj[QStringLiteral("assets")].toArray()) {
            const auto aObj = a.toObject();
            if (aObj[QStringLiteral("name")].toString().endsWith(QStringLiteral("delta_manifest.json"), Qt::CaseInsensitive)) {
                pv.deltaManifestUrl = QUrl(aObj[QStringLiteral("browser_download_url")].toString());
                pv.hasDeltaManifest = pv.deltaManifestUrl.isValid();
                return;
            }
        }
    }
}

bool fetchHaloLatestCandidate(BaseInstance* inst, QWidget* parent, ManagedPackUpdateCandidate& outCandidate, bool& outIsUpToDate)
{
    const QString manifestUrl = inst->settings()->get(QStringLiteral("ManagedPackURL")).toString().trimmed();
    if (manifestUrl.isEmpty()) {
        CustomMessageBox::selectable(
            parent,
            uiLabel(QObject::tr("Missing Update Source"), "Missing Update Source", QStringLiteral("Не указан источник обновлений")),
            uiLabel(QObject::tr("ManagedPackURL is not configured for this modpack."),
                    "ManagedPackURL is not configured for this modpack.",
                    QStringLiteral("Для этой сборки не задан адрес обновлений (ManagedPackURL).")),
            QMessageBox::Warning)
            ->exec();
        return false;
    }

    auto netJob = makeShared<NetJob>(QStringLiteral("HaloPack::CheckUpdate"), APPLICATION->network());
    auto [action, response] = Net::ApiDownload::makeByteArray(QUrl(manifestUrl));
    netJob->addNetAction(action);

    ProgressDialog progress(parent);
    progress.setSkipButton(true, uiLabel(QObject::tr("Abort"), "Abort", QStringLiteral("Отмена")));
    progress.execWithTask(netJob.get());

    if (!netJob->wasSuccessful()) {
        if (!netJob->wasAborted()) {
            const QString reason = netJob->failReason().isEmpty()
                                       ? uiLabel(QObject::tr("Failed to check for modpack updates."),
                                                 "Failed to check for modpack updates.",
                                                 QStringLiteral("Не удалось проверить обновления модпака."))
                                       : netJob->failReason();
            CustomMessageBox::selectable(parent, QObject::tr("Error"), reason, QMessageBox::Critical)->exec();
        }
        return false;
    }

    const QVector<HaloPackVersion> versions = ManagedPackUpdater::parseHaloPackVersions(*response);
    if (versions.isEmpty()) {
        CustomMessageBox::selectable(
            parent, QObject::tr("Error"),
            uiLabel(QObject::tr("No downloadable modpack versions were found."),
                    "No downloadable modpack versions were found.",
                    QStringLiteral("Не найдено доступных версий сборки.")),
            QMessageBox::Warning)
            ->exec();
        return false;
    }

    const HaloPackVersion& latest = versions.first();
    const QString currentName = inst->getManagedPackVersionName();
    const QString currentId = inst->getManagedPackVersionID();
    outIsUpToDate = (!currentName.isEmpty() && latest.version == currentName) || (!currentId.isEmpty() && latest.version == currentId);

    outCandidate.sourceLabel = QStringLiteral("GitHub");
    outCandidate.versionId = latest.version;
    outCandidate.versionName = latest.version;
    outCandidate.displayName = latest.name;
    outCandidate.downloadUrl = latest.downloadUrl;
    outCandidate.changelogHtml = StringUtils::htmlListPatch(markdownToHTML(latest.changelog.toUtf8()));
    outCandidate.isDelta = latest.hasDeltaManifest;
    outCandidate.isHaloPack = true;
    outCandidate.haloVersion = latest;
    return true;
}

template <typename ApiType>
bool fetchPlatformLatestCandidate(BaseInstance* inst,
                                  const QString& sourceLabel,
                                  bool isFlame,
                                  QWidget* parent,
                                  ManagedPackUpdateCandidate& outCandidate,
                                  bool& outIsUpToDate)
{
    ApiType api;
    ModPlatform::IndexedPack pack{ .addonId = inst->getManagedPackID() };

    QVector<ModPlatform::IndexedVersion> versions;
    QString errorReason;
    ResourceAPI::Callback<QVector<ModPlatform::IndexedVersion>> callbacks{};
    callbacks.on_succeed = [&versions](QVector<ModPlatform::IndexedVersion>& doc) { versions = doc; };
    callbacks.on_fail = [&errorReason](const QString& reason, int) { errorReason = reason; };

    Task::Ptr fetchJob = api.getProjectVersions({ .pack = std::make_shared<ModPlatform::IndexedPack>(pack),
                                                  .mcVersions = {},
                                                  .loaders = {},
                                                  .resourceType = ModPlatform::ResourceType::Modpack,
                                                  .includeChangelog = !isFlame },
                                                std::move(callbacks));
    if (!fetchJob) {
        CustomMessageBox::selectable(parent, QObject::tr("Error"), QObject::tr("Failed to start update check."), QMessageBox::Critical)
            ->exec();
        return false;
    }

    ProgressDialog progress(parent);
    progress.setSkipButton(true, uiLabel(QObject::tr("Abort"), "Abort", QStringLiteral("Отмена")));
    progress.execWithTask(fetchJob.get());

    if (!fetchJob->wasSuccessful()) {
        if (!fetchJob->wasAborted()) {
            const QString msg = errorReason.isEmpty() ? fetchJob->failReason() : errorReason;
            CustomMessageBox::selectable(
                parent, QObject::tr("Error"),
                msg.isEmpty() ? uiLabel(QObject::tr("Failed to fetch modpack versions."), "Failed to fetch modpack versions.",
                                        QStringLiteral("Не удалось получить список версий модпака."))
                              : msg,
                QMessageBox::Critical)
                ->exec();
        }
        return false;
    }

    if (versions.isEmpty()) {
        CustomMessageBox::selectable(
            parent, QObject::tr("Error"),
            uiLabel(QObject::tr("No versions available for this modpack."), "No versions available for this modpack.",
                    QStringLiteral("Для этого модпака не найдено версий.")),
            QMessageBox::Warning)
            ->exec();
        return false;
    }

    const auto& latest = versions.first();
    const QString currentName = inst->getManagedPackVersionName();
    const QString currentId = inst->getManagedPackVersionID();
    outIsUpToDate = (!currentId.isEmpty() && latest.fileId.toString() == currentId) ||
                    (!currentName.isEmpty() && latest.version == currentName);

    outCandidate.sourceLabel = sourceLabel;
    outCandidate.versionId = latest.fileId.toString();
    outCandidate.versionName = latest.version;
    outCandidate.displayName = latest.getVersionDisplayString();
    outCandidate.downloadUrl = latest.downloadUrl;

    if (!outIsUpToDate) {
        if constexpr (std::is_same_v<ApiType, FlameAPI>) {
            outCandidate.changelogHtml =
                StringUtils::htmlListPatch(api.getModFileChangelog(inst->getManagedPackID().toInt(), latest.fileId.toInt()));
        } else {
            outCandidate.changelogHtml = StringUtils::htmlListPatch(markdownToHTML(latest.changelog.toUtf8()));
        }
    }
    return true;
}

}  // namespace

namespace ManagedPackUpdater {

bool supportsOneStepUpdate(const BaseInstance* inst)
{
    if (!inst || !inst->isManagedPack()) {
        return false;
    }

    const QString packType = inst->getManagedPackType().trimmed().toLower();
    if (packType == QStringLiteral("ftb") || packType == QStringLiteral("atlauncher")) {
        return false;
    }
    if (packType == QStringLiteral("flame")) {
        if (!inst->getManagedPackID().isEmpty() && !(APPLICATION->capabilities() & Application::SupportsFlame)) {
            return false;
        }
        return true;
    }
    return packType == QStringLiteral("halo") || packType == QStringLiteral("github") || packType == QStringLiteral("custom") ||
           packType == QStringLiteral("modrinth") || packType.isEmpty();
}

QVector<HaloPackVersion> parseHaloPackVersions(const QByteArray& jsonData)
{
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Error parsing Halo pack versions JSON:" << parseError.errorString();
        return {};
    }

    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject()) {
        const auto root = doc.object();
        if (root.contains(QStringLiteral("versions"))) {
            arr = root[QStringLiteral("versions")].toArray();
        } else {
            arr.append(root);
        }
    }

    QVector<HaloPackVersion> versions;
    for (const auto& val : arr) {
        const auto obj = val.toObject();
        HaloPackVersion pv;
        pv.version = obj.contains(QStringLiteral("version")) ? obj[QStringLiteral("version")].toString()
                                                             : obj[QStringLiteral("tag_name")].toString();
        pv.name = obj.contains(QStringLiteral("name")) ? obj[QStringLiteral("name")].toString() : pv.version;
        pv.changelog = obj.contains(QStringLiteral("changelog")) ? obj[QStringLiteral("changelog")].toString()
                                                                 : obj[QStringLiteral("body")].toString();

        if (obj.contains(QStringLiteral("url"))) {
            pv.downloadUrl = QUrl(obj[QStringLiteral("url")].toString());
        } else if (obj.contains(QStringLiteral("download_url"))) {
            pv.downloadUrl = QUrl(obj[QStringLiteral("download_url")].toString());
        } else if (obj.contains(QStringLiteral("assets"))) {
            for (const auto& a : obj[QStringLiteral("assets")].toArray()) {
                const auto aObj = a.toObject();
                const QString aName = aObj[QStringLiteral("name")].toString();
                if (aName.endsWith(QStringLiteral(".mrpack"), Qt::CaseInsensitive) ||
                    aName.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                    pv.downloadUrl = QUrl(aObj[QStringLiteral("browser_download_url")].toString());
                    break;
                }
            }
        }

        extractDeltaManifestFields(obj, pv);
        if (!pv.version.isEmpty() && pv.downloadUrl.isValid()) {
            versions.append(pv);
        }
    }
    return versions;
}

bool fetchLatestCandidate(BaseInstance* inst, QWidget* parent, ManagedPackUpdateCandidate& outCandidate, bool& outIsUpToDate)
{
    const QString packType = inst->getManagedPackType();
    if (packType == QStringLiteral("modrinth")) {
        return fetchPlatformLatestCandidate<ModrinthAPI>(inst, QStringLiteral("Modrinth"), false, parent, outCandidate, outIsUpToDate);
    }
    if (packType == QStringLiteral("flame")) {
        if (!(APPLICATION->capabilities() & Application::SupportsFlame)) {
            CustomMessageBox::selectable(parent, QObject::tr("Unsupported"),
                                         QObject::tr("CurseForge updates are not supported in this build."), QMessageBox::Warning)
                ->exec();
            return false;
        }
        return fetchPlatformLatestCandidate<FlameAPI>(inst, QStringLiteral("CurseForge"), true, parent, outCandidate, outIsUpToDate);
    }
    return fetchHaloLatestCandidate(inst, parent, outCandidate, outIsUpToDate);
}

bool askConfirmUpdateDialog(BaseInstance* inst, const ManagedPackUpdateCandidate& candidate, QWidget* parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(uiLabel(QObject::tr("Modpack Update"), "Modpack Update", QStringLiteral("Обновление модпака")));
    dlg.setMinimumSize(520, 380);
    dlg.resize(560, 440);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    const QString packTitle = inst->getManagedPackName().isEmpty() ? inst->name() : inst->getManagedPackName();
    auto* titleLabel = new QLabel(packTitle, &dlg);
    titleLabel->setObjectName(QStringLiteral("inspectorTitleLabel"));
    titleLabel->setWordWrap(true);
    root->addWidget(titleLabel);

    auto* metaRow = new QHBoxLayout();
    metaRow->setSpacing(8);

    const QString currentVer = inst->getManagedPackVersionName().isEmpty() ? QStringLiteral("—") : inst->getManagedPackVersionName();
    const QString targetVer = candidate.versionName.isEmpty() ? candidate.displayName : candidate.versionName;
    auto* versionInfo = new QLabel(
        uiLabel(QObject::tr("Version: %1 → %2"), "Version: %1 → %2", QStringLiteral("Версия: %1 → %2")).arg(currentVer, targetVer),
        &dlg);
    versionInfo->setWordWrap(true);
    metaRow->addWidget(versionInfo, 1);

    if (candidate.isDelta) {
        auto* deltaBadge = new QLabel(uiLabel(QObject::tr("Delta Sync"), "Delta Sync", QStringLiteral("Дельта-обновление")), &dlg);
        deltaBadge->setObjectName(QStringLiteral("inspectorBadgeUpdate"));
        metaRow->addWidget(deltaBadge);
    }
    if (!candidate.sourceLabel.isEmpty()) {
        auto* srcLabel = new QLabel(candidate.sourceLabel, &dlg);
        srcLabel->setObjectName(QStringLiteral("inspectorMetaLabel"));
        metaRow->addWidget(srcLabel);
    }
    root->addLayout(metaRow);

    auto* browser = new QTextBrowser(&dlg);
    browser->setOpenExternalLinks(false);
    QObject::connect(browser, &QTextBrowser::anchorClicked, &dlg, [](const QUrl& url) {
        if (url.isValid() && (url.scheme() == QStringLiteral("http") || url.scheme() == QStringLiteral("https"))) {
            QDesktopServices::openUrl(url);
        }
    });
    if (candidate.changelogHtml.trimmed().isEmpty()) {
        browser->setPlainText(
            uiLabel(QObject::tr("No changelog provided."), "No changelog provided.", QStringLiteral("Список изменений не указан.")));
    } else {
        browser->setHtml(candidate.changelogHtml);
    }
    root->addWidget(browser, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);

    auto* cancelBtn = new QPushButton(uiLabel(QObject::tr("Cancel"), "Cancel", QStringLiteral("Отмена")), &dlg);
    auto* updateBtn = new QPushButton(
        QIcon::fromTheme(QStringLiteral("checkupdate")),
        uiLabel(QObject::tr("Update to %1"), "Update to %1", QStringLiteral("Обновить до %1")).arg(targetVer),
        &dlg);
    updateBtn->setObjectName(QStringLiteral("inspectorUpdateButton"));
    updateBtn->setProperty("updateAvailable", true);
    updateBtn->setDefault(true);

    QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    QObject::connect(updateBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    buttons->addWidget(cancelBtn);
    buttons->addWidget(updateBtn);
    root->addLayout(buttons);

    return dlg.exec() == QDialog::Accepted;
}

}  // namespace ManagedPackUpdater
