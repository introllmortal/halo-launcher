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

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "FileSystem.h"
#include "minecraft/GameOptionsFile.h"
#include "minecraft/mod/GraphicsModConfigManager.h"
#include "minecraft/mod/ResourcePackOrderModel.h"
#include "minecraft/mod/ShaderConfigManager.h"

class GameConfigManagersTest : public QObject {
    Q_OBJECT

   private slots:
    void gameOptionsRoundTripPreservesModKeys()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString optPath = tempDir.filePath(QStringLiteral("options.txt"));

        {
            QFile f(optPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(
                "renderDistance:12\n"
                "fov:0.5\n"
                "fullscreen:true\n"
                "key_key.create.rotate:key.keyboard.r\n"
                "resourcePacks:[\"vanilla\",\"mod_resources\",\"file/PackA.zip\"]\n");
        }

        GameOptionsFile opts;
        QString err;
        QVERIFY2(opts.loadFromFile(optPath, &err), qPrintable(err));
        QCOMPARE(opts.intValue(QStringLiteral("renderDistance")), 12);
        QCOMPARE(opts.boolValue(QStringLiteral("fullscreen")), true);
        QCOMPARE(opts.rawValue(QStringLiteral("key_key.create.rotate")), QStringLiteral("key.keyboard.r"));

        opts.setIntValue(QStringLiteral("renderDistance"), 16);
        opts.setBoolValue(QStringLiteral("enableVsync"), false);
        QVERIFY2(opts.saveToFile(optPath, true, &err), qPrintable(err));
        QVERIFY(QFile::exists(optPath + QStringLiteral(".bak")));

        GameOptionsFile reloaded;
        QVERIFY(reloaded.loadFromFile(optPath));
        QCOMPARE(reloaded.intValue(QStringLiteral("renderDistance")), 16);
        QCOMPARE(reloaded.boolValue(QStringLiteral("enableVsync")), false);
        QCOMPARE(reloaded.rawValue(QStringLiteral("key_key.create.rotate")), QStringLiteral("key.keyboard.r"));
        QCOMPARE(reloaded.stringListValue(QStringLiteral("resourcePacks")).size(), 3);
    }

    void resourcePackOrderProtectsSystemLayers()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString optPath = tempDir.filePath(QStringLiteral("options.txt"));
        const QString rpDir = tempDir.filePath(QStringLiteral("resourcepacks"));
        QVERIFY(QDir().mkpath(rpDir));

        for (const QString& zip : { QStringLiteral("Alpha.zip"), QStringLiteral("Beta.zip") }) {
            QFile z(FS::PathCombine(rpDir, zip));
            QVERIFY(z.open(QIODevice::WriteOnly));
            z.write("PK");
        }

        {
            QFile f(optPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("resourcePacks:[\"vanilla\",\"mod_resources\",\"file/Alpha.zip\"]\n");
        }

        ResourcePackOrderModel model;
        QString err;
        QVERIFY2(model.load(optPath, rpDir, &err), qPrintable(err));
        QCOMPARE(model.count(), 4);
        // Index 0 = highest priority (last in options.txt)
        QCOMPARE(model.entries().at(0).id, QStringLiteral("file/Alpha.zip"));
        QVERIFY(model.entries().at(1).isSystem);
        QVERIFY(model.entries().at(2).isSystem);
        // Beta.zip is on disk but disabled -> appended at bottom
        QCOMPARE(model.entries().at(3).id, QStringLiteral("file/Beta.zip"));
        QVERIFY(!model.entries().at(3).enabled);

        // System layers cannot be disabled
        QVERIFY(!model.setEnabled(1, false));
        // Enable Beta -> moves to top
        QVERIFY(model.setEnabled(3, true));
        QCOMPARE(model.entries().at(0).id, QStringLiteral("file/Beta.zip"));
        QVERIFY(model.save(optPath, &err));

        GameOptionsFile saved;
        QVERIFY(saved.loadFromFile(optPath));
        const QStringList list = saved.stringListValue(QStringLiteral("resourcePacks"));
        QCOMPARE(list, QStringList({ QStringLiteral("vanilla"), QStringLiteral("mod_resources"), QStringLiteral("file/Alpha.zip"),
                                     QStringLiteral("file/Beta.zip") }));
    }

