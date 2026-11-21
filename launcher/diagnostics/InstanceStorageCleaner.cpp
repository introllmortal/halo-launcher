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
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

#include "BaseInstance.h"
#include "FileSystem.h"

namespace {

constexpr int kMaxMeasureDepth = 64;

QString cleanerTr(const char* text)
{
    return QCoreApplication::translate("InstanceStorageCleaner", text);
}

void setError(QString* errorOut, const QString& message)
{
    if (errorOut) {
        *errorOut = message;
    }
}

void measurePath(const QString& path, qint64& outBytes, qint64& outFiles, int depth = 0)
{
    if (path.isEmpty() || depth > kMaxMeasureDepth) {
        return;
    }
    const QFileInfo info(path);
    if (!info.exists() || info.isSymLink()) {
        return;
    }
    if (info.isFile()) {
        outBytes += info.size();
        outFiles += 1;
        return;
    }
    if (!info.isDir()) {
        return;
    }
    const QFileInfoList entries =
        QDir(info.absoluteFilePath()).entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        if (entry.isSymLink()) {
            continue;
        }
        if (entry.isDir()) {
            measurePath(entry.absoluteFilePath(), outBytes, outFiles, depth + 1);
        } else if (entry.isFile()) {
            outBytes += entry.size();
            outFiles += 1;
        }
    }
}

bool isSubpathOrEqual(const QString& candidatePath, const QString& parentPath)
{
    if (candidatePath.isEmpty() || parentPath.isEmpty()) {
        return false;
    }
    const QString cleanCandidate = QDir::cleanPath(QFileInfo(candidatePath).absoluteFilePath());
    const QString cleanParent = QDir::cleanPath(QFileInfo(parentPath).absoluteFilePath());
#if defined(Q_OS_WIN)
    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    if (QString::compare(cleanCandidate, cleanParent, cs) == 0) {
        return true;
    }
    const QString prefix = cleanParent.endsWith(QLatin1Char('/')) ? cleanParent : (cleanParent + QLatin1Char('/'));
    return cleanCandidate.startsWith(prefix, cs);
}

bool isBisectMarkerActive(const QString& filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile() || info.isSymLink()) {
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return true;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return true;
    }
    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("active")) && obj.value(QStringLiteral("active")).isBool()) {
        return obj.value(QStringLiteral("active")).toBool();
    }
    const QString status = obj.value(QStringLiteral("status")).toString().trimmed().toLower();
    return status != QStringLiteral("finished") && status != QStringLiteral("completed") &&
           status != QStringLiteral("cancelled") && status != QStringLiteral("inactive");
}

bool isBisectActive(const QString& instanceRoot, const QString& gameRoot)
{
    return isBisectMarkerActive(FS::PathCombine(instanceRoot, QStringLiteral("halo-bisect.json"))) ||
           isBisectMarkerActive(FS::PathCombine(instanceRoot, QStringLiteral("halo-snapshots"), QStringLiteral("halo-bisect.json"))) ||
           (!gameRoot.isEmpty() && isBisectMarkerActive(FS::PathCombine(gameRoot, QStringLiteral("halo-bisect.json"))));
}

struct SnapshotEntry {
    QString name;
    QString dirPath;
    QDateTime timestamp;
};

QDateTime readSnapshotTimestamp(const QFileInfo& dirInfo)
{
    QFile metaFile(FS::PathCombine(dirInfo.absoluteFilePath(), QStringLiteral("snapshot.json")));
    if (metaFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
        if (doc.isObject()) {
            const QDateTime parsed = QDateTime::fromString(doc.object().value(QStringLiteral("createdAtUtc")).toString(), Qt::ISODateWithMs);
            if (parsed.isValid()) {
                return parsed;
            }
        }
    }
    return dirInfo.lastModified();
}

QStringList collectExcessSnapshotDirs(const QString& instanceRoot, const QString& gameRoot, int keepSnapshots)
{
    if (isBisectActive(instanceRoot, gameRoot)) {
        return {};
    }
    const QString snapRoot = FS::PathCombine(instanceRoot, QStringLiteral("halo-snapshots"));
    const QFileInfo snapRootInfo(snapRoot);
    if (!snapRootInfo.exists() || !snapRootInfo.isDir() || snapRootInfo.isSymLink()) {
        return {};
    }

    const QFileInfoList dirs = QDir(snapRoot).entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    QList<SnapshotEntry> entries;
    entries.reserve(dirs.size());
    for (const QFileInfo& dirInfo : dirs) {
        if (!dirInfo.isSymLink()) {
            entries.push_back({ dirInfo.fileName(), dirInfo.absoluteFilePath(), readSnapshotTimestamp(dirInfo) });
        }
    }

    const qsizetype safeKeep = static_cast<qsizetype>(std::max(0, keepSnapshots));
    if (entries.size() <= safeKeep) {
        return {};
    }

    std::sort(entries.begin(), entries.end(), [](const SnapshotEntry& a, const SnapshotEntry& b) {
        if (a.timestamp.isValid() && b.timestamp.isValid() && a.timestamp != b.timestamp) {
            return a.timestamp > b.timestamp;
        }
        return a.name > b.name;
    });

    QStringList excess;
    for (qsizetype i = safeKeep; i < entries.size(); ++i) {
        excess.append(entries.at(i).dirPath);
    }
    return excess;
}

