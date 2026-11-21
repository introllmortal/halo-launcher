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
#include "PackSnapshot.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStringList>
#include <algorithm>

#include "BaseInstance.h"
#include "FileSystem.h"

namespace PackSnapshot {

namespace {

QString modStem(const QString& name)
{
    const QString suffix = QStringLiteral(".disabled");
    return name.endsWith(suffix, Qt::CaseInsensitive) ? name.left(name.size() - suffix.size()) : name;
}

void sortMods(QList<ModEntry>& mods)
{
    std::sort(mods.begin(), mods.end(), [](const ModEntry& a, const ModEntry& b) {
        const int c = QString::compare(a.fileName, b.fileName, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a.fileName < b.fileName;
    });
}

int countKind(const QList<ModDiffEntry>& list, DiffKind kind)
{
    return static_cast<int>(std::count_if(list.cbegin(), list.cend(), [kind](const ModDiffEntry& e) { return e.kind == kind; }));
}

QHash<QString, ModEntry> indexMods(const QList<ModEntry>& list)
{
    QHash<QString, ModEntry> map;
    map.reserve(list.size());
    for (const ModEntry& mod : list) {
        if (mod.fileName.isEmpty()) {
            continue;
        }
        const QString key = modStem(mod.fileName).toLower();
        if (!map.contains(key) || (!map.value(key).enabled && mod.enabled)) {
            map.insert(key, mod);
        }
    }
    return map;
}

}  // namespace

int SnapshotDiff::addedCount() const
{
    return countKind(modChanges, DiffKind::Added);
}

int SnapshotDiff::removedCount() const
{
    return countKind(modChanges, DiffKind::Removed);
}

int SnapshotDiff::toggledCount() const
{
    return countKind(modChanges, DiffKind::Enabled) + countKind(modChanges, DiffKind::Disabled);
}

bool SnapshotDiff::isEmpty() const
{
    return modChanges.isEmpty();
}

QString SnapshotDiff::toMarkdown() const
{
    auto pick = [](const QString& val, const QString& fallback) { return val.isEmpty() ? fallback : val; };
    QStringList out{
        QStringLiteral("# Mod Diff: %1 -> %2").arg(pick(leftLabel, QStringLiteral("Left")), pick(rightLabel, QStringLiteral("Right"))),
    };
    if (!leftPackVersion.isEmpty() || !rightPackVersion.isEmpty()) {
        out << QStringLiteral("Pack version: %1 -> %2")
                   .arg(pick(leftPackVersion, QStringLiteral("-")), pick(rightPackVersion, QStringLiteral("-")));
    }
    out << QStringLiteral("Summary: +%1 added, -%2 removed, ~%3 toggled (%4 -> %5 mods)")
               .arg(addedCount())
               .arg(removedCount())
               .arg(toggledCount())
               .arg(totalLeftMods)
               .arg(totalRightMods);
    if (modChanges.isEmpty()) {
        return out.join(QLatin1Char('\n')) + QStringLiteral("\n\nNo mod changes.");
    }

    QStringList groups[4];
    for (const ModDiffEntry& e : modChanges) {
        const int idx = static_cast<int>(e.kind);
        if (idx < 0 || idx >= 4) {
            continue;
        }
        if (e.kind == DiffKind::Added) {
            groups[idx] << QStringLiteral("- `%1`").arg(e.rightFileName);
        } else if (e.kind == DiffKind::Removed) {
            groups[idx] << QStringLiteral("- `%1`").arg(e.leftFileName);
        } else {
            groups[idx] << QStringLiteral("- `%1` (`%2` -> `%3`)").arg(e.baseKey, e.leftFileName, e.rightFileName);
        }
    }

    static const QString kHeaders[4] = {
        QStringLiteral("Added"),
        QStringLiteral("Removed"),
        QStringLiteral("Enabled"),
        QStringLiteral("Disabled"),
    };
    for (int i = 0; i < 4; ++i) {
        if (!groups[i].isEmpty()) {
            out << QString() << QStringLiteral("## %1 (%2)").arg(kHeaders[i]).arg(groups[i].size()) << groups[i];
        }
    }
    return out.join(QLatin1Char('\n'));
}

QList<ModEntry> readModsManifest(const Info& snapshot)
{
    QList<ModEntry> mods;
    if (snapshot.dirPath.isEmpty()) {
        return mods;
    }
    QFile f(FS::PathCombine(snapshot.dirPath, QStringLiteral("mods-manifest.json")));
    if (!f.open(QIODevice::ReadOnly)) {
        return mods;
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return mods;
    }
    for (const QJsonValue& val : doc.array()) {
        if (!val.isObject()) {
            continue;
        }
        const QJsonObject obj = val.toObject();
        const QString name = obj.value(QStringLiteral("fileName")).toString();
        if (name.isEmpty()) {
            continue;
        }
        const bool enabled = obj.contains(QStringLiteral("enabled")) ? obj.value(QStringLiteral("enabled")).toBool(true)
                                                                     : !name.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive);
        mods.append({ name, qMax<qint64>(0, static_cast<qint64>(obj.value(QStringLiteral("size")).toDouble(0.0))), enabled });
    }
    sortMods(mods);
    return mods;
}

QList<ModEntry> scanCurrentMods(const BaseInstance* instance)
{
    QList<ModEntry> mods;
    const QString root = instance ? instance->modsRoot() : QString();
    if (root.isEmpty() || !QDir(root).exists()) {
        return mods;
    }
    for (const QFileInfo& fi : QDir(root).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        const QString name = fi.fileName();
        const bool isJar = name.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive);
        if (isJar || name.endsWith(QStringLiteral(".jar.disabled"), Qt::CaseInsensitive)) {
            mods.append({ name, fi.size(), isJar });
        }
    }
    sortMods(mods);
    return mods;
}

