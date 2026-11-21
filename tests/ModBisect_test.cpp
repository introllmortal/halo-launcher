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
#include <QSet>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <utility>

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

void writeFileBytes(const QString& path, const QByteArray& data)
{
    const QFileInfo fi(path);
    QVERIFY(FS::ensureFolderPathExists(fi.absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(data), data.size());
    file.close();
}

QByteArray readFileBytes(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

class BisectTestInstance final : public NullInstance {
   public:
    BisectTestInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
        : NullInstance(globalSettings, std::move(settings), rootDir)
    {
    }
    QString modsRoot() const override { return FS::PathCombine(gameRoot(), QStringLiteral("mods")); }
};

void runConvergentBisectCase(const QString& culpritMod, bool keepCulpritDisabledAtEnd)
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
    const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("instance"));
    QVERIFY(FS::ensureFolderPathExists(instDir));
    auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
    BisectTestInstance inst(global.get(), std::move(instSettings), instDir);

    const QStringList enabledMods = {
        QStringLiteral("mod01.jar"), QStringLiteral("mod02.jar"), QStringLiteral("mod03.jar"),
        QStringLiteral("mod04.jar"), QStringLiteral("mod05.jar"), QStringLiteral("mod06.jar"),
        QStringLiteral("mod07.jar"), QStringLiteral("mod08.jar"), QStringLiteral("mod09.jar"),
    };
    const QStringList preExistingDisabled = {
        QStringLiteral("pre_disabled_a.jar.disabled"),
        QStringLiteral("pre_disabled_b.jar.disabled"),
    };

    for (const QString& mod : enabledMods) {
        writeFileBytes(FS::PathCombine(inst.modsRoot(), mod), mod.toUtf8());
    }
    for (const QString& disabledMod : preExistingDisabled) {
        writeFileBytes(FS::PathCombine(inst.modsRoot(), disabledMod), disabledMod.toUtf8());
    }
    writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("readme.txt")), QByteArrayLiteral("keep me"));
    QVERIFY(FS::ensureFolderPathExists(FS::PathCombine(inst.modsRoot(), QStringLiteral("folder.jar"))));

    QString err;
    QVERIFY2(ModBisectSession::start(&inst, &err), qPrintable(err));
    QVERIFY(ModBisectSession::hasActiveSession(&inst));
    QVERIFY(QFileInfo::exists(FS::PathCombine(inst.instanceRoot(), QStringLiteral("halo-bisect.json"))));
    QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("halo-bisect.json"))));

    ModBisectSession::Status status = ModBisectSession::loadStatus(&inst);
    QVERIFY(status.active);
    QCOMPARE(status.stepNumber, 1);
    QCOMPARE(status.estimatedTotalSteps, 4);
    QCOMPARE(status.remainingCount, 9);
    QVERIFY(status.culpritMod.isEmpty());

    int iterations = 0;
    while (status.culpritMod.isEmpty()) {
        QVERIFY(iterations < 4);
        QVERIFY(status.stepNumber <= 4);

        const QSet<QString> activeSet(status.activeSubset.cbegin(), status.activeSubset.cend());
        for (const QString& mod : enabledMods) {
            const bool shouldBeEnabled = activeSet.contains(mod);
            QCOMPARE(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod)), shouldBeEnabled);
            QCOMPARE(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod + QStringLiteral(".disabled"))), !shouldBeEnabled);
        }
        for (const QString& disabledMod : preExistingDisabled) {
            QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), disabledMod)));
        }

        const bool bugPresentNow = QFileInfo::exists(FS::PathCombine(inst.modsRoot(), culpritMod));
        if (bugPresentNow) {
            QVERIFY2(ModBisectSession::recordBugPresent(&inst, &err), qPrintable(err));
        } else {
            QVERIFY2(ModBisectSession::recordBugAbsent(&inst, &err), qPrintable(err));
        }
        ++iterations;
        status = ModBisectSession::loadStatus(&inst);
        QVERIFY(status.active);
    }

    QVERIFY(status.stepNumber <= 4);
    QCOMPARE(status.culpritMod, culpritMod);
    QCOMPARE(status.remainingCount, 1);
    QCOMPARE(status.activeTestingCount, 0);

    // Non-culprit original mods are restored immediately upon convergence.
    for (const QString& mod : enabledMods) {
        if (mod == culpritMod) {
            QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod)));
            QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod + QStringLiteral(".disabled"))));
        } else {
            QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod)));
            QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod + QStringLiteral(".disabled"))));
        }
    }
    for (const QString& disabledMod : preExistingDisabled) {
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), disabledMod)));
    }

    if (keepCulpritDisabledAtEnd) {
        QVERIFY2(ModBisectSession::keepCulpritDisabledAndClear(&inst, &err), qPrintable(err));
    } else {
        QVERIFY2(ModBisectSession::restoreAllAndClear(&inst, &err), qPrintable(err));
    }

    QVERIFY(!ModBisectSession::hasActiveSession(&inst));
    QVERIFY(!ModBisectSession::loadStatus(&inst).active);
    QVERIFY(!QFileInfo::exists(ModBisectSession::sessionFilePath(&inst)));

    for (const QString& mod : enabledMods) {
        const bool expectEnabled = !keepCulpritDisabledAtEnd || (mod != culpritMod);
        QCOMPARE(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod)), expectEnabled);
        QCOMPARE(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod + QStringLiteral(".disabled"))), !expectEnabled);
    }
    for (const QString& disabledMod : preExistingDisabled) {
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), disabledMod)));
    }
    QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("readme.txt"))));
    QVERIFY(QDir(FS::PathCombine(inst.modsRoot(), QStringLiteral("folder.jar"))).exists());
}

}  // namespace

