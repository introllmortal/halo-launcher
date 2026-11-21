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

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * Manages the active resource pack load order stored in options.txt (`resourcePacks`).
 *
 * Minecraft stores `resourcePacks` from lowest priority (first) to highest priority (last).
 * This model exposes entries in UI display order: index 0 = highest priority (top of list).
 * Built-in/mod resource layers (entries without `file/` prefix) are preserved and protected
 * from accidental deletion or displacement around the user pack stack.
 */
class ResourcePackOrderModel {
   public:
    struct PackEntry {
        QString id;           // Raw options.txt entry, e.g. "file/MyPack.zip" or "vanilla"
        QString displayName;  // Human-readable name
        bool enabled = true;
        bool isSystem = false;  // True for built-in/mod layers ("vanilla", "mod_resources", "mod/...")
        bool existsOnDisk = true;
    };

    bool load(const QString& optionsPath,
              const QString& resourcePacksDir,
              QString* errorOut = nullptr,
              const QHash<QString, QString>& displayNames = {});
    bool save(const QString& optionsPath, QString* errorOut = nullptr) const;

    const QList<PackEntry>& entries() const { return m_entries; }
    int count() const { return m_entries.size(); }
    QStringList visibleIds(bool includeSystem) const;

    bool moveUp(int index);
    bool moveDown(int index);
    bool moveRow(int fromIndex, int toIndex);
    bool moveVisibleUp(int modelIndex, bool includeSystem);
    bool moveVisibleDown(int modelIndex, bool includeSystem);
    bool reorderVisibleIds(const QStringList& orderedIds, bool includeSystem);
    bool setEnabled(int index, bool enabled);
    bool setEnabledByFileName(const QString& fileName, bool enabled);

   private:
    int userPackInsertIndex() const;
    int findNeighborVisible(int modelIndex, int delta, bool includeSystem) const;

    QList<PackEntry> m_entries;
    int m_suffixAnchorCount = 0;
};
