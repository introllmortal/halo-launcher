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

#include <QString>
#include <QStringList>

class BaseInstance;

/**
 * Manages persistent binary-search ("bisect") sessions over an instance's top-level enabled mods.
 * State is stored outside mods/ in <instanceRoot>/halo-bisect.json so interrupted sessions survive restarts.
 */
class ModBisectSession {
   public:
    struct Status {
        bool active = false;
        int stepNumber = 0;
        int estimatedTotalSteps = 0;
        int remainingCount = 0;
        int activeTestingCount = 0;
        int dependencySupportCount = 0;
        QString culpritMod;
        QStringList candidates;
        QStringList activeSubset;
    };

    ModBisectSession() = delete;

    static QString sessionFilePath(const BaseInstance* instance);
    static bool hasActiveSession(const BaseInstance* instance);
    static Status loadStatus(const BaseInstance* instance);

    static bool start(BaseInstance* instance, QString* errorOut = nullptr);
    static bool recordBugPresent(BaseInstance* instance, QString* errorOut = nullptr);
    static bool recordBugAbsent(BaseInstance* instance, QString* errorOut = nullptr);
    static bool cancelAndRestore(BaseInstance* instance, QString* errorOut = nullptr);
    static bool restoreAllAndClear(BaseInstance* instance, QString* errorOut = nullptr);
    static bool keepCulpritDisabledAndClear(BaseInstance* instance, QString* errorOut = nullptr);
};
