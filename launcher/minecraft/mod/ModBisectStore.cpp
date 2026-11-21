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
#include "ModBisectStore.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QSaveFile>
#include <QSet>
#include <algorithm>

#include "FileSystem.h"

namespace ModBisectStore {

namespace {

constexpr int kSessionSchemaVersion = 1;

void setError(QString* errorOut, const QString& message)
{
    if (errorOut != nullptr) {
        *errorOut = message;
    }
}

QJsonArray toJsonArray(const QStringList& items)
{
    QJsonArray array;
    for (const QString& item : items) {
        array.append(item);
    }
    return array;
}

bool parseModArray(const QJsonObject& obj, const QString& key, QStringList& out)
{
    const QJsonValue val = obj.value(key);
    if (!val.isArray()) {
        return false;
    }
    const QJsonArray arr = val.toArray();
    out.clear();
    out.reserve(arr.size());
    for (const QJsonValue& elem : arr) {
        if (!elem.isString()) {
            return false;
        }
        const QString name = elem.toString();
        if (!isValidModFileName(name)) {
            return false;
        }
        out.append(name);
    }
    return true;
}

bool isSubsetOf(const QStringList& subset, const QSet<QString>& superset)
{
    return std::all_of(subset.cbegin(), subset.cend(), [&superset](const QString& item) { return superset.contains(item); });
}

}  // namespace

bool isValidModFileName(const QString& name)
{
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..")) {
        return false;
    }
    if (name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) {
        return false;
    }
    return name.endsWith(QLatin1String(".jar"), Qt::CaseInsensitive);
}

std::optional<SessionData> loadSessionData(const QString& sessionPath, QString* errorOut)
{
    if (sessionPath.isEmpty() || !QFileInfo::exists(sessionPath)) {
        setError(errorOut, QObject::tr("No active mod bisect session exists."));
        return std::nullopt;
    }

    QFile file(sessionPath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorOut, QObject::tr("Failed to open bisect session file."));
        return std::nullopt;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        setError(errorOut, QObject::tr("Bisect session file is malformed."));
        return std::nullopt;
    }

    const QJsonObject obj = doc.object();
    const QJsonValue versionVal = obj.value(QStringLiteral("version"));
    if (!versionVal.isDouble() || versionVal.toInt(-1) != kSessionSchemaVersion) {
        setError(errorOut, QObject::tr("Unsupported bisect session version."));
        return std::nullopt;
    }

    SessionData data;
    if (!parseModArray(obj, QStringLiteral("originalEnabledMods"), data.originalEnabledMods) || data.originalEnabledMods.isEmpty()) {
        setError(errorOut, QObject::tr("Bisect session has an invalid mod list."));
        return std::nullopt;
    }
    if (!parseModArray(obj, QStringLiteral("candidates"), data.candidates)) {
        setError(errorOut, QObject::tr("Bisect session has an invalid candidate list."));
        return std::nullopt;
    }
    if (!parseModArray(obj, QStringLiteral("activeSubset"), data.activeSubset)) {
        setError(errorOut, QObject::tr("Bisect session has an invalid active subset."));
        return std::nullopt;
    }

    const QJsonValue stepVal = obj.value(QStringLiteral("stepNumber"));
    const QJsonValue totalVal = obj.value(QStringLiteral("estimatedTotalSteps"));
    const QJsonValue culpritVal = obj.value(QStringLiteral("culpritMod"));
    if (!stepVal.isDouble() || !totalVal.isDouble() || !culpritVal.isString()) {
        setError(errorOut, QObject::tr("Bisect session metadata is invalid."));
        return std::nullopt;
    }

    data.stepNumber = stepVal.toInt(0);
    data.estimatedTotalSteps = totalVal.toInt(0);
    data.culpritMod = culpritVal.toString();

    if (data.stepNumber < 1 || data.estimatedTotalSteps < 1) {
        setError(errorOut, QObject::tr("Bisect session step counters are invalid."));
        return std::nullopt;
    }

    const QSet<QString> originalSet(data.originalEnabledMods.cbegin(), data.originalEnabledMods.cend());
    const QSet<QString> candidateSet(data.candidates.cbegin(), data.candidates.cend());
    if (!isSubsetOf(data.candidates, originalSet) || !isSubsetOf(data.activeSubset, originalSet)) {
        setError(errorOut, QObject::tr("Bisect session candidate sets are inconsistent."));
        return std::nullopt;
    }

    const QJsonValue depsVal = obj.value(QStringLiteral("dependencies"));
    if (depsVal.isObject()) {
        const QJsonObject depsObj = depsVal.toObject();
        for (auto it = depsObj.constBegin(); it != depsObj.constEnd(); ++it) {
            if (!originalSet.contains(it.key()) || !it.value().isArray()) {
                continue;
            }
            QStringList deps;
            for (const QJsonValue& elem : it.value().toArray()) {
                const QString depName = elem.toString();
                if (originalSet.contains(depName) && depName != it.key()) {
                    deps.append(depName);
                }
            }
            if (!deps.isEmpty()) {
                data.dependencyMap.insert(it.key(), deps);
            }
        }
    }

    if (data.culpritMod.isEmpty()) {
        const bool hasTestedCandidate = std::any_of(data.activeSubset.cbegin(), data.activeSubset.cend(),
                                                    [&candidateSet](const QString& item) { return candidateSet.contains(item); });
        if (data.candidates.isEmpty() || data.activeSubset.isEmpty() || !hasTestedCandidate) {
            setError(errorOut, QObject::tr("Bisect session has no active candidates to test."));
            return std::nullopt;
        }
    } else {
        if (!isValidModFileName(data.culpritMod) || data.candidates.size() != 1 || data.candidates.first() != data.culpritMod) {
            setError(errorOut, QObject::tr("Bisect session culprit state is inconsistent."));
            return std::nullopt;
        }
    }

    return data;
}

bool saveSessionData(const QString& sessionPath, const SessionData& data, QString* errorOut)
{
    if (!FS::ensureFilePathExists(sessionPath)) {
        setError(errorOut, QObject::tr("Failed to prepare directory for bisect session file."));
        return false;
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("version"), kSessionSchemaVersion);
    obj.insert(QStringLiteral("originalEnabledMods"), toJsonArray(data.originalEnabledMods));
    obj.insert(QStringLiteral("candidates"), toJsonArray(data.candidates));
    obj.insert(QStringLiteral("activeSubset"), toJsonArray(data.activeSubset));
    obj.insert(QStringLiteral("stepNumber"), data.stepNumber);
    obj.insert(QStringLiteral("estimatedTotalSteps"), data.estimatedTotalSteps);
    obj.insert(QStringLiteral("culpritMod"), data.culpritMod);
    if (!data.dependencyMap.isEmpty()) {
        QJsonObject depsObj;
        for (auto it = data.dependencyMap.constBegin(); it != data.dependencyMap.constEnd(); ++it) {
            depsObj.insert(it.key(), toJsonArray(it.value()));
        }
        obj.insert(QStringLiteral("dependencies"), depsObj);
    }

    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    QSaveFile out(sessionPath);
    if (!out.open(QIODevice::WriteOnly)) {
        setError(errorOut, QObject::tr("Failed to open bisect session file for writing."));
        return false;
    }
    if (out.write(payload) != payload.size()) {
        out.cancelWriting();
        setError(errorOut, QObject::tr("Failed to write bisect session file."));
        return false;
    }
    if (!out.commit()) {
        setError(errorOut, QObject::tr("Failed to commit bisect session file."));
        return false;
    }
    return true;
}

}  // namespace ModBisectStore
