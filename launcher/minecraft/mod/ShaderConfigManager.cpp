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
#include "ShaderConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>

#include "FileSystem.h"

namespace {
QString shaderTr(const char* text)
{
    return QCoreApplication::translate("ShaderConfigManager", text);
}

struct KeyValueLines {
    struct Line {
        bool isKv = false;
        QString key;
        QString value;
        QString raw;
    };
    QList<Line> lines;
    QHash<QString, int> index;

    bool load(const QString& path, QString* errorOut)
    {
        lines.clear();
        index.clear();
        QFile file(path);
        if (!file.exists()) {
            return true;
        }
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorOut) {
                *errorOut = shaderTr("Failed to read %1: %2").arg(path, file.errorString());
            }
            return false;
        }
        const QStringList rawLines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
        for (int i = 0; i < rawLines.size(); ++i) {
            QString line = rawLines.at(i);
            if (line.endsWith(QLatin1Char('\r'))) {
                line.chop(1);
            }
            if (i == rawLines.size() - 1 && line.isEmpty()) {
                continue;
            }
            const QString trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
                lines.append({ false, {}, {}, line });
                continue;
            }
            const int eq = line.indexOf(QLatin1Char('='));
            if (eq <= 0) {
                lines.append({ false, {}, {}, line });
                continue;
            }
            const QString key = line.left(eq).trimmed();
            const QString val = line.mid(eq + 1);
            if (index.contains(key)) {
                lines[index.value(key)].value = val;
            } else {
                index.insert(key, lines.size());
                lines.append({ true, key, val, {} });
            }
        }
        return true;
    }

    QString value(const QString& key, const QString& def = {}) const
    {
        const auto it = index.constFind(key);
        return it == index.constEnd() ? def : lines.at(it.value()).value;
    }

    void set(const QString& key, const QString& val)
    {
        const auto it = index.constFind(key);
        if (it != index.constEnd()) {
            lines[it.value()].value = val;
            return;
        }
        index.insert(key, lines.size());
        lines.append({ true, key, val, {} });
    }

    bool save(const QString& path, QString* errorOut) const
    {
        const QFileInfo info(path);
        if (info.isSymLink()) {
            if (errorOut) {
                *errorOut = shaderTr("Refusing to write through symlink: %1").arg(path);
            }
            return false;
        }
        QDir().mkpath(info.absolutePath());
        QByteArray out;
        for (const Line& l : lines) {
            if (l.isKv) {
                out.append(l.key.toUtf8());
                out.append('=');
                out.append(l.value.toUtf8());
            } else {
                out.append(l.raw.toUtf8());
            }
            out.append('\n');
        }
        QSaveFile saver(path);
        if (!saver.open(QIODevice::WriteOnly)) {
            if (errorOut) {
                *errorOut = shaderTr("Failed to open %1: %2").arg(path, saver.errorString());
            }
            return false;
        }
        if (saver.write(out) != out.size() || !saver.commit()) {
            if (errorOut) {
                *errorOut = shaderTr("Failed to save %1: %2").arg(path, saver.errorString());
            }
            return false;
        }
        return true;
    }
};

