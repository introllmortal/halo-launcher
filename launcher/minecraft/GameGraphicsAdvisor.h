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
#include <cstdint>

namespace GameGraphicsAdvisor {

enum class QualityPreset {
    MaxFps = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Ultra = 4,
};

struct HardwareProfile {
    QString cpuName;
    int cpuThreads = 4;
    uint64_t totalRamMiB = 8192;
    QString gpuName;
    uint64_t vramMiB = 0;
    bool hasDiscreteGpu = false;
    bool isSoftwareGpu = false;
};

struct PresetValues {
    int renderDistance = 12;
    int simulationDistance = 8;
    int entityDistancePercent = 100;
    int graphicsMode = 1;  // 0=Fast, 1=Fancy, 2=Fabulous!
    QString renderClouds = QStringLiteral("fast");
    int particles = 0;     // 0=All, 1=Decreased, 2=Minimal
    bool smoothLighting = true;
    bool entityShadows = true;
    int biomeBlendRadius = 2;
    int mipmapLevels = 4;
    int maxFps = 120;
    bool vsync = true;

    // Sodium / Embeddium calibration
    QString leavesQuality = QStringLiteral("DEFAULT");
    QString weatherQuality = QStringLiteral("DEFAULT");
    bool enableVignette = true;
    int chunkBuilderThreads = 0;
    QString chunkDeferMode = QStringLiteral("ONE_FRAME");
    bool useEntityCulling = true;
    bool useFogOcclusion = true;
    bool useBlockFaceCulling = true;
    bool animateOnlyVisibleTextures = true;
    int cpuRenderAheadLimit = 3;
};

HardwareProfile detectSystemHardware();
QualityPreset recommendPreset(const HardwareProfile& hw);
PresetValues presetValues(QualityPreset preset, int cpuThreads = 0);
QString presetDisplayName(QualityPreset preset);
QString formatHardwareSummary(const HardwareProfile& hw);

}  // namespace GameGraphicsAdvisor
