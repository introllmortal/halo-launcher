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
#include "ResourcePackOrderModel.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <algorithm>

#include "minecraft/GameOptionsFile.h"

namespace {
bool isUserPackId(const QString& id)
{
    return id.startsWith(QStringLiteral("file/"));
}

QString stripFilePrefix(const QString& id)
{
    return isUserPackId(id) ? id.mid(5) : id;
}

QString toUserPackId(const QString& fileName)
{
    return QStringLiteral("file/") + fileName;
}

QString resolveDisplayName(const QString& fileName, const QHash<QString, QString>& displayNames)
{
    const QString mapped = displayNames.value(fileName).trimmed();
    return mapped.isEmpty() ? fileName : mapped;
}
}  // namespace

int ResourcePackOrderModel::userPackInsertIndex() const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        const PackEntry& e = m_entries.at(i);
        if (!e.isSystem && e.enabled) {
            return i;
        }
    }
    // No enabled user packs: keep trailing suffix system anchors at the very top,
    // while placing newly enabled user packs above prefix layers ("vanilla", "mod_resources").
    return qBound(0, m_suffixAnchorCount, static_cast<int>(m_entries.size()));
}

int ResourcePackOrderModel::findNeighborVisible(int modelIndex, int delta, bool includeSystem) const
{
    if (modelIndex < 0 || modelIndex >= m_entries.size() || (delta != -1 && delta != 1)) {
        return -1;
    }
    const PackEntry& cur = m_entries.at(modelIndex);
    if (!cur.enabled || (!includeSystem && cur.isSystem)) {
        return -1;
    }
    for (int i = modelIndex + delta; i >= 0 && i < m_entries.size(); i += delta) {
        const PackEntry& cand = m_entries.at(i);
        if (cand.enabled && (includeSystem || !cand.isSystem)) {
            return i;
        }
    }
    return -1;
}

QStringList ResourcePackOrderModel::visibleIds(bool includeSystem) const
{
    QStringList ids;
    ids.reserve(m_entries.size());
    for (const PackEntry& e : m_entries) {
        if (includeSystem || !e.isSystem) {
            ids.append(e.id);
        }
    }
    return ids;
}

bool ResourcePackOrderModel::load(const QString& optionsPath,
                                  const QString& resourcePacksDir,
                                  QString* errorOut,
                                  const QHash<QString, QString>& displayNames)
{
    m_entries.clear();
    m_suffixAnchorCount = 0;

    GameOptionsFile options;
    if (!options.loadFromFile(optionsPath, errorOut)) {
        return false;
    }

    bool rpDirExists = false;
    QSet<QString> diskFiles;
    if (!resourcePacksDir.isEmpty()) {
        const QDir dir(resourcePacksDir);
        if (dir.exists()) {
            rpDirExists = true;
            const QFileInfoList items = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QFileInfo& fi : items) {
                const QString name = fi.fileName();
                if (name.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive) || name == QStringLiteral(".index")) {
                    continue;
                }
                if (fi.isDir() || name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
                    diskFiles.insert(name);
                }
            }
        }
    }

    const QStringList rawOrder = options.stringListValue(QStringLiteral("resourcePacks"));
    QSet<QString> seenIds;

    // Reverse options.txt order so index 0 represents highest priority (top of in-game list).
    for (int i = rawOrder.size() - 1; i >= 0; --i) {
        const QString id = rawOrder.at(i);
        if (id.isEmpty() || seenIds.contains(id)) {
            continue;
        }
        seenIds.insert(id);

        const bool userPack = isUserPackId(id);
        const QString fileName = stripFilePrefix(id);
        const QString display = userPack ? resolveDisplayName(fileName, displayNames) : fileName;
        const bool exists = !userPack || !rpDirExists || diskFiles.contains(fileName);
        m_entries.append({ id, display, true, !userPack, exists });
    }

    // Record how many suffix system anchors sat above the highest-priority user pack.
    bool sawUserPack = false;
    for (const PackEntry& e : m_entries) {
        if (!e.isSystem) {
            sawUserPack = true;
            break;
        }
    }
    if (sawUserPack) {
        for (const PackEntry& e : m_entries) {
            if (!e.isSystem) {
                break;
            }
            ++m_suffixAnchorCount;
        }
    }

    // Ensure baseline "vanilla" layer exists when options.txt had no resourcePacks entry yet.
    if (m_entries.isEmpty()) {
        m_entries.append({ QStringLiteral("vanilla"), QStringLiteral("vanilla"), true, true, true });
    }

    // Append available disabled user packs at the bottom for easy toggling.
    QStringList sortedDisk = diskFiles.values();
    std::sort(sortedDisk.begin(), sortedDisk.end(), [](const QString& a, const QString& b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    for (const QString& fileName : sortedDisk) {
        const QString id = toUserPackId(fileName);
        if (!seenIds.contains(id)) {
            m_entries.append({ id, resolveDisplayName(fileName, displayNames), false, false, true });
        }
    }

    return true;
}

bool ResourcePackOrderModel::save(const QString& optionsPath, QString* errorOut) const
{
    GameOptionsFile options;
    if (!options.loadFromFile(optionsPath, errorOut)) {
        return false;
    }

    QStringList encoded;
    encoded.reserve(m_entries.size());
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        const PackEntry& entry = m_entries.at(i);
        if (entry.enabled || entry.isSystem) {
            encoded.append(entry.id);
        }
    }

    options.setStringListValue(QStringLiteral("resourcePacks"), encoded);
    return options.saveToFile(optionsPath, true, errorOut);
}

