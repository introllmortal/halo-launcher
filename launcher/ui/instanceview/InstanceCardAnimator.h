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
#include <QModelIndex>
#include <QPersistentModelIndex>

/// Tracks per-card smooth hover, selection, and press transitions without touching the item model.
class InstanceCardAnimator {
   public:
    void setHovered(const QModelIndex& index, bool hovered, bool currentlySelected);
    void setSelected(const QModelIndex& index, bool selected);
    void setPressed(const QModelIndex& index, bool pressed, bool currentlySelected);

    qreal hoverProgress(const QModelIndex& index, bool fallbackHovered) const;
    qreal selectProgress(const QModelIndex& index, bool fallbackSelected) const;
    qreal pressProgress(const QModelIndex& index) const;

    /// Advances active transitions by dtMs and collects model indices that changed visually.
    bool step(qint64 dtMs, QList<QModelIndex>& outDirtyIndices);

    void clear();
    bool hasActiveTransitions() const;

   private:
    struct Entry {
        QPersistentModelIndex index;
        qreal hover = 0.0;
        qreal hoverTarget = 0.0;
        qreal select = 0.0;
        qreal selectTarget = 0.0;
        qreal press = 0.0;
        qreal pressTarget = 0.0;
    };

    Entry* findEntry(const QModelIndex& index);
    const Entry* findEntry(const QModelIndex& index) const;
    Entry& ensureEntry(const QModelIndex& index, bool currentlySelected);

    QList<Entry> m_entries;
};