SnapshotDiff diffModLists(const QList<ModEntry>& left,
                          const QList<ModEntry>& right,
                          const QString& leftLabel,
                          const QString& rightLabel)
{
    SnapshotDiff diff;
    diff.leftLabel = leftLabel;
    diff.rightLabel = rightLabel;
    diff.totalLeftMods = static_cast<int>(left.size());
    diff.totalRightMods = static_cast<int>(right.size());

    const auto leftMap = indexMods(left);
    const auto rightMap = indexMods(right);

    for (auto it = leftMap.cbegin(); it != leftMap.cend(); ++it) {
        const auto rit = rightMap.constFind(it.key());
        if (rit == rightMap.cend()) {
            diff.modChanges.append({ DiffKind::Removed, modStem(it->fileName), it->fileName, QString(), it->size, 0 });
        } else if (it->enabled != rit->enabled) {
            diff.modChanges.append({ rit->enabled ? DiffKind::Enabled : DiffKind::Disabled, modStem(rit->fileName), it->fileName,
                                     rit->fileName, it->size, rit->size });
        }
    }
    for (auto it = rightMap.cbegin(); it != rightMap.cend(); ++it) {
        if (!leftMap.contains(it.key())) {
            diff.modChanges.append({ DiffKind::Added, modStem(it->fileName), QString(), it->fileName, 0, it->size });
        }
    }

    std::sort(diff.modChanges.begin(), diff.modChanges.end(), [](const ModDiffEntry& a, const ModDiffEntry& b) {
        if (a.kind != b.kind) {
            return static_cast<int>(a.kind) < static_cast<int>(b.kind);
        }
        const int c = QString::compare(a.baseKey, b.baseKey, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a.baseKey < b.baseKey;
    });
    return diff;
}

SnapshotDiff diffSnapshotToCurrent(const BaseInstance* instance, const Info& snapshot)
{
    const QString left = snapshot.id.isEmpty() ? QStringLiteral("Snapshot") : QStringLiteral("Snapshot %1").arg(snapshot.id);
    const QString right =
        (instance && !instance->name().isEmpty()) ? QStringLiteral("%1 (Current)").arg(instance->name()) : QStringLiteral("Current");
    SnapshotDiff diff = diffModLists(readModsManifest(snapshot), scanCurrentMods(instance), left, right);
    diff.leftPackVersion = snapshot.packVersion;
    diff.rightPackVersion = instance ? instance->getManagedPackVersionName() : QString();
    return diff;
}

SnapshotDiff diffInstances(const BaseInstance* left, const BaseInstance* right)
{
    auto label = [](const BaseInstance* inst, const QString& fallback) {
        return (inst && !inst->name().isEmpty()) ? inst->name() : fallback;
    };
    SnapshotDiff diff = diffModLists(scanCurrentMods(left), scanCurrentMods(right), label(left, QStringLiteral("Left Instance")),
                                     label(right, QStringLiteral("Right Instance")));
    diff.leftPackVersion = left ? left->getManagedPackVersionName() : QString();
    diff.rightPackVersion = right ? right->getManagedPackVersionName() : QString();
    return diff;
}

}  // namespace PackSnapshot
