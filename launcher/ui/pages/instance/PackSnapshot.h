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

}  // namespace PackSnapshot
