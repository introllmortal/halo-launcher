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

#include <QTest>

#include "minecraft/GameGraphicsAdvisor.h"

class GameGraphicsAdvisorTest : public QObject {
    Q_OBJECT

   private slots:
    void graphicsAdvisorTiersAndPresets()
    {
        using namespace GameGraphicsAdvisor;

        HardwareProfile potato;
        potato.cpuThreads = 4;
        potato.totalRamMiB = 4096;
        potato.isSoftwareGpu = true;
        QCOMPARE(recommendPreset(potato), QualityPreset::MaxFps);

        HardwareProfile igpu;
        igpu.cpuThreads = 8;
        igpu.totalRamMiB = 8192;
        igpu.hasDiscreteGpu = false;
        igpu.vramMiB = 512;
        QCOMPARE(recommendPreset(igpu), QualityPreset::Low);

        HardwareProfile mid;
        mid.cpuThreads = 8;
        mid.totalRamMiB = 16384;
        mid.hasDiscreteGpu = true;
        mid.vramMiB = 4096;
        QCOMPARE(recommendPreset(mid), QualityPreset::Medium);

        HardwareProfile high;
        high.cpuThreads = 12;
        high.totalRamMiB = 16384;
        high.hasDiscreteGpu = true;
        high.vramMiB = 8192;
        QCOMPARE(recommendPreset(high), QualityPreset::High);

        HardwareProfile ultra;
        ultra.cpuThreads = 16;
        ultra.totalRamMiB = 32768;
        ultra.hasDiscreteGpu = true;
        ultra.vramMiB = 12288;
        QCOMPARE(recommendPreset(ultra), QualityPreset::Ultra);

        const PresetValues maxFpsVals = presetValues(QualityPreset::MaxFps, 6);
        QCOMPARE(maxFpsVals.renderDistance, 6);
        QCOMPARE(maxFpsVals.simulationDistance, 5);
        QCOMPARE(maxFpsVals.entityDistancePercent, 50);
        QCOMPARE(maxFpsVals.renderClouds, QStringLiteral("false"));
        QCOMPARE(maxFpsVals.smoothLighting, false);
        QCOMPARE(maxFpsVals.chunkBuilderThreads, 2);

        const PresetValues ultraVals = presetValues(QualityPreset::Ultra, 16);
        QCOMPARE(ultraVals.renderDistance, 24);
        QCOMPARE(ultraVals.simulationDistance, 12);
        QCOMPARE(ultraVals.entityDistancePercent, 200);
        QCOMPARE(ultraVals.graphicsMode, 2);
        QCOMPARE(ultraVals.renderClouds, QStringLiteral("true"));
        QCOMPARE(ultraVals.smoothLighting, true);
        QCOMPARE(ultraVals.chunkBuilderThreads, 5);
    }
};

QTEST_GUILESS_MAIN(GameGraphicsAdvisorTest)

#include "GameGraphicsAdvisor_test.moc"