class ModBisectTest : public QObject {
    Q_OBJECT

   private slots:
    void binaryConvergenceNineModsLowerAndUpper()
    {
        runConvergentBisectCase(QStringLiteral("mod02.jar"), true);
        runConvergentBisectCase(QStringLiteral("mod09.jar"), false);
    }

    void cancelAndRestoreMidSession()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_cancel"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        BisectTestInstance inst(global.get(), std::move(instSettings), instDir);

        const QStringList mods = {
            QStringLiteral("a.jar"), QStringLiteral("b.jar"), QStringLiteral("c.jar"),
            QStringLiteral("d.jar"), QStringLiteral("e.jar"), QStringLiteral("f.jar"),
        };
        for (const QString& mod : mods) {
            writeFileBytes(FS::PathCombine(inst.modsRoot(), mod), QByteArrayLiteral("jar"));
        }
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("old_disabled.jar.disabled")), QByteArrayLiteral("stay-off"));

        QString err;
        QVERIFY2(ModBisectSession::start(&inst, &err), qPrintable(err));
        QVERIFY2(ModBisectSession::recordBugAbsent(&inst, &err), qPrintable(err));
        QCOMPARE(ModBisectSession::loadStatus(&inst).stepNumber, 2);

        QVERIFY2(ModBisectSession::cancelAndRestore(&inst, &err), qPrintable(err));
        QVERIFY(!ModBisectSession::hasActiveSession(&inst));
        QVERIFY(!QFileInfo::exists(ModBisectSession::sessionFilePath(&inst)));

        for (const QString& mod : mods) {
            QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod)));
            QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), mod + QStringLiteral(".disabled"))));
        }
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("old_disabled.jar.disabled"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("old_disabled.jar"))));
    }

    void runningInstanceGuards()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_running"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        BisectTestInstance inst(global.get(), std::move(instSettings), instDir);

        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("m1.jar")), QByteArrayLiteral("1"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("m2.jar")), QByteArrayLiteral("2"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("m3.jar")), QByteArrayLiteral("3"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("m4.jar")), QByteArrayLiteral("4"));

        QString err;
        inst.setRunning(true);
        QVERIFY(!ModBisectSession::start(&inst, &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(!ModBisectSession::hasActiveSession(&inst));

        inst.setRunning(false);
        QVERIFY2(ModBisectSession::start(&inst, &err), qPrintable(err));
        const ModBisectSession::Status beforeRunning = ModBisectSession::loadStatus(&inst);

        inst.setRunning(true);
        err.clear();
        QVERIFY(!ModBisectSession::recordBugPresent(&inst, &err));
        QVERIFY(!err.isEmpty());

        err.clear();
        QVERIFY(!ModBisectSession::recordBugAbsent(&inst, &err));
        QVERIFY(!err.isEmpty());

        err.clear();
        QVERIFY(!ModBisectSession::cancelAndRestore(&inst, &err));
        QVERIFY(!err.isEmpty());

        err.clear();
        QVERIFY(!ModBisectSession::restoreAllAndClear(&inst, &err));
        QVERIFY(!err.isEmpty());

        err.clear();
        QVERIFY(!ModBisectSession::keepCulpritDisabledAndClear(&inst, &err));
        QVERIFY(!err.isEmpty());

        const ModBisectSession::Status afterRunning = ModBisectSession::loadStatus(&inst);
        QCOMPARE(afterRunning.stepNumber, beforeRunning.stepNumber);
        QCOMPARE(afterRunning.candidates, beforeRunning.candidates);
        QCOMPARE(afterRunning.activeSubset, beforeRunning.activeSubset);
    }

    void persistenceAcrossReloads()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_persist"));
        QVERIFY(FS::ensureFolderPathExists(instDir));

        ModBisectSession::Status savedStatus;
        {
            auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
            BisectTestInstance inst1(global.get(), std::move(instSettings), instDir);
            writeFileBytes(FS::PathCombine(inst1.modsRoot(), QStringLiteral("m1.jar")), QByteArrayLiteral("1"));
            writeFileBytes(FS::PathCombine(inst1.modsRoot(), QStringLiteral("m2.jar")), QByteArrayLiteral("2"));
            writeFileBytes(FS::PathCombine(inst1.modsRoot(), QStringLiteral("m3.jar")), QByteArrayLiteral("3"));
            writeFileBytes(FS::PathCombine(inst1.modsRoot(), QStringLiteral("m4.jar")), QByteArrayLiteral("4"));

            QString err;
            QVERIFY2(ModBisectSession::start(&inst1, &err), qPrintable(err));
            QVERIFY2(ModBisectSession::recordBugAbsent(&inst1, &err), qPrintable(err));
            savedStatus = ModBisectSession::loadStatus(&inst1);
        }

        auto instSettings2 = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        BisectTestInstance inst2(global.get(), std::move(instSettings2), instDir);
        QVERIFY(ModBisectSession::hasActiveSession(&inst2));
        const ModBisectSession::Status reloaded = ModBisectSession::loadStatus(&inst2);
        QVERIFY(reloaded.active);
        QCOMPARE(reloaded.stepNumber, savedStatus.stepNumber);
        QCOMPARE(reloaded.estimatedTotalSteps, savedStatus.estimatedTotalSteps);
        QCOMPARE(reloaded.remainingCount, savedStatus.remainingCount);
        QCOMPARE(reloaded.activeTestingCount, savedStatus.activeTestingCount);
        QCOMPARE(reloaded.candidates, savedStatus.candidates);
        QCOMPARE(reloaded.activeSubset, savedStatus.activeSubset);

        QString err;
        QVERIFY2(ModBisectSession::recordBugPresent(&inst2, &err), qPrintable(err));
        const ModBisectSession::Status finalStatus = ModBisectSession::loadStatus(&inst2);
        QCOMPARE(finalStatus.culpritMod, QStringLiteral("m3.jar"));
    }

    void edgeCasesAndCollisionRollback()
    {
        QString err;
        QVERIFY(!ModBisectSession::hasActiveSession(nullptr));
        QVERIFY(!ModBisectSession::loadStatus(nullptr).active);
        QVERIFY(!ModBisectSession::start(nullptr, &err));
        QVERIFY(!ModBisectSession::recordBugPresent(nullptr, &err));
        QVERIFY(!ModBisectSession::recordBugAbsent(nullptr, &err));
        QVERIFY(!ModBisectSession::cancelAndRestore(nullptr, &err));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst_edge"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        BisectTestInstance inst(global.get(), std::move(instSettings), instDir);

        // Fewer than 2 enabled .jar files
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("only_one.jar")), QByteArrayLiteral("1"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("ignored.jar.disabled")), QByteArrayLiteral("off"));
        QVERIFY(!ModBisectSession::start(&inst, &err));
        QVERIFY(!ModBisectSession::hasActiveSession(&inst));

        // Collision with an existing .disabled file rolls back prior renames in start()
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("a.jar")), QByteArrayLiteral("a"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("b.jar")), QByteArrayLiteral("b"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("c.jar")), QByteArrayLiteral("c"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("z_collide.jar")), QByteArrayLiteral("new-z"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("z_collide.jar.disabled")), QByteArrayLiteral("old-z"));

        QVERIFY(!ModBisectSession::start(&inst, &err));
        QVERIFY(!ModBisectSession::hasActiveSession(&inst));
        QVERIFY(!QFileInfo::exists(ModBisectSession::sessionFilePath(&inst)));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("c.jar"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("c.jar.disabled"))));
        QCOMPARE(readFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("z_collide.jar"))), QByteArrayLiteral("new-z"));
        QCOMPARE(readFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("z_collide.jar.disabled"))), QByteArrayLiteral("old-z"));

        // Duplicate start() fails while session is active
        QVERIFY(QFile::remove(FS::PathCombine(inst.modsRoot(), QStringLiteral("z_collide.jar.disabled"))));
        QVERIFY2(ModBisectSession::start(&inst, &err), qPrintable(err));
        QVERIFY(!ModBisectSession::start(&inst, &err));
        QVERIFY2(ModBisectSession::cancelAndRestore(&inst, &err), qPrintable(err));
    }
};

QTEST_GUILESS_MAIN(ModBisectTest)

#include "ModBisect_test.moc"
