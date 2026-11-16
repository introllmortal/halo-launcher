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
#include "ReleaseNotesDialog.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QStringList>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "Markdown.h"
#include "Version.h"
#include "net/ApiDownload.h"
#include "net/NetJob.h"
#include "settings/SettingsObject.h"

namespace {
QString fallbackReleasesWebUrl()
{
    QString repoUrl = BuildConfig.UPDATER_GITHUB_REPO;
    if (repoUrl.isEmpty()) {
        repoUrl = QStringLiteral("https://github.com/introllmortal/halo-launcher");
    }
    if (repoUrl.endsWith(QLatin1Char('/'))) {
        repoUrl.chop(1);
    }
    return repoUrl + QStringLiteral("/releases");
}
}  // namespace

ReleaseNotesDialog::ReleaseNotesDialog(QWidget* parent) : QDialog(parent)
{
    setupUi();
    setStatusMessage(tr("Loading release notes..."), tr("Fetching latest changelog from GitHub..."));
}

ReleaseNotesDialog::ReleaseNotesDialog(const ReleaseInfo& info, QWidget* parent) : QDialog(parent)
{
    setupUi();
    setReleaseInfo(info);
}

void ReleaseNotesDialog::setupUi()
{
    setWindowTitle(tr("What's New — %1").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    resize(640, 500);
    setMinimumSize(460, 360);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(10);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("releaseNotesTitle"));
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);

    m_metaLabel = new QLabel(this);
    m_metaLabel->setObjectName(QStringLiteral("releaseNotesMeta"));
    m_metaLabel->setWordWrap(true);
    layout->addWidget(m_metaLabel);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setReadOnly(true);
    layout->addWidget(m_browser, 1);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(8);

    m_openGithubButton = new QPushButton(tr("Open on GitHub"), this);
    connect(m_openGithubButton, &QPushButton::clicked, this, [this]() {
        const QUrl target = !m_htmlUrl.isEmpty() ? QUrl(m_htmlUrl) : QUrl(fallbackReleasesWebUrl());
        DesktopServices::openUrl(target);
    });
    bottomRow->addWidget(m_openGithubButton);
    bottomRow->addStretch(1);

    auto* closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    bottomRow->addWidget(closeBox);
    layout->addLayout(bottomRow);
}

void ReleaseNotesDialog::setStatusMessage(const QString& titleText, const QString& markdownOrPlain)
{
    m_titleLabel->setText(titleText);
    m_metaLabel->hide();
    m_browser->setHtml(markdownToHTML(markdownOrPlain));
}

void ReleaseNotesDialog::setReleaseInfo(const ReleaseInfo& info)
{
    m_htmlUrl = info.htmlUrl;
    const QString heading = !info.title.isEmpty() ? info.title : info.tagName;
    m_titleLabel->setText(heading.isEmpty() ? BuildConfig.LAUNCHER_DISPLAYNAME : heading);

    QStringList metaParts;
    if (!info.tagName.isEmpty()) {
        metaParts.append(tr("Version: %1").arg(info.tagName));
    }
    if (info.publishedAt.isValid()) {
        metaParts.append(tr("Released: %1").arg(QLocale().toString(info.publishedAt.toLocalTime().date(), QLocale::ShortFormat)));
    }
    m_metaLabel->setText(metaParts.join(QStringLiteral(" • ")));
    m_metaLabel->setVisible(!metaParts.isEmpty());

    const QString bodyText = !info.body.trimmed().isEmpty() ? info.body : tr("No changelog description provided for this release.");
    m_browser->setHtml(markdownToHTML(bodyText));
}

QUrl ReleaseNotesDialog::releaseApiUrl()
{
    QString repoPath = QStringLiteral("introllmortal/halo-launcher");
    const QUrl repoUrl(BuildConfig.UPDATER_GITHUB_REPO);
    if (repoUrl.isValid() && !repoUrl.path().trimmed().isEmpty()) {
        QString cleaned = repoUrl.path();
        while (cleaned.startsWith(QLatin1Char('/'))) {
            cleaned.remove(0, 1);
        }
        while (cleaned.endsWith(QLatin1Char('/'))) {
            cleaned.chop(1);
        }
        if (cleaned.count(QLatin1Char('/')) >= 1) {
            repoPath = cleaned;
        }
    }
    return QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(repoPath));
}

QString ReleaseNotesDialog::normalizeTagVersion(const QString& tagName)
{
    QString normalized = tagName.trimmed();
    if (normalized.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        normalized.remove(0, 1);
    }
    return normalized;
}

