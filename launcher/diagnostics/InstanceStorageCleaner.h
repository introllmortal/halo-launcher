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
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <optional>

class BaseInstance;

namespace InstanceStorageCleaner {

enum class Category {
    Logs,
    CrashReports,
    Cache,
    ModBackups,
    ExcessSnapshots,
};

struct CategoryStats {
    Category category = Category::Logs;
    QString displayName;
    qint64 bytes = 0;
    qint64 fileCount = 0;
    QStringList targetPaths;
};

struct ScanReport {
    QList<CategoryStats> categories;
    qint64 totalCleanableBytes = 0;
    qint64 totalInstanceBytes = 0;
    QString error;
};

struct InstancePaths {
    QString instanceRoot;
    QString gameRoot;
    QString modsRoot;
};

/// Captures plain instance directory paths on the GUI thread for safe off-thread scanning.
InstancePaths capturePaths(const BaseInstance* instance);

/// Scans safe allowlisted cleanup categories using value paths (safe to run via QtConcurrent::run).
ScanReport scanPaths(const InstancePaths& paths, int keepSnapshots = 3);

/// Convenience synchronous wrapper around capturePaths + scanPaths.
ScanReport scanInstance(const BaseInstance* instance, int keepSnapshots = 3);

/// Deletes allowlisted files/directories for the requested categories and returns bytes freed.
/// Refuses and returns 0 when instance is null or currently running.
qint64 cleanCategories(BaseInstance* instance, const QList<Category>& categories, int keepSnapshots = 3, QString* errorOut = nullptr);

/// Formats a byte count into a human-readable IEC string (B, KiB, MiB, GiB, TiB).
QString formatBytes(qint64 bytes);

/// Creates a compressed ZIP backup of a world directory under <instanceRoot>/world-backups/ using value root path.
std::optional<QString> createWorldZipBackupAtRoot(const QString& instanceRoot, const QString& worldDirPath, QString* errorOut = nullptr);

/// Creates a compressed ZIP backup of a world directory under <instanceRoot>/world-backups/.
/// Uses an atomic .tmp.zip write and rename; refuses null/running instances, missing dirs, or symlink/'..' traversal.
std::optional<QString> createWorldZipBackup(BaseInstance* instance, const QString& worldDirPath, QString* errorOut = nullptr);

}  // namespace InstanceStorageCleaner
