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

#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

#include "FileSystem.h"
#include "NullInstance.h"
#include "archive/ArchiveWriter.h"
#include "minecraft/mod/ModBisectGraph.h"
#include "minecraft/mod/ModBisectSession.h"
#include "settings/INISettingsObject.h"

namespace {

std::unique_ptr<INISettingsObject> makeGlobalSettings(const QString& path)
{
    auto settings = std::make_unique<INISettingsObject>(path);
    settings->registerSetting("ShowGameTime", true);
    settings->registerSetting("RecordGameTime", true);
    settings->registerSetting("PreLaunchCommand", "");
    settings->registerSetting("WrapperCommand", "");
    settings->registerSetting("PostExitCommand", "");
    settings->registerSetting("ShowConsole", false);
    settings->registerSetting("AutoCloseConsole", false);
    settings->registerSetting("ShowConsoleOnError", true);
    settings->registerSetting("EnableCrashAssistant", true);
    settings->registerSetting("LogPrePostOutput", true);
    settings->registerSetting("ConsoleMaxLines", 10000);
    settings->registerSetting("ConsoleOverflowStop", false);
    return settings;
}

class BisectDepTestInstance final : public NullInstance {
   public:
    BisectDepTestInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
        : NullInstance(globalSettings, std::move(settings), rootDir)
    {
    }
    QString modsRoot() const override { return FS::PathCombine(gameRoot(), QStringLiteral("mods")); }
};

void writeNeoForgeModJar(const QString& path, const QString& modId, const QStringList& requiredDeps)
{
    QString toml = QStringLiteral("modLoader=\"javafml\"\nloaderVersion=\"[4,)\"\n[[mods]]\nmodId=\"%1\"\nversion=\"1.0\"\n").arg(modId);
    for (const QString& dep : requiredDeps) {
        toml += QStringLiteral("[[dependencies.%1]]\nmodId=\"%2\"\ntype=\"required\"\nmandatory=true\n").arg(modId, dep);
    }
    QVERIFY(FS::ensureFolderPathExists(QFileInfo(path).absolutePath()));
    MMCZip::ArchiveWriter zip(path);
    QVERIFY(zip.open());
    QVERIFY(zip.addFile(QStringLiteral("META-INF/neoforge.mods.toml"), toml.toUtf8()));
    QVERIFY(zip.close());
}

}  // namespace

class ModBisectDependenciesTest : public QObject {
    Q_OBJECT

   private slots:
    void dependencyAwareBisectIsolatesAddonAndLibrary()
    {
        auto runRealJarScenario = [&](const QString& culprit) {
            QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());

            auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
            const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("instance"));
            QVERIFY(FS::ensureFolderPathExists(instDir));
            auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
            BisectDepTestInstance inst(global.get(), std::move(instSettings), instDir);

            writeNeoForgeModJar(FS::PathCombine(inst.modsRoot(), QStringLiteral("corelib.jar")), QStringLiteral("corelib"), {});
            writeNeoForgeModJar(FS::PathCombine(inst.modsRoot(), QStringLiteral("midlib.jar")), QStringLiteral("midlib"),
                                { QStringLiteral("corelib") });
            writeNeoForgeModJar(FS::PathCombine(inst.modsRoot(), QStringLiteral("bad_addon.jar")), QStringLiteral("bad_addon"),
                                { QStringLiteral("midlib") });
            writeNeoForgeModJar(FS::PathCombine(inst.modsRoot(), QStringLiteral("other_addon.jar")), QStringLiteral("other_addon"),
                                { QStringLiteral("corelib") });
            writeNeoForgeModJar(FS::PathCombine(inst.modsRoot(), QStringLiteral("standalone.jar")), QStringLiteral("standalone"), {});

            QString err;
            QVERIFY2(ModBisectSession::start(&inst, &err), qPrintable(err));
            ModBisectSession::Status status = ModBisectSession::loadStatus(&inst);

            int steps = 0;
            while (status.culpritMod.isEmpty()) {
                QVERIFY(steps++ < 6);

                const bool badAddonOn = QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("bad_addon.jar")));
                const bool midLibOn = QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("midlib.jar")));
                const bool coreLibOn = QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("corelib.jar")));
                const bool otherAddonOn = QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("other_addon.jar")));

                if (badAddonOn) {
                    QVERIFY(midLibOn);
                    QVERIFY(coreLibOn);
                }
                if (midLibOn || otherAddonOn) {
                    QVERIFY(coreLibOn);
                }

                const bool bugPresentNow = QFileInfo::exists(FS::PathCombine(inst.modsRoot(), culprit));
                if (bugPresentNow) {
                    QVERIFY2(ModBisectSession::recordBugPresent(&inst, &err), qPrintable(err));
                } else {
                    QVERIFY2(ModBisectSession::recordBugAbsent(&inst, &err), qPrintable(err));
                }
                status = ModBisectSession::loadStatus(&inst);
            }

            QCOMPARE(status.culpritMod, culprit);
            QVERIFY2(ModBisectSession::restoreAllAndClear(&inst, &err), qPrintable(err));
        };

        runRealJarScenario(QStringLiteral("bad_addon.jar"));
        runRealJarScenario(QStringLiteral("corelib.jar"));
    }

    void mutualDependencyCycleStillConverges()
    {
        {
            const QStringList all = { QStringLiteral("alpha.jar"), QStringLiteral("beta.jar") };
            QHash<QString, QStringList> graph;
            graph.insert(QStringLiteral("alpha.jar"), { QStringLiteral("beta.jar") });
            graph.insert(QStringLiteral("beta.jar"), { QStringLiteral("alpha.jar") });

            const QStringList active = ModBisectGraph::selectBisectStepSubset(all, all, graph);
            QCOMPARE(active.size(), 1);
        }

        {
            QTemporaryDir tempDir;
            QVERIFY(tempDir.isValid());
            auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
            const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_cycle3"));
            QVERIFY(FS::ensureFolderPathExists(instDir));
            auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
            BisectDepTestInstance inst(global.get(), std::move(instSettings), instDir);

            writeNeoForgeModJar(FS::PathCombine(inst.modsRoot(), QStringLiteral("A.jar")), QStringLiteral("moda"),
                                { QStringLiteral("modc") });
            writeNeoForgeModJar(FS::PathCombine(inst.modsRoot(), QStringLiteral("B.jar")), QStringLiteral("modb"),
                                { QStringLiteral("moda") });
            writeNeoForgeModJar(FS::PathCombine(inst.modsRoot(), QStringLiteral("C.jar")), QStringLiteral("modc"),
                                { QStringLiteral("modb") });

            QString err;
            QVERIFY2(ModBisectSession::start(&inst, &err), qPrintable(err));
            ModBisectSession::Status status = ModBisectSession::loadStatus(&inst);
            int steps = 0;
            while (status.culpritMod.isEmpty()) {
                QVERIFY(steps++ < 5);
                const bool bugPresentNow = QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("C.jar")));
                if (bugPresentNow) {
                    QVERIFY2(ModBisectSession::recordBugPresent(&inst, &err), qPrintable(err));
                } else {
                    QVERIFY2(ModBisectSession::recordBugAbsent(&inst, &err), qPrintable(err));
                }
                status = ModBisectSession::loadStatus(&inst);
            }
            QCOMPARE(status.culpritMod, QStringLiteral("C.jar"));
            QVERIFY2(ModBisectSession::restoreAllAndClear(&inst, &err), qPrintable(err));
        }
    }
};

QTEST_GUILESS_MAIN(ModBisectDependenciesTest)

#include "ModBisectDependencies_test.moc"
