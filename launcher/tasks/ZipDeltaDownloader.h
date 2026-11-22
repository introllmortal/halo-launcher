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

#include "tasks/Task.h"
#include "tasks/ZipDeltaPlanner.h"

class HttpRangeRequest;

/**
 * Downloads coalesced byte ranges from a remote ZIP archive and extracts each
 * verified entry into stagingDir one chunk at a time to bound peak memory usage.
 */
class ZipDeltaDownloadTask : public Task {
    Q_OBJECT
   public:
    ZipDeltaDownloadTask(QNetworkAccessManager* network,
                         const ZipDeltaPlan& plan,
                         const QString& stagingDir,
                         QObject* parent = nullptr);
    ~ZipDeltaDownloadTask() override = default;

    bool abort() override;
    bool canAbort() const override { return true; }

   protected:
    void executeTask() override;

   private:
    void downloadNextRange();

    QPointer<QNetworkAccessManager> m_network;
    ZipDeltaPlan m_plan;
    QString m_stagingDir;

    QList<CoalescedByteRange> m_ranges;
    int m_currentRangeIndex = 0;
    quint64 m_totalRangeBytes = 0;
    quint64 m_completedRangeBytes = 0;
    QPointer<HttpRangeRequest> m_activeRequest;
};