    void resourcePackOrderPreservesPrefixAndSuffixSystemAnchorsAndDisplayNames()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString optPath = tempDir.filePath(QStringLiteral("options.txt"));
        const QString rpDir = tempDir.filePath(QStringLiteral("resourcepacks"));
        QVERIFY(QDir().mkpath(rpDir));

        for (const QString& zip : { QStringLiteral("Alpha v1.zip"), QStringLiteral("Beta v2.zip"), QStringLiteral("Gamma.zip") }) {
            QFile z(FS::PathCombine(rpDir, zip));
            QVERIFY(z.open(QIODevice::WriteOnly));
            z.write("PK");
        }

        {
            QFile f(optPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(
                "resourcePacks:[\"vanilla\",\"mod_resources\",\"file/Alpha v1.zip\",\"file/Beta "
                "v2.zip\",\"mdg_engine_sounds\",\"bookofdragons_genetics_core\"]\n");
        }

        const QHash<QString, QString> displayNames = {
            { QStringLiteral("Alpha v1.zip"), QStringLiteral("Alpha Friendly") },
            { QStringLiteral("Beta v2.zip"), QStringLiteral("Beta Friendly") },
            { QStringLiteral("Gamma.zip"), QStringLiteral("Gamma Friendly") },
        };

        ResourcePackOrderModel model;
        QString err;
        QVERIFY2(model.load(optPath, rpDir, &err, displayNames), qPrintable(err));
        // Reversed load order: [bookofdragons(sys), mdg(sys), Beta(user), Alpha(user), mod_resources(sys), vanilla(sys), Gamma(off)]
        QCOMPARE(model.count(), 7);
        QCOMPARE(model.entries().at(0).id, QStringLiteral("bookofdragons_genetics_core"));
        QVERIFY(model.entries().at(0).isSystem);
        QCOMPARE(model.entries().at(1).id, QStringLiteral("mdg_engine_sounds"));
        QVERIFY(model.entries().at(1).isSystem);
        QCOMPARE(model.entries().at(2).id, QStringLiteral("file/Beta v2.zip"));
        QCOMPARE(model.entries().at(2).displayName, QStringLiteral("Beta Friendly"));
        QCOMPARE(model.entries().at(3).id, QStringLiteral("file/Alpha v1.zip"));
        QCOMPARE(model.entries().at(3).displayName, QStringLiteral("Alpha Friendly"));
        QCOMPARE(model.entries().at(4).id, QStringLiteral("mod_resources"));
        QCOMPARE(model.entries().at(5).id, QStringLiteral("vanilla"));
        QCOMPARE(model.entries().at(6).id, QStringLiteral("file/Gamma.zip"));
        QCOMPARE(model.entries().at(6).displayName, QStringLiteral("Gamma Friendly"));
        QVERIFY(!model.entries().at(6).enabled);

        // Moving enabled user pack down swaps with Alpha while system suffix/prefix anchors stay fixed.
        QVERIFY(model.moveVisibleDown(2, false));
        QCOMPARE(model.entries().at(2).id, QStringLiteral("file/Alpha v1.zip"));
        QCOMPARE(model.entries().at(3).id, QStringLiteral("file/Beta v2.zip"));
        QVERIFY(!model.moveVisibleDown(3, false));  // Cannot move across vanilla / into disabled Gamma

        // Enabling Gamma puts it at top of user packs (index 2), below suffix system anchors.
        QVERIFY(model.setEnabled(6, true));
        QCOMPARE(model.entries().at(0).id, QStringLiteral("bookofdragons_genetics_core"));
        QCOMPARE(model.entries().at(1).id, QStringLiteral("mdg_engine_sounds"));
        QCOMPARE(model.entries().at(2).id, QStringLiteral("file/Gamma.zip"));

        // Reordering visible user packs preserves all suffix & prefix system anchors.
        QVERIFY(model.reorderVisibleIds(
            { QStringLiteral("file/Beta v2.zip"), QStringLiteral("file/Gamma.zip"), QStringLiteral("file/Alpha v1.zip") }, false));
        QVERIFY(model.save(optPath, &err));

        GameOptionsFile saved;
        QVERIFY(saved.loadFromFile(optPath));
        QCOMPARE(saved.stringListValue(QStringLiteral("resourcePacks")),
                 QStringList({ QStringLiteral("vanilla"), QStringLiteral("mod_resources"), QStringLiteral("file/Alpha v1.zip"),
                               QStringLiteral("file/Gamma.zip"), QStringLiteral("file/Beta v2.zip"),
                               QStringLiteral("mdg_engine_sounds"), QStringLiteral("bookofdragons_genetics_core") }));
    }

