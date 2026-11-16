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
#pragma once

#include <QDateTime>
#include <QDialog>
#include <QString>
#include <QUrl>

class QLabel;
class QPushButton;
class QTextBrowser;

class ReleaseNotesDialog : public QDialog {
    Q_OBJECT

   public:
    struct ReleaseInfo {
        QString tagName;
        QString title;
        QString body;
        QString htmlUrl;
        QDateTime publishedAt;
    };

    explicit ReleaseNotesDialog(QWidget* parent = nullptr);
    explicit ReleaseNotesDialog(const ReleaseInfo& info, QWidget* parent = nullptr);

    void setReleaseInfo(const ReleaseInfo& info);
    void setStatusMessage(const QString& titleText, const QString& markdownOrPlain);

    static QUrl releaseApiUrl();
    static QString normalizeTagVersion(const QString& tagName);
    static bool parseLatestRelease(const QByteArray& jsonData, ReleaseInfo& out);
    static bool shouldAutoPrompt(const QString& latestTag, const QString& currentVersion, const QString& lastSeenVersion);

    static void openLatest(QWidget* parent);
    static void checkAndPromptIfUpdated(QWidget* parent);

   private:
    void setupUi();
    void startManualFetch();

    QLabel* m_titleLabel = nullptr;
    QLabel* m_metaLabel = nullptr;
    QTextBrowser* m_browser = nullptr;
    QPushButton* m_openGithubButton = nullptr;
    QString m_htmlUrl;
};
