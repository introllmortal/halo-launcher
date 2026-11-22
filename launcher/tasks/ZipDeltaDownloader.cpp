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

#include "ZipDeltaDownloader.h"

#include "FileSystem.h"
#include "net/HttpRangeClient.h"

ZipDeltaDownloadTask::ZipDeltaDownloadTask(QNetworkAccessManager* network,
                                           const ZipDeltaPlan& plan,
                                           const QString& stagingDir,
                                           QObject* parent)
    : Task(parent), m_network(network), m_plan(plan), m_stagingDir(stagingDir)
{
    setAbortable(true);
}

bool ZipDeltaDownloadTask::abort()
{
    if (m_activeRequest) {
        m_activeRequest->abort();
        return true;
    }
    emitAborted();
    return true;
}

void ZipDeltaDownloadTask::executeTask()
{
    if (!m_network || !m_plan.valid || m_stagingDir.isEmpty()) {
        emitFailed(tr("Invalid ZIP delta download configuration."));
        return;
    }

    if (m_plan.entriesToDownload.isEmpty()) {
        emitSucceeded();
        return;
    }

    m_ranges = ZipRangeUtils::coalesceByteRanges(m_plan.entriesToDownload, m_plan.cdOffset);
    if (m_ranges.isEmpty()) {
        emitFailed(tr("Failed to plan ZIP byte ranges."));
        return;
    }

    m_currentRangeIndex = 0;
    m_completedRangeBytes = 0;
    m_totalRangeBytes = 0;
    for (const CoalescedByteRange& range : m_ranges) {
        m_totalRangeBytes += (range.end - range.start + 1ULL);
    }

    downloadNextRange();
}

void ZipDeltaDownloadTask::downloadNextRange()
{
    if (m_currentRangeIndex >= m_ranges.size()) {
        emitSucceeded();
        return;
    }

    const CoalescedByteRange range = m_ranges.at(m_currentRangeIndex);
    const quint64 chunkLen = range.end - range.start + 1ULL;

    setStatus(tr("Downloading changed files (chunk %1/%2)...").arg(m_currentRangeIndex + 1).arg(m_ranges.size()));

    m_activeRequest = new HttpRangeRequest(m_network, m_plan.archiveUrl, range.start, range.end, this);
    connect(m_activeRequest, &Task::progress, this, [this](qint64 current, qint64) {
        setProgress(static_cast<qint64>(m_completedRangeBytes + static_cast<quint64>(current)),
                    static_cast<qint64>(m_totalRangeBytes));
    });
    connect(m_activeRequest, &Task::succeeded, this, [this, range, chunkLen]() {
        const QByteArray chunkBytes = m_activeRequest->payload();
        m_activeRequest->deleteLater();
        m_activeRequest.clear();

        QString extractErr;
        for (int entryIdx : range.entryIndices) {
            const ZipRemoteEntry& entry = m_plan.entriesToDownload.at(entryIdx);
            const QString stagedPath = FS::PathCombine(m_stagingDir, entry.normalizedPath);
            if (!ZipRangeUtils::extractEntryFromChunk(chunkBytes, range.start, entry, stagedPath, &extractErr)) {
                emitFailed(extractErr.isEmpty() ? tr("Failed to extract %1").arg(entry.normalizedPath) : extractErr);
                return;
            }
        }

        m_completedRangeBytes += chunkLen;
        setProgress(static_cast<qint64>(m_completedRangeBytes), static_cast<qint64>(m_totalRangeBytes));
        ++m_currentRangeIndex;
        downloadNextRange();
    });
    connect(m_activeRequest, &Task::failed, this, [this](const QString& reason) {
        if (m_activeRequest) {
            m_activeRequest->deleteLater();
            m_activeRequest.clear();
        }
        emitFailed(reason);
    });
    connect(m_activeRequest, &Task::aborted, this, [this]() {
        if (m_activeRequest) {
            m_activeRequest->deleteLater();
            m_activeRequest.clear();
        }
        emitAborted();
    });

    m_activeRequest->start();
}