InstanceStorageCleaner::CategoryStats scanDirCategory(InstanceStorageCleaner::Category category,
                                                      const QString& displayName,
                                                      const QStringList& candidateDirs)
{
    InstanceStorageCleaner::CategoryStats stats;
    stats.category = category;
    stats.displayName = displayName;

    QSet<QString> seen;
    for (const QString& dirPath : candidateDirs) {
        const QString clean = QDir::cleanPath(dirPath);
        if (clean.isEmpty() || seen.contains(clean)) {
            continue;
        }
        seen.insert(clean);

        const QFileInfo info(clean);
        if (!info.exists() || !info.isDir() || info.isSymLink()) {
            continue;
        }
        qint64 dirBytes = 0;
        qint64 dirFiles = 0;
        measurePath(clean, dirBytes, dirFiles);
        if (dirFiles > 0) {
            stats.bytes += dirBytes;
            stats.fileCount += dirFiles;
            stats.targetPaths.append(info.absoluteFilePath());
        }
    }
    return stats;
}

InstanceStorageCleaner::CategoryStats scanModBackups(const QString& modsRoot, const QString& gameRoot)
{
    InstanceStorageCleaner::CategoryStats stats;
    stats.category = InstanceStorageCleaner::Category::ModBackups;
    stats.displayName = cleanerTr("Temporary Mod & Config Backups");

    QStringList modDirs;
    if (!gameRoot.isEmpty()) {
        modDirs.append(FS::PathCombine(gameRoot, QStringLiteral("mods")));
    }
    if (!modsRoot.isEmpty()) {
        modDirs.append(modsRoot);
    }

    QSet<QString> seenDirs;
    QSet<QString> seenFiles;
    for (const QString& rawDir : modDirs) {
        const QString cleanDir = QDir::cleanPath(rawDir);
        if (cleanDir.isEmpty() || seenDirs.contains(cleanDir)) {
            continue;
        }
        seenDirs.insert(cleanDir);

        const QFileInfo dirInfo(cleanDir);
        if (!dirInfo.exists() || !dirInfo.isDir() || dirInfo.isSymLink()) {
            continue;
        }
        const QFileInfoList files = QDir(cleanDir).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& fi : files) {
            if (fi.isSymLink()) {
                continue;
            }
            const QString name = fi.fileName();
            if (!name.endsWith(QStringLiteral(".jar.bak"), Qt::CaseInsensitive) &&
                !name.endsWith(QStringLiteral(".halo_bak"), Qt::CaseInsensitive)) {
                continue;
            }
            const QString absPath = fi.absoluteFilePath();
            if (!seenFiles.contains(absPath)) {
                seenFiles.insert(absPath);
                stats.bytes += fi.size();
                stats.fileCount += 1;
                stats.targetPaths.append(absPath);
            }
        }
    }

    if (!gameRoot.isEmpty()) {
        const QFileInfo rollbackInfo(FS::PathCombine(gameRoot, QStringLiteral("config.halo_rollback_bak")));
        if (rollbackInfo.exists() && !rollbackInfo.isSymLink()) {
            measurePath(rollbackInfo.absoluteFilePath(), stats.bytes, stats.fileCount);
            stats.targetPaths.append(rollbackInfo.absoluteFilePath());
        }
    }
    return stats;
}

InstanceStorageCleaner::CategoryStats scanExcessSnapshots(const QString& instanceRoot, const QString& gameRoot, int keepSnapshots)
{
    InstanceStorageCleaner::CategoryStats stats;
    stats.category = InstanceStorageCleaner::Category::ExcessSnapshots;
    stats.displayName = cleanerTr("Old Update Snapshots");

    for (const QString& snapDir : collectExcessSnapshotDirs(instanceRoot, gameRoot, keepSnapshots)) {
        measurePath(snapDir, stats.bytes, stats.fileCount);
        stats.targetPaths.append(snapDir);
    }
    return stats;
}

void removePathCountingFreed(const QString& path, qint64& freedBytes, QStringList& errors)
{
    const QFileInfo info(path);
    if (!info.exists() || info.isSymLink()) {
        return;
    }
    qint64 entryBytes = 0;
    qint64 entryFiles = 0;
    measurePath(info.absoluteFilePath(), entryBytes, entryFiles);
    if (FS::deletePath(info.absoluteFilePath()) && !QFileInfo::exists(info.absoluteFilePath())) {
        freedBytes += entryBytes;
    } else {
        errors.append(cleanerTr("Failed to remove %1").arg(info.absoluteFilePath()));
    }
}

