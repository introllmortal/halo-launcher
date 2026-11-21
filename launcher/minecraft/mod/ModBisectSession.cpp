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
#include "ModBisectSession.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSet>
#include <cmath>
#include <utility>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "minecraft/mod/ModBisectGraph.h"
#include "minecraft/mod/ModBisectStore.h"

namespace {

const QLatin1String kSessionFileName("halo-bisect.json");
const QLatin1String kDisabledSuffix(".disabled");

struct RenameOp {
    QString from;
    QString to;
};

void setError(QString* errorOut, const QString& message)
{
    if (errorOut != nullptr) {
        *errorOut = message;
    }
}

bool isInstanceEditable(const BaseInstance* instance, QString* errorOut)
{
    if (instance == nullptr) {
        setError(errorOut, QObject::tr("Instance is null."));
        return false;
    }
    if (instance->instanceRoot().isEmpty()) {
        setError(errorOut, QObject::tr("Instance root path is empty."));
        return false;
    }
    if (instance->isRunning()) {
        setError(errorOut, QObject::tr("Cannot modify mods while the instance is running."));
        return false;
    }
    if (instance->modsRoot().isEmpty()) {
        setError(errorOut, QObject::tr("Instance mods directory path is empty."));
        return false;
    }
    return true;
}

int computeEstimatedSteps(qsizetype candidateCount)
{
    if (candidateCount <= 1) {
        return 0;
    }
    return static_cast<int>(std::ceil(std::log2(static_cast<double>(candidateCount))));
}

void rollbackRenames(const QList<RenameOp>& performed)
{
    for (qsizetype i = performed.size() - 1; i >= 0; --i) {
        const RenameOp& op = performed.at(i);
        QFile::rename(op.to, op.from);
    }
}

bool applyModStates(const QString& modsRoot,
                    const QStringList& toEnable,
                    const QStringList& toDisable,
                    bool ignoreMissing,
                    QList<RenameOp>& performed,
                    QString* errorOut)
{
    auto abortWithRollback = [&](const QString& message) -> bool {
        rollbackRenames(performed);
        performed.clear();
        setError(errorOut, message);
        return false;
    };

    for (const QString& mod : toEnable) {
        const QString enabledPath = FS::PathCombine(modsRoot, mod);
        const QString disabledPath = enabledPath + kDisabledSuffix;

        if (QFileInfo::exists(enabledPath)) {
            continue;
        }
        if (!QFileInfo::exists(disabledPath)) {
            if (ignoreMissing) {
                continue;
            }
            return abortWithRollback(QObject::tr("Mod file '%1' is missing from the mods directory.").arg(mod));
        }
        if (!QFile::rename(disabledPath, enabledPath)) {
            return abortWithRollback(QObject::tr("Failed to enable mod '%1'.").arg(mod));
        }
        performed.append(RenameOp{ disabledPath, enabledPath });
    }

    for (const QString& mod : toDisable) {
        const QString enabledPath = FS::PathCombine(modsRoot, mod);
        const QString disabledPath = enabledPath + kDisabledSuffix;

        if (!QFileInfo::exists(enabledPath)) {
            if (QFileInfo::exists(disabledPath) || ignoreMissing) {
                continue;
            }
            return abortWithRollback(QObject::tr("Mod file '%1' is missing from the mods directory.").arg(mod));
        }
        if (QFileInfo::exists(disabledPath)) {
            return abortWithRollback(QObject::tr("Cannot disable '%1': '%1.disabled' already exists.").arg(mod));
        }
        if (!QFile::rename(enabledPath, disabledPath)) {
            return abortWithRollback(QObject::tr("Failed to disable mod '%1'.").arg(mod));
        }
        performed.append(RenameOp{ enabledPath, disabledPath });
    }

    return true;
}

bool recordStepOutcome(BaseInstance* instance, bool bugPresent, QString* errorOut)
{
    if (!isInstanceEditable(instance, errorOut)) {
        return false;
    }

    const QString sessionPath = ModBisectSession::sessionFilePath(instance);
    const std::optional<ModBisectStore::SessionData> current = ModBisectStore::loadSessionData(sessionPath, errorOut);
    if (!current.has_value()) {
        return false;
    }
    if (!current->culpritMod.isEmpty()) {
        setError(errorOut, QObject::tr("Bisect session has already identified a culprit mod."));
        return false;
    }
    if (current->activeSubset.isEmpty()) {
        setError(errorOut, QObject::tr("Bisect session has no active subset to evaluate."));
        return false;
    }

    const QSet<QString> activeSet(current->activeSubset.cbegin(), current->activeSubset.cend());
    QStringList nextCandidates;
    for (const QString& mod : current->candidates) {
        if (activeSet.contains(mod) == bugPresent) {
            nextCandidates.append(mod);
        }
    }

    if (nextCandidates.isEmpty()) {
        setError(errorOut, QObject::tr("Bisect step yielded no remaining candidate mods."));
        return false;
    }

    ModBisectStore::SessionData next = *current;
    QStringList toEnable;
    QStringList toDisable;

    if (nextCandidates.size() == 1) {
        next.culpritMod = nextCandidates.first();
        next.candidates = std::move(nextCandidates);
        next.activeSubset.clear();

        for (const QString& mod : next.originalEnabledMods) {
            if (mod == next.culpritMod) {
                toDisable.append(mod);
            } else {
                toEnable.append(mod);
            }
        }
    } else {
        next.stepNumber += 1;
        next.candidates = std::move(nextCandidates);
        next.activeSubset = ModBisectGraph::selectBisectStepSubset(next.originalEnabledMods, next.candidates, next.dependencyMap);
        if (next.activeSubset.isEmpty()) {
            next.activeSubset = next.candidates.first(next.candidates.size() / 2);
        }

        const QSet<QString> nextActiveSet(next.activeSubset.cbegin(), next.activeSubset.cend());
        for (const QString& mod : next.originalEnabledMods) {
            if (nextActiveSet.contains(mod)) {
                toEnable.append(mod);
            } else {
                toDisable.append(mod);
            }
        }
    }

    QList<RenameOp> performed;
    if (!applyModStates(instance->modsRoot(), toEnable, toDisable, false, performed, errorOut)) {
        return false;
    }
    if (!ModBisectStore::saveSessionData(sessionPath, next, errorOut)) {
        rollbackRenames(performed);
        return false;
    }
    return true;
}

bool clearSessionWithStates(BaseInstance* instance, bool keepCulpritDisabled, QString* errorOut)
{
    if (!isInstanceEditable(instance, errorOut)) {
        return false;
    }

    const QString sessionPath = ModBisectSession::sessionFilePath(instance);
    const std::optional<ModBisectStore::SessionData> current = ModBisectStore::loadSessionData(sessionPath, errorOut);
    if (!current.has_value()) {
        return false;
    }

    if (keepCulpritDisabled && current->culpritMod.isEmpty()) {
        setError(errorOut, QObject::tr("No culprit mod has been identified yet."));
        return false;
    }

    QStringList toEnable;
    QStringList toDisable;
    if (keepCulpritDisabled) {
        for (const QString& mod : current->originalEnabledMods) {
            if (mod == current->culpritMod) {
                toDisable.append(mod);
            } else {
                toEnable.append(mod);
            }
        }
    } else {
        toEnable = current->originalEnabledMods;
    }

    QList<RenameOp> performed;
    if (!applyModStates(instance->modsRoot(), toEnable, toDisable, true, performed, errorOut)) {
        return false;
    }

    if (!QFile::remove(sessionPath)) {
        rollbackRenames(performed);
        setError(errorOut, QObject::tr("Failed to remove bisect session file."));
        return false;
    }
    return true;
}

}  // namespace