    void resourcePackOrderEnableAndPartitionEdgeCases()
    {
        // Case A: Missing options.txt with a disabled zip on disk -> enabling places it ABOVE vanilla.
        {
            QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            const QString optPath = tempDir.filePath(QStringLiteral("options.txt"));
            const QString rpDir = tempDir.filePath(QStringLiteral("resourcepacks"));
            QVERIFY(QDir().mkpath(rpDir));
            QFile z(FS::PathCombine(rpDir, QStringLiteral("Alpha.zip")));
            QVERIFY(z.open(QIODevice::WriteOnly));
            z.write("PK");

            ResourcePackOrderModel model;
            QString err;
            QVERIFY2(model.load(optPath, rpDir, &err), qPrintable(err));
            QCOMPARE(model.count(), 2);
            QCOMPARE(model.entries().at(0).id, QStringLiteral("vanilla"));
            QCOMPARE(model.entries().at(1).id, QStringLiteral("file/Alpha.zip"));
            QVERIFY(!model.entries().at(1).enabled);

            QVERIFY(model.setEnabled(1, true));
            QCOMPARE(model.entries().at(0).id, QStringLiteral("file/Alpha.zip"));
            QCOMPARE(model.entries().at(1).id, QStringLiteral("vanilla"));
            QVERIFY(model.save(optPath, &err));

            GameOptionsFile saved;
            QVERIFY(saved.loadFromFile(optPath));
            QCOMPARE(saved.stringListValue(QStringLiteral("resourcePacks")),
                     QStringList({ QStringLiteral("vanilla"), QStringLiteral("file/Alpha.zip") }));
        }

        // Case B: System-only options.txt + disabled packs -> setEnabled & setEnabledByFileName place ABOVE vanilla/mod_resources.
        {
            QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            const QString optPath = tempDir.filePath(QStringLiteral("options.txt"));
            const QString rpDir = tempDir.filePath(QStringLiteral("resourcepacks"));
            QVERIFY(QDir().mkpath(rpDir));
            QFile z(FS::PathCombine(rpDir, QStringLiteral("Dormant.zip")));
            QVERIFY(z.open(QIODevice::WriteOnly));
            z.write("PK");

            {
                QFile f(optPath);
                QVERIFY(f.open(QIODevice::WriteOnly));
                f.write("resourcePacks:[\"vanilla\",\"mod_resources\"]\n");
            }

            ResourcePackOrderModel model;
            QString err;
            QVERIFY2(model.load(optPath, rpDir, &err), qPrintable(err));
            QVERIFY(model.setEnabled(2, true));
            QCOMPARE(model.entries().at(0).id, QStringLiteral("file/Dormant.zip"));
            QVERIFY(model.setEnabledByFileName(QStringLiteral("NewPack.zip"), true));
            QCOMPARE(model.entries().at(0).id, QStringLiteral("file/NewPack.zip"));
            QCOMPARE(model.entries().at(1).id, QStringLiteral("file/Dormant.zip"));
            QCOMPARE(model.entries().at(2).id, QStringLiteral("mod_resources"));
            QCOMPARE(model.entries().at(3).id, QStringLiteral("vanilla"));
        }

        // Case C: Suffix anchor + disable/re-enable, moveVisibleUp/Down (both modes), stable_partition, and missing check.
        {
            QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            const QString optPath = tempDir.filePath(QStringLiteral("options.txt"));
            const QString rpDir = tempDir.filePath(QStringLiteral("resourcepacks"));
            const QString emptyRpDir = tempDir.filePath(QStringLiteral("empty_rp"));
            QVERIFY(QDir().mkpath(rpDir));
            QVERIFY(QDir().mkpath(emptyRpDir));
            for (const QString& zip : { QStringLiteral("First.zip"), QStringLiteral("Second.zip"), QStringLiteral("Off.zip") }) {
                QFile z(FS::PathCombine(rpDir, zip));
                QVERIFY(z.open(QIODevice::WriteOnly));
                z.write("PK");
            }
            {
                QFile f(optPath);
                QVERIFY(f.open(QIODevice::WriteOnly));
                f.write("resourcePacks:[\"vanilla\",\"mod_resources\",\"file/First.zip\",\"mdg_engine_sounds\"]\n");
            }

            ResourcePackOrderModel missingCheck;
            QString err;
            QVERIFY2(missingCheck.load(optPath, emptyRpDir, &err), qPrintable(err));
            QCOMPARE(missingCheck.entries().at(1).id, QStringLiteral("file/First.zip"));
            QVERIFY(!missingCheck.entries().at(1).existsOnDisk);

            ResourcePackOrderModel model;
            QVERIFY2(model.load(optPath, rpDir, &err), qPrintable(err));
            QVERIFY(model.setEnabled(1, false));
            const int firstIdx = model.count() - 1;
            QCOMPARE(model.entries().at(firstIdx).id, QStringLiteral("file/First.zip"));
            QVERIFY(!model.entries().at(firstIdx).enabled);
            QCOMPARE(model.visibleIds(false),
                     QStringList({ QStringLiteral("file/Off.zip"), QStringLiteral("file/Second.zip"), QStringLiteral("file/First.zip") }));
            QVERIFY(model.setEnabled(firstIdx, true));
            QCOMPARE(model.entries().at(0).id, QStringLiteral("mdg_engine_sounds"));
            QCOMPARE(model.entries().at(1).id, QStringLiteral("file/First.zip"));
            QCOMPARE(model.entries().at(2).id, QStringLiteral("mod_resources"));

            QVERIFY(model.setEnabled(model.count() - 1, true));
            QVERIFY(model.moveVisibleUp(2, false));
            QCOMPARE(model.entries().at(1).id, QStringLiteral("file/First.zip"));
            QCOMPARE(model.entries().at(2).id, QStringLiteral("file/Second.zip"));

            // Disabled Off.zip at index 5 cannot move up/down even when includeSystem == true
            QVERIFY(!model.moveVisibleUp(5, true));
            QVERIFY(!model.moveVisibleDown(4, true));  // vanilla cannot swap with disabled Off.zip
            QVERIFY(model.reorderVisibleIds(
                { QStringLiteral("file/Off.zip"), QStringLiteral("file/Second.zip"), QStringLiteral("file/First.zip") }, false));
            QCOMPARE(model.visibleIds(false),
                     QStringList({ QStringLiteral("file/Second.zip"), QStringLiteral("file/First.zip"), QStringLiteral("file/Off.zip") }));

            // With includeSystem == true, dragging Off.zip to top also snaps it back to tail
            QVERIFY(model.reorderVisibleIds({ QStringLiteral("file/Off.zip"), QStringLiteral("mdg_engine_sounds"),
                                              QStringLiteral("file/Second.zip"), QStringLiteral("file/First.zip"),
                                              QStringLiteral("mod_resources"), QStringLiteral("vanilla") },
                                            true));
            QCOMPARE(model.entries().last().id, QStringLiteral("file/Off.zip"));
            QVERIFY(!model.entries().last().enabled);
        }
    }

