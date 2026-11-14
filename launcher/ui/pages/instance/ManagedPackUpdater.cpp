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
#include "ManagedPackUpdater.h"

#include <QDir>
#include <QFileDialog>
#include <QJsonDocument>
#include <QPointer>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "InstanceTask.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "tasks/DeltaSyncTask.h"
#include "translations/TranslationsModel.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {

QString uiLabel(const QString& translated, const char* englishLiteral, const QString& ruFallback)
{
    return TranslationsModel::trFallback(APPLICATION ? APPLICATION->translations() : nullptr, translated, englishLiteral, ruFallback);
}

bool warnIfInstanceRunning(BaseInstance* inst, QWidget* parent)
{
    if (!inst || !inst->isRunning()) {
        return false;
    }
    CustomMessageBox::selectable(parent,
                                 uiLabel(QObject::tr("Game Running"), "Game Running", QStringLiteral("Игра запущена")),
                                 uiLabel(QObject::tr("Minecraft is currently running. Please close the game before updating."),
                                         "Minecraft is currently running. Please close the game before updating.",
                                         QStringLiteral("Minecraft сейчас запущен. Закройте игру перед обновлением сборки.")),
                                 QMessageBox::Warning)
        ->exec();
    return true;
}

void showUpdateOutcomeDialog(BaseInstance* inst, bool didSucceed, QWidget* parent)
{
    const QString versionName = inst ? inst->getManagedPackVersionName() : QString();
    if (didSucceed) {
        CustomMessageBox::selectable(parent,
                                     QObject::tr("Update Successful"),
                                     QObject::tr("The instance updated to pack version %1 successfully.").arg(versionName),
                                     QMessageBox::Information)
            ->show();
    } else {
        CustomMessageBox::selectable(
            parent,
            QObject::tr("Update Failed"),
            QObject::tr("The instance failed to update to pack version %1. Please check launcher logs for more information.")
                .arg(versionName),
            QMessageBox::Critical)
            ->show();
    }
}

}  // namespace

