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
#include <QVariant>

/**
 * Discovers and edits graphics/performance mod config files (Sodium, Embeddium, Rubidium,
 * Sodium/Embeddium Extra, Iridium, ImmediatelyFast, EntityCulling, Iris, Oculus).
 * Untouched keys and sections are preserved on save.
 */
class GraphicsModConfigManager {
   public:
    enum class Format {
        Json,
        Properties,
    };

    struct ConfigFileInfo {
        QString id;
        QString displayName;
        QString filePath;
        Format format = Format::Json;
        bool isSodiumFamily = false;
    };

    struct Entry {
        QString keyPath;  // Dotted path for JSON (e.g. "quality.leaves_quality") or key for .properties
        QVariant value;
    };

    static QList<ConfigFileInfo> discoverConfigs(const QString& gameRoot);
    static QList<Entry> loadEntries(const QString& filePath, Format format, QString* errorOut = nullptr);
    static bool saveEntries(const QString& filePath, Format format, const QList<Entry>& entries, QString* errorOut = nullptr);
};
