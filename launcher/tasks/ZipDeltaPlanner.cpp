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

#include "ZipDeltaPlanner.h"

#include <QHash>
#include <QSet>

#include "FileSystem.h"
#include "net/HttpRangeClient.h"
#include "tasks/DeltaSyncTask.h"
#include "tasks/ManagedFilesIndex.h"

namespace {
constexpr quint64 kMaxEocdTailBytes = 65558ULL;
}  // namespace

ZipDeltaPlanTask::ZipDeltaPlanTask(QNetworkAccessManager* network,
                                   const QUrl& archiveUrl,
                                   const QString& instanceRoot,
                                   const QString& gameRoot,
                                   const QString& targetVersion,
                                   QObject* parent)
    : Task(parent),
      m_network(network),
      m_archiveUrl(archiveUrl),
      m_instanceRoot(instanceRoot),
      m_gameRoot(gameRoot),
      m_targetVersion(targetVersion)
{
    setAbortable(true);
    m_plan.archiveUrl = archiveUrl;
    m_plan.targetVersion = targetVersion;
}

bool ZipDeltaPlanTask::abort()
{
    if (m_activeRequest) {
        m_activeRequest->abort();
        return true;
    }
    emitAborted();
    return true;
}

void ZipDeltaPlanTask::executeTask()
{
    if (!m_network || !m_archiveUrl.isValid() || m_gameRoot.isEmpty()) {
        emitFailed(tr("Invalid parameters for ZIP delta planning."));
        return;
    }
    startProbeRequest();
}

void ZipDeltaPlanTask::startProbeRequest()
{
    setStatus(tr("Checking modpack archive metadata..."));
    m_activeRequest = new HttpRangeRequest(m_network, m_archiveUrl, 0, 3, this);
    connect(m_activeRequest, &Task::succeeded, this, [this]() {
        const quint64 totalSize = m_activeRequest->totalRemoteSize();
        m_activeRequest->deleteLater();
        m_activeRequest.clear();

        if (totalSize < 22) {
            emitFailed(tr("Remote archive is too small or missing Content-Range size."));
            return;
        }
        m_plan.totalRemoteZipBytes = totalSize;
        startTailRequest();
    });
    connect(m_activeRequest, &Task::failed, this, [this](const QString& reason) {
        if (m_activeRequest) {
            m_plan.rangeNotSupported = m_activeRequest->rangeNotSupported();
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

void ZipDeltaPlanTask::startTailRequest()
{
    const quint64 totalSize = m_plan.totalRemoteZipBytes;
    m_tailStartOffset = (totalSize > kMaxEocdTailBytes) ? (totalSize - kMaxEocdTailBytes) : 0ULL;
    const quint64 tailEnd = totalSize - 1ULL;

    setStatus(tr("Reading modpack directory header..."));
    m_activeRequest = new HttpRangeRequest(m_network, m_archiveUrl, m_tailStartOffset, tailEnd, this);
    connect(m_activeRequest, &Task::succeeded, this, [this]() {
        const QByteArray tailBytes = m_activeRequest->payload();
        m_activeRequest->deleteLater();
        m_activeRequest.clear();

        const ZipEocdInfo eocd = ZipRangeUtils::parseEocd(tailBytes, m_tailStartOffset, m_plan.totalRemoteZipBytes);
        if (!eocd.valid) {
            emitFailed(tr("Failed to locate ZIP Central Directory."));
            return;
        }
        m_plan.cdOffset = eocd.cdOffset;
        startCentralDirectoryRequest(eocd);
    });
    connect(m_activeRequest, &Task::failed, this, [this](const QString& reason) {
        if (m_activeRequest) {
            m_plan.rangeNotSupported = m_activeRequest->rangeNotSupported();
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

void ZipDeltaPlanTask::startCentralDirectoryRequest(const ZipEocdInfo& eocd)
{
    const quint64 cdEnd = eocd.cdOffset + eocd.cdSize - 1ULL;
    setStatus(tr("Fetching modpack file index..."));
    m_activeRequest = new HttpRangeRequest(m_network, m_archiveUrl, eocd.cdOffset, cdEnd, this);
    connect(m_activeRequest, &Task::succeeded, this, [this]() {
        const QByteArray cdBytes = m_activeRequest->payload();
        m_activeRequest->deleteLater();
        m_activeRequest.clear();
        buildLocalDiffPlan(cdBytes);
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

void ZipDeltaPlanTask::buildLocalDiffPlan(const QByteArray& cdBytes)
{
    setStatus(tr("Comparing local files with update..."));
    bool parsedOk = false;
    const QList<ZipRemoteEntry> parsedEntries = ZipRangeUtils::parseCentralDirectory(cdBytes, &parsedOk);
    if (!parsedOk) {
        emitFailed(tr("Corrupt ZIP Central Directory."));
        return;
    }

    QList<ZipRemoteEntry> uniqueEntries;
    QHash<QString, int> seenPathIndex;
    for (const ZipRemoteEntry& entry : parsedEntries) {
        if (!DeltaSyncTask::isPathSafe(entry.normalizedPath)) {
            continue;
        }
        if (entry.method != 0 && entry.method != 8) {
            emitFailed(tr("Unsupported ZIP compression method (%1) for %2.").arg(entry.method).arg(entry.normalizedPath));
            return;
        }
        if (auto it = seenPathIndex.constFind(entry.normalizedPath); it != seenPathIndex.constEnd()) {
            uniqueEntries[it.value()] = entry;
        } else {
            seenPathIndex.insert(entry.normalizedPath, uniqueEntries.size());
            uniqueEntries.append(entry);
        }
    }

    QSet<QString> managedPathSet;
    for (const ZipRemoteEntry& entry : uniqueEntries) {
        if (ManagedFilesIndex::hasSymlinkInRelativeChain(m_gameRoot, entry.normalizedPath)) {
            emitFailed(tr("Refusing to update through symlink path: %1").arg(entry.normalizedPath));
            return;
        }
        m_plan.allManagedEntries.append(entry);
        managedPathSet.insert(entry.normalizedPath);

        const QString localPath = FS::PathCombine(m_gameRoot, entry.normalizedPath);
        quint32 localCrc = 0;
        qint64 localSize = -1;
        if (ZipRangeUtils::computeFileCrc32(localPath, localCrc, localSize) &&
            localSize == static_cast<qint64>(entry.uncompressedSize) && localCrc == entry.crc32) {
            ++m_plan.skippedCount;
            m_plan.skippedBytes += entry.uncompressedSize;
            continue;
        }

        m_plan.entriesToDownload.append(entry);
        m_plan.downloadCompressedBytes += entry.compressedSize;
        m_plan.downloadUncompressedBytes += entry.uncompressedSize;
    }

    if (!m_instanceRoot.isEmpty()) {
        const ManagedFilesManifest previousIndex = ManagedFilesIndex::load(m_instanceRoot);
        m_plan.pathsToRemove = ManagedFilesIndex::computeRemovedPaths(previousIndex, managedPathSet);
    }

    m_plan.valid = true;
    emitSucceeded();
}