namespace ManagedPackUpdater {

bool runImportUpdate(BaseInstance* inst,
                     const QUrl& url,
                     const QString& versionID,
                     const QString& versionName,
                     QWidget* parent,
                     bool showResultDialog)
{
    if (!inst || warnIfInstanceRunning(inst, parent)) {
        return false;
    }

    QMap<QString, QString> extraInfo;
    extraInfo.insert(QStringLiteral("pack_id"), inst->getManagedPackID());
    extraInfo.insert(QStringLiteral("pack_version_id"), versionID);
    extraInfo.insert(QStringLiteral("original_instance_id"), inst->id());

    auto* extracted = new InstanceImportTask(url, parent, std::move(extraInfo));
    if (versionName.isEmpty()) {
        extracted->setName(inst->name());
    } else {
        InstanceName instName(inst->getManagedPackName(), versionName);
        QString nextName = inst->name();
        const QString currentVersionName = inst->getManagedPackVersionName();
        if (!currentVersionName.isEmpty()) {
            nextName.replace(currentVersionName, versionName);
        }
        instName.setName(nextName);
        extracted->setName(instName);
    }
    extracted->setGroup(APPLICATION->instances()->getInstanceGroup(inst->id()));
    extracted->setIcon(inst->iconKey());
    extracted->setConfirmUpdate(false);

    unique_qobject_ptr<Task> wrappedTask(APPLICATION->instances()->wrapInstanceTask(extracted));
    QObject::connect(wrappedTask.get(), &Task::failed, parent, [parent](const QString& reason) {
        CustomMessageBox::selectable(parent, QObject::tr("Error"), reason, QMessageBox::Critical)->show();
    });
    QObject::connect(wrappedTask.get(), &Task::succeeded, parent, [parent, extracted]() {
        const QStringList warnings = extracted->warnings();
        if (!warnings.isEmpty()) {
            CustomMessageBox::selectable(parent, QObject::tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    QObject::connect(wrappedTask.get(), &Task::aborted, parent, [parent]() {
        CustomMessageBox::selectable(parent, QObject::tr("Task aborted"), QObject::tr("The task has been aborted by the user."),
                                     QMessageBox::Information)
            ->show();
    });

    ProgressDialog loadDialog(parent);
    loadDialog.setSkipButton(true, QObject::tr("Abort"));
    loadDialog.execWithTask(wrappedTask.get());

    const bool didSucceed = wrappedTask->wasSuccessful();
    if (didSucceed) {
        inst->reloadSettings();
        inst->setUpdateAvailable(false);
    }
    if (showResultDialog) {
        showUpdateOutcomeDialog(inst, didSucceed, parent);
    }
    return didSucceed;
}

bool executeHaloDeltaSync(BaseInstance* inst, const HaloPackVersion& version, QWidget* parent)
{
    if (!inst || warnIfInstanceRunning(inst, parent)) {
        return false;
    }

    auto offerFullFallback = [&](const QString& reason) -> bool {
        const auto res = CustomMessageBox::selectable(
                             parent, QObject::tr("Delta Update Failed"),
                             QObject::tr("Delta update could not be completed (%1).\n\nWould you like to perform a full modpack "
                                         "archive update instead?")
                                 .arg(reason),
                             QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                             ->exec();
        if (res == QMessageBox::Yes) {
            return runImportUpdate(inst, version.downloadUrl, version.version, version.version, parent);
        }
        return false;
    };

    QJsonObject manifest = version.deltaManifest;
    if (manifest.isEmpty() && version.deltaManifestUrl.isValid()) {
        auto netJob = makeShared<NetJob>(QObject::tr("Fetch Delta Manifest"), APPLICATION->network());
        auto [action, response] = Net::ApiDownload::makeByteArray(version.deltaManifestUrl);
        netJob->addNetAction(action);

        ProgressDialog fetchDlg(parent);
        fetchDlg.setSkipButton(true, QObject::tr("Abort"));
        fetchDlg.execWithTask(netJob.get());

        if (!netJob->wasSuccessful()) {
            const QString reason = netJob->failReason().isEmpty() ? QObject::tr("Failed to download delta manifest") : netJob->failReason();
            return offerFullFallback(reason);
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(*response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return offerFullFallback(QObject::tr("Invalid delta manifest format"));
        }
        manifest = doc.object();
    }

    auto* deltaTask = new DeltaSyncTask(inst, manifest, version.version, parent);
    ProgressDialog progressDialog(parent);
    progressDialog.setSkipButton(true, QObject::tr("Abort"));
    progressDialog.execWithTask(deltaTask);

    const bool deltaSucceeded = deltaTask->wasSuccessful();
    if (!deltaSucceeded) {
        QString reason = deltaTask->failReason();
        if (reason.isEmpty()) {
            reason = deltaTask->wasAborted() ? QObject::tr("Task aborted by user") : QObject::tr("Unknown error");
        }
        delete deltaTask;
        return offerFullFallback(reason);
    }

    delete deltaTask;
    inst->reloadSettings();
    inst->setUpdateAvailable(false);

    CustomMessageBox::selectable(parent, QObject::tr("Update Successful"),
                                 QObject::tr("The instance updated to pack version %1 successfully.").arg(version.version),
                                 QMessageBox::Information)
        ->show();
    return true;
}

bool runOneStepUpdate(BaseInstance* inst, QWidget* parent)
{
    if (!supportsOneStepUpdate(inst)) {
        return false;
    }
    if (warnIfInstanceRunning(inst, parent)) {
        return false;
    }

    QPointer<BaseInstance> guard(inst);
    const QString customUrl = inst->settings()->get(QStringLiteral("ManagedPackURL")).toString().trimmed();

    if (inst->getManagedPackID().isEmpty()) {
        if (!customUrl.isEmpty()) {
            const auto res = CustomMessageBox::selectable(
                                 parent,
                                 uiLabel(QObject::tr("Modpack Update"), "Modpack Update", QStringLiteral("Обновление модпака")),
                                 uiLabel(QObject::tr("Update \"%1\" from configured URL?\n%2"),
                                         "Update \"%1\" from configured URL?\n%2",
                                         QStringLiteral("Обновить сборку «%1» из настроенного источника?\n%2"))
                                     .arg(inst->name(), customUrl),
                                 QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                                 ->exec();
            if (res == QMessageBox::Yes && guard) {
                return runImportUpdate(guard, QUrl(customUrl), {}, {}, parent);
            }
            return false;
        }

        const QUrl fileUrl = QFileDialog::getOpenFileUrl(parent, QObject::tr("Choose update file"), QDir::homePath(),
                                                         QObject::tr("Modpack") + QStringLiteral(" (*.mrpack *.zip)"));
        if (!fileUrl.isEmpty() && guard) {
            return runImportUpdate(guard, fileUrl, {}, {}, parent);
        }
        return false;
    }

    ManagedPackUpdateCandidate candidate;
    bool isUpToDate = false;
    if (!fetchLatestCandidate(inst, parent, candidate, isUpToDate) || !guard) {
        return false;
    }

    if (isUpToDate) {
        guard->setUpdateAvailable(false);
        const QString ver = candidate.versionName.isEmpty() ? guard->getManagedPackVersionName() : candidate.versionName;
        CustomMessageBox::selectable(
            parent,
            uiLabel(QObject::tr("Up to Date"), "Up to Date", QStringLiteral("Установлена последняя версия")),
            uiLabel(QObject::tr("\"%1\" is already on the latest version (%2)."),
                    "\"%1\" is already on the latest version (%2).",
                    QStringLiteral("Сборка «%1» уже обновлена до последней версии (%2)."))
                .arg(guard->name(), ver),
            QMessageBox::Information)
            ->exec();
        return false;
    }

    guard->setUpdateAvailable(true);
    if (!askConfirmUpdateDialog(guard, candidate, parent) || !guard) {
        return false;
    }
    if (warnIfInstanceRunning(guard, parent)) {
        return false;
    }

    if (candidate.isHaloPack && candidate.haloVersion.hasDeltaManifest) {
        return executeHaloDeltaSync(guard, candidate.haloVersion, parent);
    }
    return runImportUpdate(guard, candidate.downloadUrl, candidate.versionId, candidate.versionName, parent);
}

}  // namespace ManagedPackUpdater