bool validateWritableInstance(const BaseInstance* instance, const QString& runningMsg, QString* errorOut)
{
    if (!instance) {
        setError(errorOut, cleanerTr("Instance is null."));
        return false;
    }
    if (instance->isRunning()) {
        setError(errorOut, runningMsg);
        return false;
    }
    if (instance->instanceRoot().isEmpty()) {
        setError(errorOut, cleanerTr("Instance root path is empty."));
        return false;
    }
    return true;
}

}  // namespace

namespace InstanceStorageCleaner {

InstancePaths capturePaths(const BaseInstance* instance)
{
    if (!instance) {
        return {};
    }
    return { instance->instanceRoot(), instance->gameRoot(), instance->modsRoot() };
}

ScanReport scanPaths(const InstancePaths& paths, int keepSnapshots)
{
    ScanReport report;
    const QString& instanceRoot = paths.instanceRoot;
    if (instanceRoot.isEmpty()) {
        report.error = cleanerTr("Instance root path is empty.");
        return report;
    }

    const QString& gameRoot = paths.gameRoot;
    qint64 instanceFiles = 0;
    measurePath(instanceRoot, report.totalInstanceBytes, instanceFiles);
    if (!gameRoot.isEmpty() && !isSubpathOrEqual(gameRoot, instanceRoot)) {
        qint64 separateFiles = 0;
        measurePath(gameRoot, report.totalInstanceBytes, separateFiles);
    }

    const QString logsDir = gameRoot.isEmpty() ? QString() : FS::PathCombine(gameRoot, QStringLiteral("logs"));
    const QString crashDir = gameRoot.isEmpty() ? QString() : FS::PathCombine(gameRoot, QStringLiteral("crash-reports"));
    QStringList cacheDirs{ FS::PathCombine(instanceRoot, QStringLiteral(".tmp")) };
    if (!gameRoot.isEmpty()) {
        cacheDirs.prepend(FS::PathCombine(gameRoot, QStringLiteral("cache")));
        cacheDirs.prepend(FS::PathCombine(gameRoot, QStringLiteral(".cache")));
    }

    report.categories.append(scanDirCategory(Category::Logs, cleanerTr("Game Logs"), { logsDir }));
    report.categories.append(scanDirCategory(Category::CrashReports, cleanerTr("Crash Reports"), { crashDir }));
    report.categories.append(scanDirCategory(Category::Cache, cleanerTr("Transient Caches"), cacheDirs));
    report.categories.append(scanModBackups(paths.modsRoot, gameRoot));
    report.categories.append(scanExcessSnapshots(instanceRoot, gameRoot, keepSnapshots));

    for (const CategoryStats& cat : report.categories) {
        report.totalCleanableBytes += cat.bytes;
    }
    return report;
}

ScanReport scanInstance(const BaseInstance* instance, int keepSnapshots)
{
    if (!instance) {
        ScanReport report;
        report.error = cleanerTr("Instance is null.");
        return report;
    }
    return scanPaths(capturePaths(instance), keepSnapshots);
}

qint64 cleanCategories(BaseInstance* instance, const QList<Category>& categories, int keepSnapshots, QString* errorOut)
{
    setError(errorOut, QString());
    if (!validateWritableInstance(instance, cleanerTr("Cannot clean storage while the instance is running."), errorOut) ||
        categories.isEmpty()) {
        return 0;
    }

    const ScanReport report = scanInstance(instance, keepSnapshots);
    if (!report.error.isEmpty()) {
        setError(errorOut, report.error);
        return 0;
    }

    qint64 freedBytes = 0;
    QStringList errors;
    for (const CategoryStats& stats : report.categories) {
        if (!categories.contains(stats.category)) {
            continue;
        }
        if (stats.category == Category::Logs || stats.category == Category::CrashReports) {
            for (const QString& dirPath : stats.targetPaths) {
                const QFileInfo dirInfo(dirPath);
                if (!dirInfo.exists() || !dirInfo.isDir() || dirInfo.isSymLink()) {
                    continue;
                }
                for (const QFileInfo& entry : QDir(dirPath).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot)) {
                    removePathCountingFreed(entry.absoluteFilePath(), freedBytes, errors);
                }
            }
        } else {
            for (const QString& targetPath : stats.targetPaths) {
                removePathCountingFreed(targetPath, freedBytes, errors);
            }
        }
    }

    if (!errors.isEmpty()) {
        setError(errorOut, errors.join(QStringLiteral("; ")));
    }
    return freedBytes;
}

QString formatBytes(qint64 bytes)
{
    if (bytes <= 0) {
        return QStringLiteral("0 B");
    }
    static constexpr const char* kUnits[] = { "B", "KiB", "MiB", "GiB", "TiB" };
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < 4) {
        value /= 1024.0;
        ++unitIndex;
    }
    if (unitIndex == 0) {
        return QStringLiteral("%1 B").arg(bytes);
    }
    const int decimals = (value >= 100.0) ? 0 : (value >= 10.0 ? 1 : 2);
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', decimals), QLatin1String(kUnits[unitIndex]));
}

}  // namespace InstanceStorageCleaner
