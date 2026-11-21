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
#include "GameGraphicsAdvisor.h"

#include <QCoreApplication>
#include <QThread>

#include "HardwareInfo.h"
#include "diagnostics/GpuSanityChecker.h"

namespace GameGraphicsAdvisor {

HardwareProfile detectSystemHardware()
{
    HardwareProfile hw;
    hw.cpuName = HardwareInfo::cpuInfo().trimmed();
    hw.cpuThreads = qMax(1, QThread::idealThreadCount());
    hw.totalRamMiB = HardwareInfo::totalRamMiB();

    const auto adapters = GpuSanityChecker::detectAdapters();
    const GpuAdapterInfo* bestHardware = nullptr;
    for (const auto& adapter : adapters) {
        if (adapter.isSoftware) {
            continue;
        }
        if (!bestHardware) {
            bestHardware = &adapter;
            continue;
        }
        if (adapter.isDiscrete && !bestHardware->isDiscrete) {
            bestHardware = &adapter;
        } else if (adapter.isDiscrete == bestHardware->isDiscrete &&
                   adapter.dedicatedVideoMemory > bestHardware->dedicatedVideoMemory) {
            bestHardware = &adapter;
        }
    }

    if (bestHardware) {
        hw.gpuName = bestHardware->name.trimmed();
        hw.vramMiB = bestHardware->dedicatedVideoMemory / (1024ULL * 1024ULL);
        hw.hasDiscreteGpu = bestHardware->isDiscrete;
        hw.isSoftwareGpu = false;
    } else if (!adapters.empty()) {
        hw.gpuName = adapters.front().name.trimmed();
        hw.vramMiB = adapters.front().dedicatedVideoMemory / (1024ULL * 1024ULL);
        hw.hasDiscreteGpu = false;
        hw.isSoftwareGpu = adapters.front().isSoftware;
    } else {
        const QStringList fallbackGpus = HardwareInfo::gpuInfo();
        hw.gpuName = fallbackGpus.isEmpty() ? QString() : fallbackGpus.first().trimmed();
    }

    return hw;
}

QualityPreset recommendPreset(const HardwareProfile& hw)
{
    if (hw.isSoftwareGpu || hw.totalRamMiB < 6144 || hw.cpuThreads <= 2) {
        return QualityPreset::MaxFps;
    }
    if (!hw.hasDiscreteGpu) {
        if (hw.totalRamMiB >= 14336 && hw.cpuThreads >= 8) {
            return QualityPreset::Medium;
        }
        return QualityPreset::Low;
    }
    if (hw.vramMiB < 3584 || hw.totalRamMiB < 8192) {
        return QualityPreset::Low;
    }
    if (hw.vramMiB >= 10240 && hw.totalRamMiB >= 24576 && hw.cpuThreads >= 12) {
        return QualityPreset::Ultra;
    }
    if (hw.vramMiB >= 6144 && hw.totalRamMiB >= 14336 && hw.cpuThreads >= 8) {
        return QualityPreset::High;
    }
    return QualityPreset::Medium;
}

PresetValues presetValues(QualityPreset preset, int cpuThreads)
{
    PresetValues v;
    const int recommendedThreads = cpuThreads > 0 ? qBound(1, cpuThreads / 3, 8) : 0;

    switch (preset) {
        case QualityPreset::MaxFps:
            v.renderDistance = 6;
            v.simulationDistance = 5;
            v.entityDistancePercent = 50;
            v.graphicsMode = 0;
            v.renderClouds = QStringLiteral("false");
            v.particles = 2;
            v.smoothLighting = false;
            v.entityShadows = false;
            v.biomeBlendRadius = 0;
            v.mipmapLevels = 2;
            v.maxFps = 260;
            v.vsync = false;
            v.leavesQuality = QStringLiteral("FAST");
            v.weatherQuality = QStringLiteral("FAST");
            v.enableVignette = false;
            v.chunkBuilderThreads = recommendedThreads;
            v.chunkDeferMode = QStringLiteral("ALWAYS");
            v.useEntityCulling = true;
            v.useFogOcclusion = true;
            v.useBlockFaceCulling = true;
            v.animateOnlyVisibleTextures = true;
            v.cpuRenderAheadLimit = 2;
            break;
        case QualityPreset::Low:
            v.renderDistance = 8;
            v.simulationDistance = 6;
            v.entityDistancePercent = 75;
            v.graphicsMode = 0;
            v.renderClouds = QStringLiteral("fast");
            v.particles = 1;
            v.smoothLighting = true;
            v.entityShadows = false;
            v.biomeBlendRadius = 1;
            v.mipmapLevels = 3;
            v.maxFps = 120;
            v.vsync = false;
            v.leavesQuality = QStringLiteral("FAST");
            v.weatherQuality = QStringLiteral("FAST");
            v.enableVignette = false;
            v.chunkBuilderThreads = recommendedThreads;
            v.chunkDeferMode = QStringLiteral("ALWAYS");
            v.useEntityCulling = true;
            v.useFogOcclusion = true;
            v.useBlockFaceCulling = true;
            v.animateOnlyVisibleTextures = true;
            v.cpuRenderAheadLimit = 3;
            break;
        case QualityPreset::Medium:
            v.renderDistance = 12;
            v.simulationDistance = 8;
            v.entityDistancePercent = 100;
            v.graphicsMode = 1;
            v.renderClouds = QStringLiteral("fast");
            v.particles = 0;
            v.smoothLighting = true;
            v.entityShadows = true;
            v.biomeBlendRadius = 2;
            v.mipmapLevels = 4;
            v.maxFps = 120;
            v.vsync = true;
            v.leavesQuality = QStringLiteral("DEFAULT");
            v.weatherQuality = QStringLiteral("DEFAULT");
            v.enableVignette = true;
            v.chunkBuilderThreads = recommendedThreads;
            v.chunkDeferMode = QStringLiteral("ONE_FRAME");
            v.useEntityCulling = true;
            v.useFogOcclusion = true;
            v.useBlockFaceCulling = true;
            v.animateOnlyVisibleTextures = true;
            v.cpuRenderAheadLimit = 3;
            break;
        case QualityPreset::High:
            v.renderDistance = 16;
            v.simulationDistance = 10;
            v.entityDistancePercent = 125;
            v.graphicsMode = 1;
            v.renderClouds = QStringLiteral("true");
            v.particles = 0;
            v.smoothLighting = true;
            v.entityShadows = true;
            v.biomeBlendRadius = 3;
            v.mipmapLevels = 4;
            v.maxFps = 165;
            v.vsync = true;
            v.leavesQuality = QStringLiteral("FANCY");
            v.weatherQuality = QStringLiteral("FANCY");
            v.enableVignette = true;
            v.chunkBuilderThreads = recommendedThreads;
            v.chunkDeferMode = QStringLiteral("ONE_FRAME");
            v.useEntityCulling = true;
            v.useFogOcclusion = true;
            v.useBlockFaceCulling = true;
            v.animateOnlyVisibleTextures = true;
            v.cpuRenderAheadLimit = 3;
            break;
        case QualityPreset::Ultra:
            v.renderDistance = 24;
            v.simulationDistance = 12;
            v.entityDistancePercent = 200;
            v.graphicsMode = 2;
            v.renderClouds = QStringLiteral("true");
            v.particles = 0;
            v.smoothLighting = true;
            v.entityShadows = true;
            v.biomeBlendRadius = 5;
            v.mipmapLevels = 4;
            v.maxFps = 260;
            v.vsync = false;
            v.leavesQuality = QStringLiteral("FANCY");
            v.weatherQuality = QStringLiteral("FANCY");
            v.enableVignette = true;
            v.chunkBuilderThreads = recommendedThreads;
            v.chunkDeferMode = QStringLiteral("ZERO_FRAMES");
            v.useEntityCulling = true;
            v.useFogOcclusion = false;
            v.useBlockFaceCulling = true;
            v.animateOnlyVisibleTextures = false;
            v.cpuRenderAheadLimit = 4;
            break;
    }

    return v;
}

QString presetDisplayName(QualityPreset preset)
{
    switch (preset) {
        case QualityPreset::MaxFps:
            return QCoreApplication::translate("GameOptionsPage", "Max FPS");
        case QualityPreset::Low:
            return QCoreApplication::translate("GameOptionsPage", "Low");
        case QualityPreset::Medium:
            return QCoreApplication::translate("GameOptionsPage", "Medium");
        case QualityPreset::High:
            return QCoreApplication::translate("GameOptionsPage", "High");
        case QualityPreset::Ultra:
            return QCoreApplication::translate("GameOptionsPage", "Ultra");
    }
    return QString();
}

QString formatHardwareSummary(const HardwareProfile& hw)
{
    const double ramGb = static_cast<double>(hw.totalRamMiB) / 1024.0;
    const QString gpuText = hw.gpuName.isEmpty() ? QCoreApplication::translate("GameOptionsPage", "Unknown GPU") : hw.gpuName;
    if (hw.vramMiB > 0) {
        const double vramGb = static_cast<double>(hw.vramMiB) / 1024.0;
        return QCoreApplication::translate("GameOptionsPage", "%1 (%2 GB VRAM) • %3 GB RAM • %4 threads")
            .arg(gpuText)
            .arg(vramGb, 0, 'f', 1)
            .arg(ramGb, 0, 'f', 0)
            .arg(hw.cpuThreads);
    }
    return QCoreApplication::translate("GameOptionsPage", "%1 • %2 GB RAM • %3 threads")
        .arg(gpuText)
        .arg(ramGb, 0, 'f', 0)
        .arg(hw.cpuThreads);
}

}  // namespace GameGraphicsAdvisor
