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
#include <QList>
#include <QString>
#include <QtGlobal>
#include <optional>

class BaseInstance;

namespace PackSnapshot {

constexpr int kMaxSnapshotsPerInstance = 3;

struct Info {
    QString id;
    QString dirPath;
    QDateTime createdAt;
    QString packVersion;
    QString targetVersion;
    qint64 modCount = 0;
    bool hasConfig = false;
};

struct ModEntry {
    QString fileName;
    qint64 size = 0;
    bool enabled = true;
};

enum class DiffKind {
    Added,
    Removed,
    Enabled,
    Disabled,
};

struct ModDiffEntry {
    DiffKind kind = DiffKind::Added;
    QString baseKey;
    QString leftFileName;
    QString rightFileName;
    qint64 leftSize = 0;
    qint64 rightSize = 0;
};

struct SnapshotDiff {
    QString leftLabel;
    QString rightLabel;
    QString leftPackVersion;
    QString rightPackVersion;
    QList<ModDiffEntry> modChanges;
    int totalLeftMods = 0;
    int totalRightMods = 0;

    int addedCount() const;
    int removedCount() const;
    int toggledCount() const;
    bool isEmpty() const;
    QString toMarkdown() const;
};

/// Returns the root folder storing snapshots for an instance.
QString snapshotsRoot(const BaseInstance* instance);

/// Saves a lightweight pre-update snapshot (instance.cfg, mmc-pack.json, options.txt, config/, mods manifest).
std::optional<Info> createSnapshot(BaseInstance* instance,
                                   const QString& targetVersionLabel = QString(),
                                   int maxToKeep = kMaxSnapshotsPerInstance);

/// Lists available snapshots sorted newest first.
QList<Info> listSnapshots(const BaseInstance* instance);

/// Returns true if at least one valid snapshot exists for the instance.
bool hasSnapshot(const BaseInstance* instance);

/// Restores a specific snapshot (refuses if instance is null or running).
bool restoreSnapshot(BaseInstance* instance, const Info& snapshot);

/// Restores the most recent snapshot for the instance.
bool restoreLatestSnapshot(BaseInstance* instance);

/// Reads the mod manifest from a snapshot directory (empty if missing or malformed).
QList<ModEntry> readModsManifest(const Info& snapshot);

/// Scans top-level .jar and .jar.disabled files in an instance's mods directory.
QList<ModEntry> scanCurrentMods(const BaseInstance* instance);

/// Computes a deterministic mod diff matching entries by canonical stem ignoring trailing .disabled.
SnapshotDiff diffModLists(const QList<ModEntry>& left,
                          const QList<ModEntry>& right,
                          const QString& leftLabel = {},
                          const QString& rightLabel = {});

/// Compares a saved snapshot (left) against the instance's current mods (right).
SnapshotDiff diffSnapshotToCurrent(const BaseInstance* instance, const Info& snapshot);

/// Compares two instances' current mods (left vs right).
SnapshotDiff diffInstances(const BaseInstance* left, const BaseInstance* right);

}  // namespace PackSnapshot