    void shaderConfigSupportsIrisAndProfiles()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString gameRoot = tempDir.path();
        const QString cfgDir = FS::PathCombine(gameRoot, QStringLiteral("config"));
        const QString spDir = FS::PathCombine(gameRoot, QStringLiteral("shaderpacks"));
        QVERIFY(QDir().mkpath(cfgDir));
        QVERIFY(QDir().mkpath(spDir));

        {
            QFile iris(FS::PathCombine(cfgDir, QStringLiteral("iris.properties")));
            QVERIFY(iris.open(QIODevice::WriteOnly));
            iris.write("# Iris config\nenableShaders=false\nshaderPack=\n");
        }

        auto state = ShaderConfigManager::loadState(gameRoot);
        QCOMPARE(state.backend, ShaderConfigManager::Backend::Iris);
        QVERIFY(!state.shadersEnabled);
        QVERIFY(state.activePack.isEmpty());

        state.shadersEnabled = true;
        state.activePack = QStringLiteral("ComplementaryReimagined.zip");
        QString err;
        QVERIFY2(ShaderConfigManager::saveState(state, &err), qPrintable(err));

        const auto reloaded = ShaderConfigManager::loadState(gameRoot);
        QVERIFY(reloaded.shadersEnabled);
        QCOMPARE(reloaded.activePack, QStringLiteral("ComplementaryReimagined.zip"));

