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
#include <cstdint>
#include <vector>

struct GpuAdapterInfo {
    QString name;
    uint32_t vendorId = 0;
    uint32_t deviceId = 0;
    uint64_t dedicatedVideoMemory = 0;
    bool isDiscrete = false;
    bool isIntegrated = false;
    bool isSoftware = false;
};

class GpuSanityChecker {
public:
    static bool isSupported();

    static std::vector<GpuAdapterInfo> detectAdapters();

    static bool isDualGpu();

    static QStringList discreteGpuNames();

    static QStringList integratedGpuNames();

    static QString validateAndCanonicalizeJavaPath(const QString& javaPath);

    static bool isHighPerformanceEnforced(const QString& javaPath);

    static bool setGpuPreference(const QString& javaPath);

    static bool removeGpuPreference(const QString& javaPath);
};
