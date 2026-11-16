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
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <memory>
#include <utility>

#include "FileSystem.h"
#include "NullInstance.h"
#include "settings/INISettingsObject.h"
#include "ui/pages/instance/PackSnapshot.h"

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

void writeFileBytes(const QString& path, const QByteArray& data)
{
    const QFileInfo fi(path);
    QVERIFY(FS::ensureFolderPathExists(fi.absolutePath()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(data), data.size());
    f.close();
}

QByteArray readFileBytes(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    return f.readAll();
}

class SnapshotTestInstance final : public NullInstance {
   public:
    SnapshotTestInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
        : NullInstance(globalSettings, std::move(settings), rootDir)
    {
    }
    QString modsRoot() const override { return FS::PathCombine(gameRoot(), QStringLiteral("mods")); }
};
}  // namespace

class PackSnapshotTest : public QObject {
    Q_OBJECT

   private slots:
    void createAndListLightweightSnapshot()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst1"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        SnapshotTestInstance inst(global.get(), std::move(instSettings), instDir);
        inst.setName(QStringLiteral("Snapshot Pack"));
        inst.setManagedPack(QStringLiteral("modrinth"), QStringLiteral("pack-1"), QStringLiteral("Pack"), QStringLiteral("ver-1"),
                            QStringLiteral("1.0.0"));

        writeFileBytes(FS::PathCombine(instDir, QStringLiteral("mmc-pack.json")), QByteArrayLiteral("{\"version\":1}"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("options.txt")), QByteArrayLiteral("fov:70.0\n"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/sub/test.toml")), QByteArrayLiteral("enabled = true\n"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("sodium.jar")), QByteArrayLiteral("dummy-jar-1"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("iris.jar.disabled")), QByteArrayLiteral("dummy-jar-2"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("notes.txt")), QByteArrayLiteral("ignore"));

        const auto created = PackSnapshot::createSnapshot(&inst, QStringLiteral("1.1.0"));
        QVERIFY(created.has_value());
        QCOMPARE(created->packVersion, QStringLiteral("1.0.0"));
        QCOMPARE(created->targetVersion, QStringLiteral("1.1.0"));
        QCOMPARE(created->modCount, 2);

        QVERIFY(PackSnapshot::hasSnapshot(&inst));
        const auto listed = PackSnapshot::listSnapshots(&inst);
        QCOMPARE(listed.size(), 1);
        QCOMPARE(listed.first().id, created->id);

        // Verify JAR files are NOT duplicated into snapshot directory
        QVERIFY(!QDir(FS::PathCombine(created->dirPath, QStringLiteral("mods"))).exists());
        const QByteArray manifestBytes = readFileBytes(FS::PathCombine(created->dirPath, QStringLiteral("mods-manifest.json")));
        const QJsonArray manifestArr = QJsonDocument::fromJson(manifestBytes).array();
        QCOMPARE(manifestArr.size(), 2);
    }

    void prunesOldSnapshotsAboveLimit()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst2"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        SnapshotTestInstance inst(global.get(), std::move(instSettings), instDir);

        for (int i = 1; i <= 5; ++i) {
            QThread::msleep(3);
            const auto snap = PackSnapshot::createSnapshot(&inst, QStringLiteral("v%1").arg(i));
            QVERIFY(snap.has_value());
        }

        const auto listed = PackSnapshot::listSnapshots(&inst);
        QCOMPARE(listed.size(), PackSnapshot::kMaxSnapshotsPerInstance);
        QCOMPARE(listed.at(0).targetVersion, QStringLiteral("v5"));
        QCOMPARE(listed.at(1).targetVersion, QStringLiteral("v4"));
        QCOMPARE(listed.at(2).targetVersion, QStringLiteral("v3"));
    }

    void restoreLatestRestoresConfigsAndRefusesWhenRunning()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst3"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        SnapshotTestInstance inst(global.get(), std::move(instSettings), instDir);
        inst.setManagedPack(QStringLiteral("modrinth"), QStringLiteral("pack-3"), QStringLiteral("Pack"), QStringLiteral("ver-1"),
                            QStringLiteral("1.0.0"));

        writeFileBytes(FS::PathCombine(instDir, QStringLiteral("mmc-pack.json")), QByteArrayLiteral("{\"state\":\"before\"}"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("options.txt")), QByteArrayLiteral("fov:70.0\n"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/mod.toml")), QByteArrayLiteral("mode = \"old\"\n"));

        const auto snap = PackSnapshot::createSnapshot(&inst, QStringLiteral("2.0.0"));
        QVERIFY(snap.has_value());

        // Simulate an update overwriting configs and adding a new config file
        inst.setManagedPack(QStringLiteral("modrinth"), QStringLiteral("pack-3"), QStringLiteral("Pack"), QStringLiteral("ver-2"),
                            QStringLiteral("2.0.0"));
        inst.saveNow();
        writeFileBytes(FS::PathCombine(instDir, QStringLiteral("mmc-pack.json")), QByteArrayLiteral("{\"state\":\"after\"}"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("options.txt")), QByteArrayLiteral("fov:100.0\n"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/mod.toml")), QByteArrayLiteral("mode = \"broken\"\n"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/new_extra.toml")), QByteArrayLiteral("junk = 1\n"));

        // Running instances cannot be rolled back
        inst.setRunning(true);
        QVERIFY(!PackSnapshot::restoreLatestSnapshot(&inst));
        QCOMPARE(readFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("options.txt"))), QByteArrayLiteral("fov:100.0\n"));
        inst.setRunning(false);

        QVERIFY(PackSnapshot::restoreLatestSnapshot(&inst));
        QCOMPARE(inst.getManagedPackVersionName(), QStringLiteral("1.0.0"));
        QCOMPARE(readFileBytes(FS::PathCombine(instDir, QStringLiteral("mmc-pack.json"))), QByteArrayLiteral("{\"state\":\"before\"}"));
        QCOMPARE(readFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("options.txt"))), QByteArrayLiteral("fov:70.0\n"));
        QCOMPARE(readFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/mod.toml"))), QByteArrayLiteral("mode = \"old\"\n"));
        QVERIFY(!QFile::exists(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/new_extra.toml"))));
        QVERIFY(!QDir(FS::PathCombine(inst.gameRoot(), QStringLiteral("config.halo_rollback_bak"))).exists());
    }

    void ignoresPartialConfigWhenHasConfigIsFalse()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst4"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        SnapshotTestInstance inst(global.get(), std::move(instSettings), instDir);

        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("options.txt")), QByteArrayLiteral("fov:70.0\n"));
        // Create snapshot when config/ did not exist yet (hasConfig == false)
        const auto snap = PackSnapshot::createSnapshot(&inst, QStringLiteral("1.1.0"));
        QVERIFY(snap.has_value());
        QVERIFY(!snap->hasConfig);

        // Plant a partial config folder inside snapshot dir while snapshot.json records hasConfig=false
        writeFileBytes(FS::PathCombine(snap->dirPath, QStringLiteral("config/broken.toml")), QByteArrayLiteral("broken = true\n"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/live.toml")), QByteArrayLiteral("valid = true\n"));

        QVERIFY(PackSnapshot::restoreSnapshot(&inst, *snap));
        QVERIFY(QFile::exists(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/live.toml"))));
        QVERIFY(!QFile::exists(FS::PathCombine(inst.gameRoot(), QStringLiteral("config/broken.toml"))));
    }
};

QTEST_GUILESS_MAIN(PackSnapshotTest)

#include "PackSnapshot_test.moc"
