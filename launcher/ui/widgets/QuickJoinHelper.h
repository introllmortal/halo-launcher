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
#include <QObject>
#include <QString>
#include <optional>

#include "minecraft/launch/MinecraftTarget.h"

class BaseInstance;

namespace QuickJoinHelper {

struct Target {
    enum class Kind { Server, World };

    Kind kind = Kind::Server;
    QString label;
    QString targetValue;  // host[:port] for Server, world folderName for World

    MinecraftTarget::Ptr toMinecraftTarget() const;
};

/// Discovers saved multiplayer servers (from servers.dat) and worlds (most recently played first).
QList<Target> discoverTargets(const BaseInstance* instance);

/// Resolves the pinned quick-join target from a pre-discovered target list.
std::optional<Target> resolveActiveTarget(const BaseInstance* instance, const QList<Target>& available);

/// Resolves the pinned quick-join target for an instance, or falls back to newest world / first server.
std::optional<Target> resolveActiveTarget(const BaseInstance* instance);

/// Persists (or clears when nullopt) the user's pinned quick-join target in <instanceRoot>/halo-quickjoin.json.
void savePinnedTarget(BaseInstance* instance, const std::optional<Target>& target);

}  // namespace QuickJoinHelper
