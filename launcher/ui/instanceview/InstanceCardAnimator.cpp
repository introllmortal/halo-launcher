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
#include "InstanceCardAnimator.h"

#include <QEasingCurve>
#include <QtMath>

namespace {
constexpr qreal kHoverDurationMs = 150.0;
constexpr qreal kSelectDurationMs = 160.0;
constexpr qreal kPressDurationMs = 90.0;

qreal easeChannel(qreal t)
{
    static const QEasingCurve curve(QEasingCurve::InOutCubic);
    return curve.valueForProgress(qBound<qreal>(0.0, t, 1.0));
}

bool stepChannel(qreal& value, qreal target, qint64 dtMs, qreal durationMs)
{
    if (qFuzzyCompare(value + 1.0, target + 1.0)) {
        if (value != target) {
            value = target;
        }
        return false;
    }
    const qreal delta = static_cast<qreal>(qMax<qint64>(1, dtMs)) / durationMs;
    if (value < target) {
        value = qMin(target, value + delta);
    } else {
        value = qMax(target, value - delta);
    }
    return true;
}

bool isChannelSettled(qreal value, qreal target)
{
    return qFuzzyCompare(value + 1.0, target + 1.0);
}
}  // namespace

void InstanceCardAnimator::setHovered(const QModelIndex& index, bool hovered, bool currentlySelected)
{
    if (!index.isValid()) {
        return;
    }
    const qreal target = hovered ? 1.0 : 0.0;
    if (!hovered && !findEntry(index)) {
        return;
    }
    Entry& entry = ensureEntry(index, currentlySelected);
    entry.hoverTarget = target;
}

void InstanceCardAnimator::setSelected(const QModelIndex& index, bool selected)
{
    if (!index.isValid()) {
        return;
    }
    // Start from the opposite edge only if this card isn't already tracked.
    Entry* existing = findEntry(index);
    if (!existing) {
        Entry entry;
        entry.index = index;
        entry.select = selected ? 0.0 : 1.0;
        entry.selectTarget = selected ? 1.0 : 0.0;
        m_entries.append(entry);
        return;
    }
    existing->selectTarget = selected ? 1.0 : 0.0;
}

void InstanceCardAnimator::setPressed(const QModelIndex& index, bool pressed, bool currentlySelected)
{
    if (!index.isValid()) {
        return;
    }
    const qreal target = pressed ? 1.0 : 0.0;
    if (!pressed && !findEntry(index)) {
        return;
    }
    Entry& entry = ensureEntry(index, currentlySelected);
    entry.pressTarget = target;
}

qreal InstanceCardAnimator::hoverProgress(const QModelIndex& index, bool fallbackHovered) const
{
    if (const Entry* entry = findEntry(index)) {
        return easeChannel(entry->hover);
    }
    return fallbackHovered ? 1.0 : 0.0;
}

qreal InstanceCardAnimator::selectProgress(const QModelIndex& index, bool fallbackSelected) const
{
    if (const Entry* entry = findEntry(index)) {
        return easeChannel(entry->select);
    }
    return fallbackSelected ? 1.0 : 0.0;
}

qreal InstanceCardAnimator::pressProgress(const QModelIndex& index) const
{
    if (const Entry* entry = findEntry(index)) {
        return easeChannel(entry->press);
    }
    return 0.0;
}

bool InstanceCardAnimator::step(qint64 dtMs, QList<QModelIndex>& outDirtyIndices)
{
    outDirtyIndices.clear();
    if (m_entries.isEmpty()) {
        return false;
    }

    for (qsizetype i = m_entries.size() - 1; i >= 0; --i) {
        Entry& entry = m_entries[i];
        if (!entry.index.isValid()) {
            m_entries.removeAt(i);
            continue;
        }

        bool changed = false;
        changed |= stepChannel(entry.hover, entry.hoverTarget, dtMs, kHoverDurationMs);
        changed |= stepChannel(entry.select, entry.selectTarget, dtMs, kSelectDurationMs);
        changed |= stepChannel(entry.press, entry.pressTarget, dtMs, kPressDurationMs);

        if (changed) {
            outDirtyIndices.append(entry.index);
        }

        const bool settled = isChannelSettled(entry.hover, entry.hoverTarget) && isChannelSettled(entry.select, entry.selectTarget) &&
                             isChannelSettled(entry.press, entry.pressTarget);
        if (settled && qFuzzyIsNull(entry.hoverTarget) && qFuzzyIsNull(entry.pressTarget)) {
            m_entries.removeAt(i);
        }
    }

    return hasActiveTransitions();
}

void InstanceCardAnimator::clear()
{
    m_entries.clear();
}

bool InstanceCardAnimator::hasActiveTransitions() const
{
    for (const Entry& entry : m_entries) {
        if (!entry.index.isValid()) {
            continue;
        }
        if (!isChannelSettled(entry.hover, entry.hoverTarget) || !isChannelSettled(entry.select, entry.selectTarget) ||
            !isChannelSettled(entry.press, entry.pressTarget)) {
            return true;
        }
    }
    return false;
}

InstanceCardAnimator::Entry* InstanceCardAnimator::findEntry(const QModelIndex& index)
{
    for (Entry& entry : m_entries) {
        if (entry.index == index) {
            return &entry;
        }
    }
    return nullptr;
}

const InstanceCardAnimator::Entry* InstanceCardAnimator::findEntry(const QModelIndex& index) const
{
    for (const Entry& entry : m_entries) {
        if (entry.index == index) {
            return &entry;
        }
    }
    return nullptr;
}

InstanceCardAnimator::Entry& InstanceCardAnimator::ensureEntry(const QModelIndex& index, bool currentlySelected)
{
    if (Entry* existing = findEntry(index)) {
        return *existing;
    }
    Entry entry;
    entry.index = index;
    entry.select = currentlySelected ? 1.0 : 0.0;
    entry.selectTarget = entry.select;
    m_entries.append(entry);
    return m_entries.last();
}
