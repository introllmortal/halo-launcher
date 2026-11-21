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
#include "ModBisectGraph.h"

#include <QFileInfo>
#include <QQueue>
#include <QSet>
#include <algorithm>

#include "FileSystem.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/tasks/LocalModParseTask.h"

namespace ModBisectGraph {

namespace {

QSet<QString> transitiveClosure(const QStringList& seeds,
                                const QHash<QString, QStringList>& dependencyMap,
                                const QSet<QString>& allowedSet)
{
    QSet<QString> result;
    QQueue<QString> queue;
    for (const QString& seed : seeds) {
        if (allowedSet.contains(seed) && !result.contains(seed)) {
            result.insert(seed);
            queue.enqueue(seed);
        }
    }
    while (!queue.isEmpty()) {
        const QString current = queue.dequeue();
        const QStringList deps = dependencyMap.value(current);
        for (const QString& dep : deps) {
            if (allowedSet.contains(dep) && !result.contains(dep)) {
                result.insert(dep);
                queue.enqueue(dep);
            }
        }
    }
    return result;
}

QStringList orderByList(const QStringList& referenceOrder, const QSet<QString>& subset)
{
    QStringList ordered;
    ordered.reserve(subset.size());
    for (const QString& item : referenceOrder) {
        if (subset.contains(item)) {
            ordered.append(item);
        }
    }
    return ordered;
}

bool strictlyShrinksCandidates(const QSet<QString>& fullClosure, const QSet<QString>& candidateSet, qsizetype candidateCount)
{
    int testedCount = 0;
    for (const QString& item : fullClosure) {
        if (candidateSet.contains(item)) {
            ++testedCount;
        }
    }
    return testedCount >= 1 && testedCount < candidateCount;
}

}  // namespace

QHash<QString, QStringList> buildDependencyGraph(const QString& modsRoot, const QStringList& modFileNames)
{
    QHash<QString, QStringList> modIdToFiles;
    QHash<QString, QStringList> rawDepsByFile;

    for (const QString& fileName : modFileNames) {
        QFileInfo fi(FS::PathCombine(modsRoot, fileName));
        if (!fi.exists()) {
            fi = QFileInfo(FS::PathCombine(modsRoot, fileName + QStringLiteral(".disabled")));
        }
        if (!fi.exists() || !fi.isFile()) {
            continue;
        }

        Mod mod(fi);
        const bool ok = (mod.type() == ResourceType::ZIPFILE)
                            ? ModUtils::process(mod, ModUtils::ProcessingLevel::BasicInfoOnly)
                            : ModUtils::processZIP(mod, ModUtils::ProcessingLevel::BasicInfoOnly);
        if (!ok) {
            continue;
        }

        const QString id = mod.mod_id().trimmed().toLower();
        if (!id.isEmpty()) {
            modIdToFiles[id].append(fileName);
        }
        const QStringList deps = mod.dependencies();
        if (!deps.isEmpty()) {
            rawDepsByFile.insert(fileName, deps);
        }
    }

    QHash<QString, QStringList> graph;
    for (auto it = rawDepsByFile.cbegin(); it != rawDepsByFile.cend(); ++it) {
        const QString& fileName = it.key();
        QSet<QString> resolved;
        for (const QString& depId : it.value()) {
            const QStringList targets = modIdToFiles.value(depId.trimmed().toLower());
            for (const QString& targetFile : targets) {
                if (targetFile != fileName) {
                    resolved.insert(targetFile);
                }
            }
        }
        if (!resolved.isEmpty()) {
            QStringList sorted = resolved.values();
            std::sort(sorted.begin(), sorted.end(), [](const QString& a, const QString& b) {
                return a.compare(b, Qt::CaseInsensitive) < 0;
            });
            graph.insert(fileName, sorted);
        }
    }
    return graph;
}

QStringList selectBisectStepSubset(const QStringList& originalEnabledMods,
                                   const QStringList& candidates,
                                   const QHash<QString, QStringList>& dependencyMap)
{
    if (candidates.size() < 2) {
        return {};
    }

    const QSet<QString> originalSet(originalEnabledMods.cbegin(), originalEnabledMods.cend());
    const QSet<QString> candidateSet(candidates.cbegin(), candidates.cend());

    QStringList ordered = candidates;
    std::sort(ordered.begin(), ordered.end(), [&dependencyMap, &candidateSet](const QString& a, const QString& b) {
        auto inCandDegree = [&dependencyMap, &candidateSet](const QString& file) {
            int deg = 0;
            for (const QString& dep : dependencyMap.value(file)) {
                if (candidateSet.contains(dep)) {
                    ++deg;
                }
            }
            return deg;
        };
        const int degA = inCandDegree(a);
        const int degB = inCandDegree(b);
        if (degA != degB) {
            return degA < degB;
        }
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });

    const qsizetype splitIndex = qMax<qsizetype>(1, ordered.size() / 2);
    const QStringList halfA = ordered.first(splitIndex);
    const QStringList halfB = ordered.sliced(splitIndex);

    // Check shrinkage on the full transitive closure through originalSet so cleared dependencies
    // cannot walk back into candidateSet and accidentally cover every remaining candidate.
    const QSet<QString> fullClosureA = transitiveClosure(halfA, dependencyMap, originalSet);
    if (strictlyShrinksCandidates(fullClosureA, candidateSet, candidates.size())) {
        return orderByList(originalEnabledMods, fullClosureA);
    }

    const QSet<QString> fullClosureB = transitiveClosure(halfB, dependencyMap, originalSet);
    if (strictlyShrinksCandidates(fullClosureB, candidateSet, candidates.size())) {
        return orderByList(originalEnabledMods, fullClosureB);
    }

    // Remaining candidates form a transitive dependency cycle; test halfA directly so search converges.
    const QSet<QString> fallbackSet(halfA.cbegin(), halfA.cend());
    return orderByList(originalEnabledMods, fallbackSet);
}

}  // namespace ModBisectGraph
