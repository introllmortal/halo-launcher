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
#include <QStringList>
#include <optional>

class BaseInstance;

namespace InstanceProfilePreset {

struct Preset {
    QString id;
    QString name;
    QDateTime updatedAt;
    QStringList enabledMods;
    QStringList knownMods;
    std::optional<QString> shaderPack;
};

/// Returns the root folder storing presets for an instance (<instanceRoot>/halo-presets).
QString presetsRoot(const BaseInstance* instance);

/// Lists all saved presets for an instance, sorted by display name.
QList<Preset> listPresets(const BaseInstance* instance);

/// Returns the active preset ID for an instance, or an empty string if unset.
QString activePresetId(const BaseInstance* instance);

/// Persists the active preset ID for an instance outside mods/config/saves.
void setActivePresetId(BaseInstance* instance, const QString& id);

/// Captures the current mod state and optional shaderPack setting as a named preset.
std::optional<Preset> saveCurrentAsPreset(BaseInstance* instance,
                                          const QString& displayName,
                                          bool captureShaderSetting = true,
                                          QString* errorOut = nullptr);

/// Applies a saved preset to the instance with preflight collision checks and rollback.
bool applyPreset(BaseInstance* instance, const QString& presetId, QString* errorOut = nullptr);

/// Deletes a saved preset by ID and clears active preset ID if it matched.
bool deletePreset(BaseInstance* instance, const QString& presetId);

}  // namespace InstanceProfilePreset