QString ModBisectSession::sessionFilePath(const BaseInstance* instance)
{
    if (instance == nullptr || instance->instanceRoot().isEmpty()) {
        return {};
    }
    return FS::PathCombine(instance->instanceRoot(), kSessionFileName);
}

bool ModBisectSession::hasActiveSession(const BaseInstance* instance)
{
    return loadStatus(instance).active;
}

ModBisectSession::Status ModBisectSession::loadStatus(const BaseInstance* instance)
{
    Status status;
    const QString path = sessionFilePath(instance);
    const std::optional<ModBisectStore::SessionData> data = ModBisectStore::loadSessionData(path);
    if (!data.has_value()) {
        return status;
    }

    status.active = true;
    status.stepNumber = data->stepNumber;
    status.estimatedTotalSteps = data->estimatedTotalSteps;
    status.remainingCount = static_cast<int>(data->candidates.size());
    status.activeTestingCount = static_cast<int>(data->activeSubset.size());

    const QSet<QString> candidateSet(data->candidates.cbegin(), data->candidates.cend());
    int supportCount = 0;
    for (const QString& item : data->activeSubset) {
        if (!candidateSet.contains(item)) {
            ++supportCount;
        }
    }
    status.dependencySupportCount = supportCount;
    status.culpritMod = data->culpritMod;
    status.candidates = data->candidates;
    status.activeSubset = data->activeSubset;
    return status;
}