        QList<ShaderConfigManager::ProfileEntry> prof = {
            { QStringLiteral("SHADOW_QUALITY"), QStringLiteral("2") },
            { QStringLiteral("BLOOM"), QStringLiteral("false") },
        };
        QVERIFY(ShaderConfigManager::saveShaderProfile(spDir, state.activePack, prof, &err));
        const auto readProf = ShaderConfigManager::loadShaderProfile(spDir, state.activePack);
        QCOMPARE(readProf.size(), 2);
        QCOMPARE(readProf.at(0).key, QStringLiteral("SHADOW_QUALITY"));
        QCOMPARE(readProf.at(0).value, QStringLiteral("2"));
        QVERIFY(ShaderConfigManager::resetShaderProfile(spDir, state.activePack));
        QVERIFY(ShaderConfigManager::loadShaderProfile(spDir, state.activePack).isEmpty());
    }

    void graphicsModConfigPreservesUntouchedSections()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString gameRoot = tempDir.path();
        const QString cfgDir = FS::PathCombine(gameRoot, QStringLiteral("config"));
        QVERIFY(QDir().mkpath(cfgDir));

        const QString sodiumPath = FS::PathCombine(cfgDir, QStringLiteral("sodium-options.json"));
        {
            QFile f(sodiumPath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(
                "{\n"
                "  \"quality\": { \"leaves_quality\": \"DEFAULT\", \"enable_vignette\": true },\n"
                "  \"performance\": { \"chunk_builder_threads\": 4 },\n"
                "  \"notifications\": { \"has_seen_donation_prompt\": true }\n"
                "}\n");
        }

        const auto found = GraphicsModConfigManager::discoverConfigs(gameRoot);
        QCOMPARE(found.size(), 1);
        QVERIFY(found.at(0).isSodiumFamily);

        auto entries = GraphicsModConfigManager::loadEntries(sodiumPath, GraphicsModConfigManager::Format::Json);
        bool updated = false;
        for (auto& e : entries) {
            if (e.keyPath == QStringLiteral("performance.chunk_builder_threads")) {
                e.value = 6;
                updated = true;
            }
        }
        QVERIFY(updated);
        QVERIFY(GraphicsModConfigManager::saveEntries(sodiumPath, GraphicsModConfigManager::Format::Json, entries));

        const auto after = GraphicsModConfigManager::loadEntries(sodiumPath, GraphicsModConfigManager::Format::Json);
        bool foundThreads = false;
        bool foundNotif = false;
        for (const auto& e : after) {
            if (e.keyPath == QStringLiteral("performance.chunk_builder_threads")) {
                QCOMPARE(e.value.toInt(), 6);
                foundThreads = true;
            } else if (e.keyPath == QStringLiteral("notifications.has_seen_donation_prompt")) {
                QCOMPARE(e.value.toBool(), true);
                foundNotif = true;
            }
        }
        QVERIFY(foundThreads);
        QVERIFY(foundNotif);
    }
};

QTEST_GUILESS_MAIN(GameConfigManagersTest)

#include "GameConfigManagers_test.moc"
