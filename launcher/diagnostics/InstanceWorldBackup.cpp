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
#include "InstanceStorageCleaner.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "archive/ArchiveWriter.h"

namespace InstanceStorageCleaner {

namespace {

constexpr int kMaxTraverseDepth = 64;
constexpr int kMaxCollisionSuffix = 999;

QString backupTr(const char* text)
{
    return QCoreApplication::translate("InstanceStorageCleaner", text);
}

bool hasParentTraversal(const QString& rawPath)
{
    const QStringList parts = QDir::fromNativeSeparators(rawPath).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.contains(QStringLiteral(".."));
}

bool isCanonicalInside(const QString& candidateCanonical, const QString& rootCanonical)
{
    if (candidateCanonical.isEmpty() || rootCanonical.isEmpty()) {
        return false;
    }
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    const QString cleanRoot = QDir::cleanPath(rootCanonical);
    const QString cleanCandidate = QDir::cleanPath(candidateCanonical);
    if (QString::compare(cleanCandidate, cleanRoot, cs) == 0) {
        return true;
    }
    const QString prefix = cleanRoot.endsWith(QLatin1Char('/')) ? cleanRoot : (cleanRoot + QLatin1Char('/'));
    return cleanCandidate.startsWith(prefix, cs);
}

QString sanitizeWorldPrefix(const QString& worldDirPath)
{
    QString sanitized = QFileInfo(worldDirPath).fileName().trimmed();
    sanitized.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\s]+")), QStringLiteral("_"));
    while (sanitized.endsWith(QLatin1Char('.')) || sanitized.endsWith(QLatin1Char('_'))) {
        sanitized.chop(1);
    }
    return sanitized.isEmpty() ? QStringLiteral("world") : sanitized;
}

bool collectSafeWorldFiles(const QString& dirPath,
                           const QString& canonicalRoot,
                           QFileInfoList& outFiles,
                           int depth = 0)
{
    if (depth > kMaxTraverseDepth) {
        return false;
    }
    const QFileInfoList entries =
        QDir(dirPath).entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& entry : entries) {
        if (entry.isSymLink()) {
            continue;
        }
        const QString canon = entry.canonicalFilePath();
        if (!isCanonicalInside(canon, canonicalRoot)) {
            continue;
        }
        if (entry.isDir()) {
            if (!collectSafeWorldFiles(entry.absoluteFilePath(), canonicalRoot, outFiles, depth + 1)) {
                return false;
            }
        } else if (entry.isFile()) {
            outFiles.append(entry);
        }
    }
    return true;
}

std::optional<QString> failBackup(const QString& tmpZipPath, QString* errorOut, const QString& message)
{
    if (!tmpZipPath.isEmpty()) {
        QFile::remove(tmpZipPath);
    }
    if (errorOut) {
        *errorOut = message;
    }
    return std::nullopt;
}

}  // namespace

std::optional<QString> createWorldZipBackupAtRoot(const QString& instanceRoot, const QString& worldDirPath, QString* errorOut)
{
    if (errorOut) {
        errorOut->clear();
    }
    if (instanceRoot.trimmed().isEmpty()) {
        return failBackup({}, errorOut, backupTr("Instance root path is empty."));
    }
    if (worldDirPath.trimmed().isEmpty() || hasParentTraversal(worldDirPath)) {
        return failBackup({}, errorOut, backupTr("Invalid world directory path."));
    }

    const QFileInfo worldInfo(worldDirPath);
    if (!worldInfo.exists() || !worldInfo.isDir() || worldInfo.isSymLink()) {
        return failBackup({}, errorOut, backupTr("World directory does not exist or is not a directory."));
    }

    const QString cleanWorldDir = worldInfo.absoluteFilePath();
    const QString canonicalWorldDir = worldInfo.canonicalFilePath();
    if (canonicalWorldDir.isEmpty()) {
        return failBackup({}, errorOut, backupTr("Failed to resolve canonical world directory."));
    }

    const QString backupsDir = FS::PathCombine(instanceRoot, QStringLiteral("world-backups"));
    if (!FS::ensureFolderPathExists(backupsDir)) {
        return failBackup({}, errorOut, backupTr("Failed to create world-backups directory."));
    }

    const QString baseName = QStringLiteral("%1-%2").arg(
        sanitizeWorldPrefix(cleanWorldDir), QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    QString stem = baseName;
    QString finalZipPath = FS::PathCombine(backupsDir, stem + QStringLiteral(".zip"));
    QString tmpZipPath = FS::PathCombine(backupsDir, stem + QStringLiteral(".tmp.zip"));
    for (int suffix = 1; (QFileInfo::exists(finalZipPath) || QFileInfo::exists(tmpZipPath)) && suffix <= kMaxCollisionSuffix; ++suffix) {
        stem = QStringLiteral("%1-%2").arg(baseName).arg(suffix);
        finalZipPath = FS::PathCombine(backupsDir, stem + QStringLiteral(".zip"));
        tmpZipPath = FS::PathCombine(backupsDir, stem + QStringLiteral(".tmp.zip"));
    }
    if (QFileInfo::exists(finalZipPath) || QFileInfo::exists(tmpZipPath)) {
        return failBackup({}, errorOut, backupTr("Could not allocate a unique backup archive filename."));
    }

    QFileInfoList files;
    if (!collectSafeWorldFiles(cleanWorldDir, canonicalWorldDir, files)) {
        return failBackup({}, errorOut, backupTr("Failed to enumerate world directory files."));
    }

    MMCZip::ArchiveWriter writer(tmpZipPath);
    if (!writer.open()) {
        return failBackup(tmpZipPath, errorOut, backupTr("Failed to open temporary backup archive."));
    }

    const QDir worldQDir(cleanWorldDir);
    for (const QFileInfo& fileInfo : files) {
        const QString relativeDest = worldQDir.relativeFilePath(fileInfo.absoluteFilePath());
        if (!writer.addFile(fileInfo.absoluteFilePath(), relativeDest)) {
            writer.close();
            return failBackup(tmpZipPath, errorOut, backupTr("Failed to add '%1' to backup archive.").arg(relativeDest));
        }
    }

    if (!writer.close()) {
        return failBackup(tmpZipPath, errorOut, backupTr("Failed to finalize backup archive."));
    }
    if (!QFile::rename(tmpZipPath, finalZipPath)) {
        return failBackup(tmpZipPath, errorOut, backupTr("Failed to move temporary backup archive to destination."));
    }
    return finalZipPath;
}

std::optional<QString> createWorldZipBackup(BaseInstance* instance, const QString& worldDirPath, QString* errorOut)
{
    if (!instance) {
        return failBackup({}, errorOut, backupTr("Instance is null."));
    }
    if (instance->isRunning()) {
        return failBackup({}, errorOut, backupTr("Cannot back up a world while the instance is running."));
    }
    return createWorldZipBackupAtRoot(instance->instanceRoot(), worldDirPath, errorOut);
}

}  // namespace InstanceStorageCleaner
