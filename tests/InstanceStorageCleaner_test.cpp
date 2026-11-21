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
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <memory>
#include <utility>

#include "FileSystem.h"
#include "MMCZip.h"
#include "NullInstance.h"
#include "diagnostics/InstanceStorageCleaner.h"
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

void writeBytes(const QString& path, const QByteArray& bytes)
{
    const QFileInfo fi(path);
    QVERIFY(FS::ensureFolderPathExists(fi.absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
    file.close();
}

QByteArray readBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

const InstanceStorageCleaner::CategoryStats* findCategory(const InstanceStorageCleaner::ScanReport& report,
                                                          InstanceStorageCleaner::Category category)
{
    for (const auto& stats : report.categories) {
        if (stats.category == category) {
            return &stats;
        }
    }
    return nullptr;
}

class StorageTestInstance final : public NullInstance {
   public:
    StorageTestInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
        : NullInstance(globalSettings, std::move(settings), rootDir)
    {
    }
    QString gameRoot() const override { return FS::PathCombine(instanceRoot(), QStringLiteral(".minecraft")); }
    QString modsRoot() const override { return FS::PathCombine(gameRoot(), QStringLiteral("mods")); }
};

}  // namespace

class InstanceStorageCleanerTest : public QObject {
    Q_OBJECT

   private slots:
    void scanAndCleanRespectsAllowlistAndPreservesUserData()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_clean"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        StorageTestInstance inst(global.get(), std::move(instSettings), instDir);

        const QString gameDir = inst.gameRoot();

        // Cleanable files
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("logs/latest.log")), QByteArray(120, 'L'));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("logs/old/2026-01-01.log.gz")), QByteArray(80, 'G'));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("crash-reports/crash-1.txt")), QByteArray(150, 'C'));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral(".cache/shader.bin")), QByteArray(200, 'H'));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("cache/temp.dat")), QByteArray(50, 'T'));
        writeBytes(FS::PathCombine(instDir, QStringLiteral(".tmp/download.part")), QByteArray(40, 'P'));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("mods/oldmod.jar.bak")), QByteArray(90, 'B'));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("mods/staging.halo_bak")), QByteArray(60, 'S'));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("config.halo_rollback_bak/leftover.toml")), QByteArray(70, 'R'));

        // 5 snapshots (keep 3 newest => 2 oldest excess)
        for (int i = 1; i <= 5; ++i) {
            QThread::msleep(5);
            const QString snapMeta = QStringLiteral(R"({"id":"snap-%1","createdAtUtc":"2026-01-0%1T00:00:00.000Z"})").arg(i);
            writeBytes(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-%1/snapshot.json").arg(i)), snapMeta.toUtf8());
            writeBytes(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-%1/data.bin").arg(i)), QByteArray(100, 'X'));
        }

        // Protected user data (must NEVER be modified or deleted)
        writeBytes(FS::PathCombine(instDir, QStringLiteral("instance.cfg")), QByteArrayLiteral("name=KeepMe\n"));
        writeBytes(FS::PathCombine(instDir, QStringLiteral("mmc-pack.json")), QByteArrayLiteral("{\"formatVersion\":1}"));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("options.txt")), QByteArrayLiteral("fov:75.0\n"));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("saves/MyWorld/level.dat")), QByteArrayLiteral("world-nbt-data"));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("mods/sodium.jar")), QByteArrayLiteral("sodium-jar-bytes"));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("mods/iris.jar.disabled")), QByteArrayLiteral("iris-jar-bytes"));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("config/sodium.json")), QByteArrayLiteral("{\"quality\":1}"));
        writeBytes(FS::PathCombine(gameDir, QStringLiteral("backups/simplebackups-world.zip")), QByteArrayLiteral("user-backup-zip"));

        const auto report = InstanceStorageCleaner::scanInstance(&inst, 3);
        QVERIFY(report.error.isEmpty());

        const auto* logsStats = findCategory(report, InstanceStorageCleaner::Category::Logs);
        const auto* crashStats = findCategory(report, InstanceStorageCleaner::Category::CrashReports);
        const auto* cacheStats = findCategory(report, InstanceStorageCleaner::Category::Cache);
        const auto* modBackupStats = findCategory(report, InstanceStorageCleaner::Category::ModBackups);
        const auto* snapStats = findCategory(report, InstanceStorageCleaner::Category::ExcessSnapshots);

        QVERIFY(logsStats && crashStats && cacheStats && modBackupStats && snapStats);
        QCOMPARE(logsStats->bytes, 200);
        QCOMPARE(logsStats->fileCount, 2);
        QCOMPARE(crashStats->bytes, 150);
        QCOMPARE(crashStats->fileCount, 1);
        QCOMPARE(cacheStats->bytes, 290);
        QCOMPARE(cacheStats->fileCount, 3);
        QCOMPARE(modBackupStats->bytes, 220);
        QCOMPARE(modBackupStats->fileCount, 3);

        const qint64 snap1Size = readBytes(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-1/snapshot.json"))).size() + 100;
        const qint64 snap2Size = readBytes(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-2/snapshot.json"))).size() + 100;
        QCOMPARE(snapStats->bytes, snap1Size + snap2Size);
        QCOMPARE(snapStats->fileCount, 4);

        const qint64 expectedCleanable = 200 + 150 + 290 + 220 + snap1Size + snap2Size;
        QCOMPARE(report.totalCleanableBytes, expectedCleanable);
        QVERIFY(report.totalInstanceBytes > report.totalCleanableBytes);

        QString cleanError;
        const QList<InstanceStorageCleaner::Category> allCategories = {
            InstanceStorageCleaner::Category::Logs,
            InstanceStorageCleaner::Category::CrashReports,
            InstanceStorageCleaner::Category::Cache,
            InstanceStorageCleaner::Category::ModBackups,
            InstanceStorageCleaner::Category::ExcessSnapshots,
        };
        const qint64 freed = InstanceStorageCleaner::cleanCategories(&inst, allCategories, 3, &cleanError);
        QVERIFY2(cleanError.isEmpty(), qPrintable(cleanError));
        QCOMPARE(freed, expectedCleanable);

        // Cleanable targets removed
        QVERIFY(!QFileInfo::exists(FS::PathCombine(gameDir, QStringLiteral("logs/latest.log"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(gameDir, QStringLiteral("logs/old/2026-01-01.log.gz"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(gameDir, QStringLiteral("crash-reports/crash-1.txt"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(gameDir, QStringLiteral(".cache"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(gameDir, QStringLiteral("cache"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral(".tmp"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(gameDir, QStringLiteral("mods/oldmod.jar.bak"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(gameDir, QStringLiteral("mods/staging.halo_bak"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(gameDir, QStringLiteral("config.halo_rollback_bak"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-1"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-2"))));

        // Newest 3 snapshots kept
        QVERIFY(QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-3/snapshot.json"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-4/snapshot.json"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-5/snapshot.json"))));

        // Protected files untouched
        QCOMPARE(readBytes(FS::PathCombine(instDir, QStringLiteral("instance.cfg"))), QByteArrayLiteral("name=KeepMe\n"));
        QCOMPARE(readBytes(FS::PathCombine(instDir, QStringLiteral("mmc-pack.json"))), QByteArrayLiteral("{\"formatVersion\":1}"));
        QCOMPARE(readBytes(FS::PathCombine(gameDir, QStringLiteral("options.txt"))), QByteArrayLiteral("fov:75.0\n"));
        QCOMPARE(readBytes(FS::PathCombine(gameDir, QStringLiteral("saves/MyWorld/level.dat"))), QByteArrayLiteral("world-nbt-data"));
        QCOMPARE(readBytes(FS::PathCombine(gameDir, QStringLiteral("mods/sodium.jar"))), QByteArrayLiteral("sodium-jar-bytes"));
        QCOMPARE(readBytes(FS::PathCombine(gameDir, QStringLiteral("mods/iris.jar.disabled"))), QByteArrayLiteral("iris-jar-bytes"));
        QCOMPARE(readBytes(FS::PathCombine(gameDir, QStringLiteral("config/sodium.json"))), QByteArrayLiteral("{\"quality\":1}"));
        QCOMPARE(readBytes(FS::PathCombine(gameDir, QStringLiteral("backups/simplebackups-world.zip"))), QByteArrayLiteral("user-backup-zip"));
    }

    void skipsExcessSnapshotsWhenBisectActive()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_bisect"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        StorageTestInstance inst(global.get(), std::move(instSettings), instDir);

        for (int i = 1; i <= 5; ++i) {
            writeBytes(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-%1/data.bin").arg(i)), QByteArray(64, 'S'));
        }
        writeBytes(FS::PathCombine(instDir, QStringLiteral("halo-bisect.json")), QByteArrayLiteral("{\"active\":true}"));

        const auto report = InstanceStorageCleaner::scanInstance(&inst, 3);
        const auto* snapStats = findCategory(report, InstanceStorageCleaner::Category::ExcessSnapshots);
        QVERIFY(snapStats != nullptr);
        QCOMPARE(snapStats->bytes, 0);
        QCOMPARE(snapStats->fileCount, 0);

        const qint64 freed = InstanceStorageCleaner::cleanCategories(&inst, { InstanceStorageCleaner::Category::ExcessSnapshots }, 3);
        QCOMPARE(freed, 0);
        QVERIFY(QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral("halo-snapshots/snap-1/data.bin"))));
    }

    void runningGuardBlocksCleanAndBackup()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_running"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        StorageTestInstance inst(global.get(), std::move(instSettings), instDir);

        const QString logPath = FS::PathCombine(inst.gameRoot(), QStringLiteral("logs/latest.log"));
        const QString worldDir = FS::PathCombine(inst.gameRoot(), QStringLiteral("saves/LiveWorld"));
        writeBytes(logPath, QByteArrayLiteral("running log"));
        writeBytes(FS::PathCombine(worldDir, QStringLiteral("level.dat")), QByteArrayLiteral("live-world"));

        inst.setRunning(true);

        QString cleanError;
        const qint64 freed = InstanceStorageCleaner::cleanCategories(&inst, { InstanceStorageCleaner::Category::Logs }, 3, &cleanError);
        QCOMPARE(freed, 0);
        QVERIFY(!cleanError.isEmpty());
        QVERIFY(QFileInfo::exists(logPath));

        QString backupError;
        const auto backupPath = InstanceStorageCleaner::createWorldZipBackup(&inst, worldDir, &backupError);
        QVERIFY(!backupPath.has_value());
        QVERIFY(!backupError.isEmpty());
        QVERIFY(!QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral("world-backups"))));

        inst.setRunning(false);
    }

    void worldZipBackupCreatesValidArchiveAndRejectsBadPaths()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_world"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        StorageTestInstance inst(global.get(), std::move(instSettings), instDir);

        const QString worldDir = FS::PathCombine(inst.gameRoot(), QStringLiteral("saves/Survival One!"));
        const QByteArray levelBytes = QByteArrayLiteral("fake-nbt-level-dat-payload");
        const QByteArray regionBytes = QByteArray(512, 'M');
        const QByteArray mapBytes = QByteArrayLiteral("map-data-0");

        writeBytes(FS::PathCombine(worldDir, QStringLiteral("level.dat")), levelBytes);
        writeBytes(FS::PathCombine(worldDir, QStringLiteral("region/r.0.0.mca")), regionBytes);
        writeBytes(FS::PathCombine(worldDir, QStringLiteral("data/map_0.dat")), mapBytes);

        // Reject missing world dir, null instance, and '..' traversal
        QString err;
        QVERIFY(!InstanceStorageCleaner::createWorldZipBackup(nullptr, worldDir, &err).has_value());
        QVERIFY(!err.isEmpty());

        QVERIFY(!InstanceStorageCleaner::createWorldZipBackup(&inst, FS::PathCombine(inst.gameRoot(), QStringLiteral("saves/MissingWorld")), &err)
                     .has_value());
        QVERIFY(!err.isEmpty());

        QVERIFY(!InstanceStorageCleaner::createWorldZipBackup(&inst, FS::PathCombine(worldDir, QStringLiteral("level.dat")), &err).has_value());
        QVERIFY(!err.isEmpty());

        const QString traversalPath = inst.gameRoot() + QStringLiteral("/saves/../saves/Survival One!");
        QVERIFY(!InstanceStorageCleaner::createWorldZipBackup(&inst, traversalPath, &err).has_value());
        QVERIFY(!err.isEmpty());

        // Create backup
        const auto zipPath = InstanceStorageCleaner::createWorldZipBackup(&inst, worldDir, &err);
        QVERIFY2(zipPath.has_value(), qPrintable(err));
        QVERIFY(err.isEmpty());
        QVERIFY(zipPath->endsWith(QStringLiteral(".zip")));
        QVERIFY(zipPath->startsWith(QDir::cleanPath(FS::PathCombine(instDir, QStringLiteral("world-backups")))));

        const QFileInfo zipInfo(*zipPath);
        QVERIFY(zipInfo.exists());
        QVERIFY(zipInfo.size() > 0);
        QVERIFY(!QFileInfo::exists(FS::PathCombine(instDir, QStringLiteral("world-backups")) + QLatin1Char('/') + zipInfo.completeBaseName() +
                                  QStringLiteral(".tmp.zip")));

        // Verify contents via MMCZip::extractDir into a fresh temp folder
        QTemporaryDir extractDir;
        QVERIFY(extractDir.isValid());
        const auto extracted = MMCZip::extractDir(*zipPath, extractDir.path());
        QVERIFY(extracted.has_value());
        QCOMPARE(readBytes(FS::PathCombine(extractDir.path(), QStringLiteral("level.dat"))), levelBytes);
        QCOMPARE(readBytes(FS::PathCombine(extractDir.path(), QStringLiteral("region/r.0.0.mca"))), regionBytes);
        QCOMPARE(readBytes(FS::PathCombine(extractDir.path(), QStringLiteral("data/map_0.dat"))), mapBytes);

        // Second backup handles filename collision cleanly
        const auto secondZipPath = InstanceStorageCleaner::createWorldZipBackup(&inst, worldDir, &err);
        QVERIFY2(secondZipPath.has_value(), qPrintable(err));
        QVERIFY(*secondZipPath != *zipPath);
        QVERIFY(QFileInfo(*secondZipPath).exists());
    }

    void formatBytesFormatsUnits()
    {
        QCOMPARE(InstanceStorageCleaner::formatBytes(0), QStringLiteral("0 B"));
        QCOMPARE(InstanceStorageCleaner::formatBytes(512), QStringLiteral("512 B"));
        QCOMPARE(InstanceStorageCleaner::formatBytes(1024), QStringLiteral("1.00 KiB"));
        QCOMPARE(InstanceStorageCleaner::formatBytes(1536), QStringLiteral("1.50 KiB"));
        QCOMPARE(InstanceStorageCleaner::formatBytes(1024LL * 1024LL), QStringLiteral("1.00 MiB"));
    }
};

QTEST_GUILESS_MAIN(InstanceStorageCleanerTest)

#include "InstanceStorageCleaner_test.moc"