bool hasModFileMatching(const QString& dirPath, const QString& token)
{
    if (dirPath.isEmpty()) {
        return false;
    }
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return false;
    }
    const QStringList entries = dir.entryList(QDir::Files);
    for (const QString& name : entries) {
        if (!name.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive) &&
            name.contains(token, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QString sanitizePackName(const QString& packFileName)
{
    return QFileInfo(packFileName).fileName();
}
}  // namespace

ShaderConfigManager::Backend ShaderConfigManager::detectBackend(const QString& gameRoot, const QString& modsRoot)
{
    if (gameRoot.isEmpty()) {
        return Backend::Unknown;
    }
    if (QFileInfo::exists(FS::PathCombine(gameRoot, QStringLiteral("config"), QStringLiteral("iris.properties")))) {
        return Backend::Iris;
    }
    if (QFileInfo::exists(FS::PathCombine(gameRoot, QStringLiteral("config"), QStringLiteral("oculus.properties")))) {
        return Backend::Oculus;
    }
    if (QFileInfo::exists(FS::PathCombine(gameRoot, QStringLiteral("optionsshaders.txt")))) {
        return Backend::OptiFine;
    }

    const QString modsDir = modsRoot.isEmpty() ? FS::PathCombine(gameRoot, QStringLiteral("mods")) : modsRoot;
    if (hasModFileMatching(modsDir, QStringLiteral("iris"))) {
        return Backend::Iris;
    }
    if (hasModFileMatching(modsDir, QStringLiteral("oculus"))) {
        return Backend::Oculus;
    }
    if (hasModFileMatching(modsDir, QStringLiteral("optifine"))) {
        return Backend::OptiFine;
    }
    return Backend::Unknown;
}

QString ShaderConfigManager::backendDisplayName(Backend backend)
{
    switch (backend) {
        case Backend::Iris:
            return QStringLiteral("Iris");
        case Backend::Oculus:
            return QStringLiteral("Oculus");
        case Backend::OptiFine:
            return QStringLiteral("OptiFine");
        case Backend::Unknown:
            break;
    }
    return shaderTr("Not detected");
}

QString ShaderConfigManager::configPathForBackend(const QString& gameRoot, Backend backend)
{
    if (gameRoot.isEmpty()) {
        return {};
    }
    switch (backend) {
        case Backend::Iris:
            return FS::PathCombine(gameRoot, QStringLiteral("config"), QStringLiteral("iris.properties"));
        case Backend::Oculus:
            return FS::PathCombine(gameRoot, QStringLiteral("config"), QStringLiteral("oculus.properties"));
        case Backend::OptiFine:
            return FS::PathCombine(gameRoot, QStringLiteral("optionsshaders.txt"));
        case Backend::Unknown:
            break;
    }
    return {};
}

ShaderConfigManager::State ShaderConfigManager::loadState(const QString& gameRoot, const QString& modsRoot, QString* errorOut)
{
    State state;
    state.backend = detectBackend(gameRoot, modsRoot);
    state.configPath = configPathForBackend(gameRoot, state.backend);
    if (state.backend == Backend::Unknown || state.configPath.isEmpty()) {
        return state;
    }

    KeyValueLines kv;
    if (!kv.load(state.configPath, errorOut)) {
        return state;
    }

    if (state.backend == Backend::OptiFine) {
        const QString pack = kv.value(QStringLiteral("shaderPack")).trimmed();
        state.shadersEnabled = !pack.isEmpty() && pack != QStringLiteral("OFF");
        state.activePack = (pack == QStringLiteral("OFF") || pack == QStringLiteral("(internal)")) ? QString() : pack;
    } else {
        const QString enabled = kv.value(QStringLiteral("enableShaders"), QStringLiteral("true")).trimmed().toLower();
        state.shadersEnabled = (enabled != QStringLiteral("false"));
        state.activePack = kv.value(QStringLiteral("shaderPack")).trimmed();
    }
    return state;
}

bool ShaderConfigManager::saveState(const State& state, QString* errorOut)
{
    if (state.backend == Backend::Unknown || state.configPath.isEmpty()) {
        if (errorOut) {
            *errorOut = shaderTr("No shader mod (Iris, Oculus, or OptiFine) detected in this instance.");
        }
        return false;
    }

    KeyValueLines kv;
    if (!kv.load(state.configPath, errorOut)) {
        return false;
    }

    if (state.backend == Backend::OptiFine) {
        const QString packVal = (!state.shadersEnabled || state.activePack.isEmpty()) ? QStringLiteral("OFF") : state.activePack;
        kv.set(QStringLiteral("shaderPack"), packVal);
    } else {
        kv.set(QStringLiteral("enableShaders"), state.shadersEnabled ? QStringLiteral("true") : QStringLiteral("false"));
        kv.set(QStringLiteral("shaderPack"), state.activePack);
    }
    return kv.save(state.configPath, errorOut);
}

QString ShaderConfigManager::shaderProfilePath(const QString& shaderPacksDir, const QString& packFileName)
{
    const QString safeName = sanitizePackName(packFileName);
    if (shaderPacksDir.isEmpty() || safeName.isEmpty()) {
        return {};
    }
    return FS::PathCombine(shaderPacksDir, safeName + QStringLiteral(".txt"));
}

QList<ShaderConfigManager::ProfileEntry> ShaderConfigManager::loadShaderProfile(const QString& shaderPacksDir,
                                                                                  const QString& packFileName,
                                                                                  QString* errorOut)
{
    const QString path = shaderProfilePath(shaderPacksDir, packFileName);
    if (path.isEmpty()) {
        return {};
    }
    KeyValueLines kv;
    if (!kv.load(path, errorOut)) {
        return {};
    }
    QList<ProfileEntry> out;
    for (const auto& line : kv.lines) {
        if (line.isKv) {
            out.append({ line.key, line.value });
        }
    }
    return out;
}

bool ShaderConfigManager::saveShaderProfile(const QString& shaderPacksDir,
                                            const QString& packFileName,
                                            const QList<ProfileEntry>& entries,
                                            QString* errorOut)
{
    const QString path = shaderProfilePath(shaderPacksDir, packFileName);
    if (path.isEmpty()) {
        if (errorOut) {
            *errorOut = shaderTr("Shader pack path is empty.");
        }
        return false;
    }
    KeyValueLines kv;
    if (!kv.load(path, errorOut)) {
        return false;
    }
    for (const auto& entry : entries) {
        if (!entry.key.trimmed().isEmpty()) {
            kv.set(entry.key.trimmed(), entry.value);
        }
    }
    return kv.save(path, errorOut);
}

bool ShaderConfigManager::resetShaderProfile(const QString& shaderPacksDir, const QString& packFileName, QString* errorOut)
{
    const QString path = shaderProfilePath(shaderPacksDir, packFileName);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return true;
    }
    if (!QFile::remove(path)) {
        if (errorOut) {
            *errorOut = shaderTr("Failed to remove %1").arg(path);
        }
        return false;
    }
    return true;
}
