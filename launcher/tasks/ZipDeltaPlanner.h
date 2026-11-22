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

#include <QList>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "tasks/Task.h"
#include "tasks/ZipRangeUtils.h"

class HttpRangeRequest;

struct ZipDeltaPlan {
    bool valid = false;
    bool rangeNotSupported = false;
    QUrl archiveUrl;
    QString targetVersion;
    quint64 totalRemoteZipBytes = 0;
    quint64 cdOffset = 0;

    QList<ZipRemoteEntry> allManagedEntries;
    QList<ZipRemoteEntry> entriesToDownload;
    QStringList pathsToRemove;

    int skippedCount = 0;
    quint64 skippedBytes = 0;
    quint64 downloadCompressedBytes = 0;
    quint64 downloadUncompressedBytes = 0;
};

/**
 * Inspects a remote ZIP modpack archive via HTTP Range requests (tail + Central Directory)
 * and builds an exact incremental update plan against local gameRoot files.
 */
class ZipDeltaPlanTask : public Task {
    Q_OBJECT
   public:
    ZipDeltaPlanTask(QNetworkAccessManager* network,
                     const QUrl& archiveUrl,
                     const QString& instanceRoot,
                     const QString& gameRoot,
                     const QString& targetVersion,
                     QObject* parent = nullptr);
    ~ZipDeltaPlanTask() override = default;

    bool abort() override;
    bool canAbort() const override { return true; }

    [[nodiscard]] const ZipDeltaPlan& plan() const { return m_plan; }

   protected:
    void executeTask() override;

   private:
    void startProbeRequest();
    void startTailRequest();
    void startCentralDirectoryRequest(const ZipEocdInfo& eocd);
    void buildLocalDiffPlan(const QByteArray& cdBytes);

    QPointer<QNetworkAccessManager> m_network;
    QUrl m_archiveUrl;
    QString m_instanceRoot;
    QString m_gameRoot;
    QString m_targetVersion;

    quint64 m_tailStartOffset = 0;
    QPointer<HttpRangeRequest> m_activeRequest;
    ZipDeltaPlan m_plan;
};
