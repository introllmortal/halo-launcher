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

#include <QHash>
#include <QString>
#include <QStringList>
#include <optional>

namespace ModBisectStore {

struct SessionData {
    QStringList originalEnabledMods;
    QStringList candidates;
    QStringList activeSubset;
    QHash<QString, QStringList> dependencyMap;
    int stepNumber = 0;
    int estimatedTotalSteps = 0;
    QString culpritMod;
};

bool isValidModFileName(const QString& name);
std::optional<SessionData> loadSessionData(const QString& sessionPath, QString* errorOut = nullptr);
bool saveSessionData(const QString& sessionPath, const SessionData& data, QString* errorOut = nullptr);

}  // namespace ModBisectStore