bool ModBisectSession::start(BaseInstance* instance, QString* errorOut)
{
    if (!isInstanceEditable(instance, errorOut)) {
        return false;
    }

    const QString sessionPath = sessionFilePath(instance);
    if (QFileInfo::exists(sessionPath)) {
        setError(errorOut, QObject::tr("A bisect session already exists for this instance."));
        return false;
    }

    const QString modsPath = instance->modsRoot();
    const QDir modsDir(modsPath);
    if (!modsDir.exists()) {
        setError(errorOut, QObject::tr("Mods directory does not exist."));
        return false;
    }

    const QFileInfoList entries = modsDir.entryInfoList(
        QStringList{ QStringLiteral("*.jar") },
        QDir::Files | QDir::NoDotAndDotDot,
        QDir::Name | QDir::IgnoreCase);

    QStringList originalEnabledMods;
    originalEnabledMods.reserve(entries.size());
    for (const QFileInfo& entry : entries) {
        const QString fileName = entry.fileName();
        if (ModBisectStore::isValidModFileName(fileName)) {
            originalEnabledMods.append(fileName);
        }
    }

    if (originalEnabledMods.size() < 2) {
        setError(errorOut, QObject::tr("At least 2 enabled .jar mods are required to start a bisect session."));
        return false;
    }

    ModBisectStore::SessionData session;
    session.originalEnabledMods = originalEnabledMods;
    session.candidates = originalEnabledMods;
    session.dependencyMap = ModBisectGraph::buildDependencyGraph(modsPath, originalEnabledMods);
    session.activeSubset = ModBisectGraph::selectBisectStepSubset(originalEnabledMods, session.candidates, session.dependencyMap);
    if (session.activeSubset.isEmpty()) {
        session.activeSubset = session.candidates.first(session.candidates.size() / 2);
    }
    session.stepNumber = 1;
    session.estimatedTotalSteps = computeEstimatedSteps(session.candidates.size());

    const QSet<QString> activeSet(session.activeSubset.cbegin(), session.activeSubset.cend());
    QStringList toDisable;
    for (const QString& mod : originalEnabledMods) {
        if (!activeSet.contains(mod)) {
            toDisable.append(mod);
        }
    }

    QList<RenameOp> performed;
    if (!applyModStates(modsPath, {}, toDisable, false, performed, errorOut)) {
        return false;
    }

    if (!ModBisectStore::saveSessionData(sessionPath, session, errorOut)) {
        rollbackRenames(performed);
        return false;
    }
    return true;
}

bool ModBisectSession::recordBugPresent(BaseInstance* instance, QString* errorOut)
{
    return recordStepOutcome(instance, true, errorOut);
}

bool ModBisectSession::recordBugAbsent(BaseInstance* instance, QString* errorOut)
{
    return recordStepOutcome(instance, false, errorOut);
}

bool ModBisectSession::cancelAndRestore(BaseInstance* instance, QString* errorOut)
{
    return clearSessionWithStates(instance, false, errorOut);
}

bool ModBisectSession::restoreAllAndClear(BaseInstance* instance, QString* errorOut)
{
    return clearSessionWithStates(instance, false, errorOut);
}

bool ModBisectSession::keepCulpritDisabledAndClear(BaseInstance* instance, QString* errorOut)
{
    return clearSessionWithStates(instance, true, errorOut);
}