bool ReleaseNotesDialog::parseLatestRelease(const QByteArray& jsonData, ReleaseInfo& out)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(jsonData, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("draft")).toBool()) {
        return false;
    }
    const QString tag = obj.value(QStringLiteral("tag_name")).toString().trimmed();
    if (tag.isEmpty()) {
        return false;
    }
    out.tagName = tag;
    out.title = obj.value(QStringLiteral("name")).toString().trimmed();
    out.body = obj.value(QStringLiteral("body")).toString();
    out.htmlUrl = obj.value(QStringLiteral("html_url")).toString().trimmed();
    out.publishedAt = QDateTime::fromString(obj.value(QStringLiteral("published_at")).toString(), Qt::ISODate);
    return true;
}

bool ReleaseNotesDialog::shouldAutoPrompt(const QString& latestTag, const QString& currentVersion, const QString& lastSeenVersion)
{
    if (currentVersion.isEmpty() || latestTag.isEmpty() || lastSeenVersion == currentVersion) {
        return false;
    }
    const QString tagVer = normalizeTagVersion(latestTag);
    if (tagVer.isEmpty()) {
        return false;
    }
    return Version(tagVer) >= Version(currentVersion);
}

void ReleaseNotesDialog::startManualFetch()
{
    if (!APPLICATION_DYN || !APPLICATION_DYN->network()) {
        setStatusMessage(tr("Changelog Unavailable"), tr("Network client is not initialized."));
        return;
    }

    auto netJob = makeShared<NetJob>(QStringLiteral("FetchReleaseNotes"), APPLICATION->network());
    auto [action, response] = Net::ApiDownload::makeByteArray(releaseApiUrl());
    netJob->addNetAction(action);
    netJob->setAskRetry(false);

    const QPointer<ReleaseNotesDialog> self(this);
    connect(netJob.get(), &Task::finished, this, [self, netJob, response]() {
        if (!self) {
            return;
        }
        if (!netJob->wasSuccessful()) {
            self->setStatusMessage(
                tr("Unable to Load Release Notes"),
                tr("Could not retrieve release notes from GitHub right now. You can open the releases page directly below."));
            return;
        }
        ReleaseInfo info;
        if (!parseLatestRelease(*response, info)) {
            self->setStatusMessage(tr("Unable to Load Release Notes"),
                                   tr("GitHub returned an unexpected response format. You can open the releases page directly below."));
            return;
        }
        self->setReleaseInfo(info);
        if (APPLICATION_DYN && APPLICATION_DYN->settings()) {
            APPLICATION_DYN->settings()->set(QStringLiteral("LastSeenChangelogVersion"), BuildConfig.printableVersionString());
        }
    });
    netJob->start();
}

void ReleaseNotesDialog::openLatest(QWidget* parent)
{
    auto* dlg = new ReleaseNotesDialog(parent);
    dlg->show();
    dlg->startManualFetch();
}

void ReleaseNotesDialog::checkAndPromptIfUpdated(QWidget* parent)
{
    if (!parent || !APPLICATION_DYN || !APPLICATION_DYN->settings() || !APPLICATION_DYN->network()) {
        return;
    }
    const QString currentVer = BuildConfig.printableVersionString();
    const QString lastSeen = APPLICATION->settings()->get(QStringLiteral("LastSeenChangelogVersion")).toString();
    if (currentVer.isEmpty() || lastSeen == currentVer) {
        return;
    }

    auto netJob = makeShared<NetJob>(QStringLiteral("CheckReleaseNotes"), APPLICATION->network());
    auto [action, response] = Net::ApiDownload::makeByteArray(releaseApiUrl());
    netJob->addNetAction(action);
    netJob->setAskRetry(false);

    const QPointer<QWidget> safeParent(parent);
    connect(netJob.get(), &Task::finished, parent, [safeParent, netJob, response, currentVer]() {
        if (!safeParent || !netJob->wasSuccessful() || !APPLICATION_DYN || !APPLICATION_DYN->settings()) {
            return;
        }
        ReleaseInfo info;
        if (!parseLatestRelease(*response, info)) {
            return;
        }
        const QString latestSeenNow = APPLICATION_DYN->settings()->get(QStringLiteral("LastSeenChangelogVersion")).toString();
        if (!shouldAutoPrompt(info.tagName, currentVer, latestSeenNow)) {
            return;
        }
        APPLICATION_DYN->settings()->set(QStringLiteral("LastSeenChangelogVersion"), currentVer);
        auto* dlg = new ReleaseNotesDialog(info, safeParent);
        dlg->show();
    });
    netJob->start();
}
