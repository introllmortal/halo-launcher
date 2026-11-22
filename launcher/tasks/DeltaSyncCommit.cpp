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

#include "DeltaSyncTask.h"

#if defined(LAUNCHER_APPLICATION)
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "tasks/ManagedFilesIndex.h"

void DeltaSyncTask::rollbackJournal(const QStringList& movedFiles)
{
    setStatus(tr("Rolling back changes..."));
    const QString gameRoot = m_instance->gameRoot();

    for (const QString& relPath : movedFiles) {
        const QString backupPath = FS::PathCombine(m_backupDir, relPath);
        const QString originalPath = FS::PathCombine(gameRoot, relPath);
        if (QFile::exists(backupPath)) {
            FS::ensureFilePathExists(originalPath);
            if (QFile::exists(originalPath)) {
                QFile::remove(originalPath);
            }
            FS::move(backupPath, originalPath);
        }
    }

    cleanupTemporaryDirs();
    emitFailed(tr("File locked or access denied. Delta sync rolled back. Please use full archive import."));
}

bool DeltaSyncTask::executeRollbackJournalAndCommit()
{
    const QString gameRoot = m_instance->gameRoot();

    setStatus(tr("Preparing rollback journal..."));
    if (QDir(m_backupDir).exists()) {
        FS::deletePath(m_backupDir);
    }
    if (!QDir().mkpath(m_backupDir)) {
        cleanupTemporaryDirs();
        emitFailed(tr("File locked or access denied. Delta sync rolled back. Please use full archive import."));
        return false;
    }

    QStringList movedFiles;
    for (const auto& item : m_items) {
        if (ManagedFilesIndex::hasSymlinkInRelativeChain(gameRoot, item.path)) {
            rollbackJournal(movedFiles);
            return false;
        }
        const QString targetPath = FS::PathCombine(gameRoot, item.path);
        if (QFile::exists(targetPath)) {
            const QString backupPath = FS::PathCombine(m_backupDir, item.path);
            FS::ensureFilePathExists(backupPath);
            if (QFile::exists(backupPath)) {
                QFile::remove(backupPath);
            }
            if (!FS::move(targetPath, backupPath)) {
                rollbackJournal(movedFiles);
                return false;
            }
            movedFiles.append(item.path);
        }
    }

    const QString cleanGameRoot = QDir::cleanPath(QDir(gameRoot).absolutePath());
    setStatus(tr("Committing updates..."));
    for (const auto& item : m_items) {
        if (item.action == DeltaFileItem::Action::Update) {
            const QString stagedPath = FS::PathCombine(m_stagingDir, item.path);
            const QString targetPath = FS::PathCombine(gameRoot, item.path);
            if (QFile::exists(stagedPath)) {
                FS::ensureFilePathExists(targetPath);
                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }
                if (!FS::move(stagedPath, targetPath)) {
                    rollbackJournal(movedFiles);
                    return false;
                }
            }
        } else if (item.action == DeltaFileItem::Action::Remove) {
            const QString targetPath = FS::PathCombine(gameRoot, item.path);
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }
            QDir parentDir = QFileInfo(targetPath).dir();
            while (QDir::cleanPath(parentDir.absolutePath()) != cleanGameRoot && parentDir.exists() && parentDir.isEmpty()) {
                const QString dirName = parentDir.dirName();
                if (!parentDir.cdUp() || !parentDir.rmdir(dirName)) {
                    break;
                }
            }
        }
    }

    m_instance->setManagedPackVersionID(m_targetVersion);
    m_instance->setManagedPackVersionName(m_targetVersion);
    m_instance->saveNow();
    persistManagedIndex();

    cleanupTemporaryDirs();
    emitSucceeded();
    return true;
}

void DeltaSyncTask::persistManagedIndex()
{
    if (!m_instance) {
        return;
    }

    if (m_useZipPlan && m_zipPlan.valid) {
        ManagedFilesManifest indexManifest;
        indexManifest.version = m_targetVersion;
        indexManifest.files.reserve(m_zipPlan.allManagedEntries.size());
        for (const ZipRemoteEntry& entry : m_zipPlan.allManagedEntries) {
            ManagedFileRecord record;
            record.path = entry.normalizedPath;
            record.size = static_cast<qint64>(entry.uncompressedSize);
            record.crc32 = entry.crc32;
            indexManifest.files.append(record);
        }
        ManagedFilesIndex::save(m_instance->instanceRoot(), indexManifest);
        return;
    }

    ManagedFilesIndex::save(m_instance->instanceRoot(),
                            ManagedFilesIndex::scanGameRoot(m_instance->gameRoot(), m_targetVersion));
}

void DeltaSyncTask::cleanupTemporaryDirs()
{
    if (!m_stagingDir.isEmpty() && QDir(m_stagingDir).exists()) {
        FS::deletePath(m_stagingDir);
    }
    if (!m_backupDir.isEmpty() && QDir(m_backupDir).exists()) {
        FS::deletePath(m_backupDir);
    }
}
#endif