bool ResourcePackOrderModel::moveUp(int index)
{
    if (index <= 0 || index >= m_entries.size()) {
        return false;
    }
    m_entries.swapItemsAt(index, index - 1);
    return true;
}

bool ResourcePackOrderModel::moveDown(int index)
{
    if (index < 0 || index + 1 >= m_entries.size()) {
        return false;
    }
    m_entries.swapItemsAt(index, index + 1);
    return true;
}

bool ResourcePackOrderModel::moveRow(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_entries.size() || toIndex < 0 || toIndex >= m_entries.size() || fromIndex == toIndex) {
        return false;
    }
    m_entries.move(fromIndex, toIndex);
    return true;
}

bool ResourcePackOrderModel::moveVisibleUp(int modelIndex, bool includeSystem)
{
    const int target = findNeighborVisible(modelIndex, -1, includeSystem);
    if (target < 0) {
        return false;
    }
    m_entries.swapItemsAt(modelIndex, target);
    return true;
}

bool ResourcePackOrderModel::moveVisibleDown(int modelIndex, bool includeSystem)
{
    const int target = findNeighborVisible(modelIndex, 1, includeSystem);
    if (target < 0) {
        return false;
    }
    m_entries.swapItemsAt(modelIndex, target);
    return true;
}

bool ResourcePackOrderModel::reorderVisibleIds(const QStringList& orderedIds, bool includeSystem)
{
    QList<int> targetSlots;
    QHash<QString, PackEntry> visibleById;
    for (int i = 0; i < m_entries.size(); ++i) {
        const PackEntry& e = m_entries.at(i);
        if (includeSystem || !e.isSystem) {
            targetSlots.append(i);
            visibleById.insert(e.id, e);
        }
    }

    if (orderedIds.size() != targetSlots.size()) {
        return false;
    }

    QList<PackEntry> reordered;
    reordered.reserve(orderedIds.size());
    QSet<QString> usedIds;
    for (const QString& id : orderedIds) {
        if (!visibleById.contains(id) || usedIds.contains(id)) {
            return false;
        }
        usedIds.insert(id);
        reordered.append(visibleById.value(id));
    }

    std::stable_partition(reordered.begin(), reordered.end(), [](const PackEntry& e) { return e.enabled; });

    for (int i = 0; i < targetSlots.size(); ++i) {
        m_entries[targetSlots.at(i)] = reordered.at(i);
    }
    return true;
}

bool ResourcePackOrderModel::setEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }
    PackEntry& entry = m_entries[index];
    if (entry.isSystem && !enabled) {
        return false;
    }
    if (entry.enabled == enabled) {
        return false;
    }
    if (enabled) {
        const int targetIndex = userPackInsertIndex();
        m_entries[index].enabled = true;
        if (index != targetIndex) {
            m_entries.move(index, targetIndex);
        }
    } else {
        m_entries[index].enabled = false;
        if (!m_entries[index].isSystem && index + 1 < m_entries.size()) {
            m_entries.move(index, m_entries.size() - 1);
        }
    }
    return true;
}

bool ResourcePackOrderModel::setEnabledByFileName(const QString& fileName, bool enabled)
{
    const QString targetId = toUserPackId(fileName);
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).id == targetId) {
            return setEnabled(i, enabled);
        }
    }
    if (!enabled) {
        return false;
    }
    m_entries.insert(userPackInsertIndex(), { targetId, fileName, true, false, true });
    return true;
}
