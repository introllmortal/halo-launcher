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

/**
 * Reads and writes active shader pack selection (Iris, Oculus, OptiFine) and
 * per-shader profile overrides stored in shaderpacks/<packName>.txt.
 */
class ShaderConfigManager {
   public:
    enum class Backend {
        Unknown,
        Iris,
        Oculus,
        OptiFine,
    };

    struct State {
        Backend backend = Backend::Unknown;
        QString configPath;
        bool shadersEnabled = true;
        QString activePack;
    };

    struct ProfileEntry {
        QString key;
        QString value;
    };

    static Backend detectBackend(const QString& gameRoot, const QString& modsRoot = {});
    static QString backendDisplayName(Backend backend);
    static QString configPathForBackend(const QString& gameRoot, Backend backend);

    static State loadState(const QString& gameRoot, const QString& modsRoot = {}, QString* errorOut = nullptr);
    static bool saveState(const State& state, QString* errorOut = nullptr);

    static QString shaderProfilePath(const QString& shaderPacksDir, const QString& packFileName);
    static QList<ProfileEntry> loadShaderProfile(const QString& shaderPacksDir, const QString& packFileName, QString* errorOut = nullptr);
    static bool saveShaderProfile(const QString& shaderPacksDir,
                                  const QString& packFileName,
                                  const QList<ProfileEntry>& entries,
                                  QString* errorOut = nullptr);
    static bool resetShaderProfile(const QString& shaderPacksDir, const QString& packFileName, QString* errorOut = nullptr);
};
