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

#include <QJsonObject>
#include <QList>
#include <QString>

#include "net/NetJob.h"
#include "tasks/Task.h"

class BaseInstance;

struct DeltaFileItem {
    enum class Action { Update, Remove };

    QString path;
    QString sha1;
    qint64 size = 0;
    QString url;
    Action action = Action::Update;
};

class DeltaSyncTask : public Task {
    Q_OBJECT
   public:
    DeltaSyncTask(BaseInstance* instance,
                  const QJsonObject& manifest,
                  const QString& targetVersion,
                  QObject* parent = nullptr);
    ~DeltaSyncTask() override;

    bool abort() override;

   protected:
    void executeTask() override;

   private slots:
    void onDownloadJobSucceeded();
    void onDownloadJobFailed(const QString& reason);
    void onDownloadJobAborted();

   private:
    bool parseManifest();
    bool validatePathSecurity(const QString& rawPath) const;
    bool createPreUpdateSnapshot();
    void rotateBackups(const QString& backupsDir, int maxKeep);
    void startStagedDownload();
    bool validateDownloadedFiles();
    bool executeRollbackJournalAndCommit();
    void rollbackJournal(const QStringList& movedFiles);
    void cleanupTemporaryDirs();

   private:
    BaseInstance* m_instance = nullptr;
    QJsonObject m_manifest;
    QString m_targetVersion;
    QList<DeltaFileItem> m_items;

    QString m_stagingDir;
    QString m_backupDir;
    NetJob::Ptr m_downloadJob;
};
