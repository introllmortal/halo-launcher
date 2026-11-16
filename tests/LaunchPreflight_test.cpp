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
#include <QTimer>
#include <memory>
#include <utility>

#include "NullInstance.h"
#include "diagnostics/LaunchPreflightCheck.h"
#include "java/JavaInstall.h"
#include "java/JavaInstallList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"
#include "tasks/Task.h"

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

void writeDummyFile(const QString& path, const QByteArray& content = QByteArrayLiteral("jar"))
{
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(content);
    f.close();
}
}  // namespace

class LaunchPreflightTest : public QObject {
    Q_OBJECT

   private slots:
    void requiredJavaVersions()
    {
        QCOMPARE(LaunchPreflightCheck::requiredJavaMajorForMinecraft(QStringLiteral("1.12.2")), 8);
        QCOMPARE(LaunchPreflightCheck::requiredJavaMajorForMinecraft(QStringLiteral("1.16.5")), 8);
        QCOMPARE(LaunchPreflightCheck::requiredJavaMajorForMinecraft(QStringLiteral("1.17.1")), 16);
        QCOMPARE(LaunchPreflightCheck::requiredJavaMajorForMinecraft(QStringLiteral("1.18.2")), 17);
        QCOMPARE(LaunchPreflightCheck::requiredJavaMajorForMinecraft(QStringLiteral("1.20.4")), 17);
        QCOMPARE(LaunchPreflightCheck::requiredJavaMajorForMinecraft(QStringLiteral("1.20.5")), 21);
        QCOMPARE(LaunchPreflightCheck::requiredJavaMajorForMinecraft(QStringLiteral("1.21.1")), 21);
        QCOMPARE(LaunchPreflightCheck::requiredJavaMajorForMinecraft(QStringLiteral("")), 0);
    }

    void javaCompatibilityAndSelection()
    {
        auto j8 = std::make_shared<JavaInstall>(QStringLiteral("8.0.402"), QStringLiteral("amd64"), QStringLiteral("/opt/j8/bin/java"));
        j8->is_64bit = true;
        auto j17_32 = std::make_shared<JavaInstall>(QStringLiteral("17.0.10"), QStringLiteral("i386"), QStringLiteral("/opt/j17-x86/bin/java"));
        j17_32->is_64bit = false;
        auto j17 = std::make_shared<JavaInstall>(QStringLiteral("17.0.10"), QStringLiteral("amd64"), QStringLiteral("/opt/j17/bin/java"));
        j17->is_64bit = true;
        auto j21 = std::make_shared<JavaInstall>(QStringLiteral("21.0.5"), QStringLiteral("amd64"), QStringLiteral("/opt/j21/bin/java"));
        j21->is_64bit = true;

        JavaInstallList list;
        list.updateListData({ j8, j17_32, j17, j21 });

        auto bestFor17 = LaunchPreflightCheck::findBestInstalledJava(&list, 17, false);
        QVERIFY(bestFor17 != nullptr);
        QCOMPARE(bestFor17->id.major(), 17);
        QVERIFY(bestFor17->is_64bit);
        QCOMPARE(bestFor17->path, QStringLiteral("/opt/j17/bin/java"));

        auto bestFor21 = LaunchPreflightCheck::findBestInstalledJava(&list, 21, false);
        QVERIFY(bestFor21 != nullptr);
        QCOMPARE(bestFor21->id.major(), 21);

        // Compatible runtime produces no issue
        QVERIFY(!LaunchPreflightCheck::checkJavaRequirement(QStringLiteral("1.21.1"), 21, true, false, false, bestFor21).has_value());
        // Automatic Java or IgnoreJavaCompatibility bypasses mismatch
        QVERIFY(!LaunchPreflightCheck::checkJavaRequirement(QStringLiteral("1.21.1"), 17, true, true, false, bestFor21).has_value());
        QVERIFY(!LaunchPreflightCheck::checkJavaRequirement(QStringLiteral("1.21.1"), 17, true, false, true, bestFor21).has_value());

        // Incompatible runtime yields SwitchJava with suggested Java 21
        auto issue = LaunchPreflightCheck::checkJavaRequirement(QStringLiteral("1.21.1"), 17, true, false, false, bestFor21);
        QVERIFY(issue.has_value());
        QCOMPARE(issue->fixType, PreflightFixType::SwitchJava);
        QCOMPARE(issue->suggestedJavaPath, bestFor21->path);

        // Missing Java binary flags critical issue
        auto missing = LaunchPreflightCheck::checkJavaRequirement(QStringLiteral("1.21.1"), 21, false, false, false, bestFor21);
        QVERIFY(missing.has_value());
        QCOMPARE(missing->severity, PreflightSeverity::Critical);

        // requireExistingBinary=true skips nonexistent paths
        QVERIFY(LaunchPreflightCheck::findBestInstalledJava(&list, 17, true) == nullptr);

        QTemporaryDir binDir;
        QVERIFY(binDir.isValid());
        const QString realBin = binDir.filePath(QStringLiteral("java21.exe"));
        writeDummyFile(realBin, QByteArrayLiteral("mz"));
        j21->path = realBin;
        auto foundExisting = LaunchPreflightCheck::findBestInstalledJava(&list, 21, true);
        QVERIFY(foundExisting != nullptr);
        QCOMPARE(foundExisting->path, realBin);

        // Test loadIfNeeded = true with async Task::getLoadTask()
        class LazyJavaInstallList final : public JavaInstallList {
           public:
            explicit LazyJavaInstallList(JavaInstallPtr provided) : m_provided(std::move(provided)) {}
            Task::Ptr getLoadTask(bool) override
            {
                if (m_loaded) {
                    return nullptr;
                }
                class DelayedTask final : public Task {
                   public:
                    DelayedTask(LazyJavaInstallList* owner, JavaInstallPtr java) : m_owner(owner), m_java(std::move(java)) {}
                   protected:
                    void executeTask() override
                    {
                        QTimer::singleShot(25, this, [this]() {
                            m_owner->m_loaded = true;
                            m_owner->updateListData({ m_java });
                            emitSucceeded();
                        });
                    }
                   private:
                    LazyJavaInstallList* m_owner;
                    JavaInstallPtr m_java;
                };
                auto task = makeShared<DelayedTask>(this, m_provided);
                task->start();
                return task;
            }
            bool isLoaded() override { return m_loaded; }
            bool m_loaded = false;
            JavaInstallPtr m_provided;
        };

        LazyJavaInstallList lazyList(j21);
        auto loadedJava = LaunchPreflightCheck::findBestInstalledJava(&lazyList, 21, true, true);
        QVERIFY(lazyList.m_loaded);
        QVERIFY(loadedJava != nullptr);
        QCOMPARE(loadedJava->path, realBin);
    }

    void analyzeInstanceEndToEnd()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(tempDir.filePath(QStringLiteral("global.cfg")));
        const QString instRoot = tempDir.filePath(QStringLiteral("mc-analyze"));
        QDir().mkpath(instRoot);
        auto instSettings = std::make_unique<INISettingsObject>(QDir(instRoot).filePath(QStringLiteral("instance.cfg")));
        SettingsObject* rawSettings = instSettings.get();
        rawSettings->registerSetting("OverrideJavaLocation", false);
        rawSettings->registerSetting("JavaPath", "");
        rawSettings->registerSetting("JavaVersion", "");
        rawSettings->registerSetting("AutomaticJava", false);
        rawSettings->registerSetting("IgnoreJavaCompatibility", false);
        rawSettings->registerSetting("OverrideMemory", false);
        rawSettings->registerSetting("MinMemAlloc", 512);
        rawSettings->registerSetting("MaxMemAlloc", 4096);

        class TestMinecraftInstance final : public MinecraftInstance {
           public:
            TestMinecraftInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
                : MinecraftInstance(globalSettings, std::move(settings), rootDir)
            {
                setSpecificSettingsLoaded(true);
            }
        };

        TestMinecraftInstance inst(global.get(), std::move(instSettings), instRoot);
        auto* profile = inst.getPackProfile();
        profile->buildingFromScratch();
        QVERIFY(profile->installEmpty(QStringLiteral("net.minecraft"), QStringLiteral("Minecraft")));
        QVERIFY(profile->installEmpty(QStringLiteral("net.neoforge"), QStringLiteral("NeoForge")));

        // Verify missing Java binary + low modded RAM triggers both issues via analyze()
        rawSettings->set("OverrideJavaLocation", true);
        rawSettings->set("JavaPath", tempDir.filePath(QStringLiteral("missing-java.exe")));
        rawSettings->set("AutomaticJava", true);  // even if stale AutomaticJava=true, missing binary must be flagged
        rawSettings->set("MaxMemAlloc", 512);

        const PreflightReport report = LaunchPreflightCheck::analyze(&inst);
        QCOMPARE(report.issues.size(), 2);
        QCOMPARE(report.issues.at(0).severity, PreflightSeverity::Critical);
        QCOMPARE(report.issues.at(1).fixType, PreflightFixType::ClampRam);
        QCOMPARE(report.issues.at(1).suggestedRamMb, 2048);
    }

    void memoryBudgetChecks()
    {
        // Normal modded allocation (6 GB out of 16 GB) passes
        QVERIFY(!LaunchPreflightCheck::checkMemoryAllocation(6144, 16384, true, 8192).has_value());

        // Over-allocation (>85% of 16 GB) flags ClampRam
        auto highMem = LaunchPreflightCheck::checkMemoryAllocation(15360, 16384, true, 8192);
        QVERIFY(highMem.has_value());
        QCOMPARE(highMem->fixType, PreflightFixType::ClampRam);
        QVERIFY(highMem->suggestedRamMb <= 13926);

        // Under-allocation for modded instance flags ClampRam bump
        auto lowModded = LaunchPreflightCheck::checkMemoryAllocation(512, 16384, true, 8192);
        QVERIFY(lowModded.has_value());
        QCOMPARE(lowModded->fixType, PreflightFixType::ClampRam);
        QCOMPARE(lowModded->suggestedRamMb, 2048);

        // Vanilla instance with 768 MB is allowed
        QVERIFY(!LaunchPreflightCheck::checkMemoryAllocation(768, 16384, false, 4096).has_value());

        // Unknown system RAM (totalRamMiB == 0) must not emit false over-allocation error
        QVERIFY(!LaunchPreflightCheck::checkMemoryAllocation(8192, 0, true, 4096).has_value());
        // Still catches low modded allocation even when totalRamMiB == 0
        auto lowUnknownTotal = LaunchPreflightCheck::checkMemoryAllocation(512, 0, true, 4096);
        QVERIFY(lowUnknownTotal.has_value());
        QCOMPARE(lowUnknownTotal->suggestedRamMb, 2048);
    }

    void duplicateModDetectionAndFixApplication()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString modsDir = tempDir.filePath(QStringLiteral("mods"));
        QDir().mkpath(modsDir);

        // Duplicate slug + no zip metadata fallback
        const QString oldSodium = QDir(modsDir).filePath(QStringLiteral("sodium-fabric-0.5.8+mc1.20.1.jar"));
        const QString newSodium = QDir(modsDir).filePath(QStringLiteral("sodium-fabric-0.5.11+mc1.20.1.jar"));
        writeDummyFile(oldSodium, QByteArrayLiteral("old"));
        QThread::msleep(25);
        writeDummyFile(newSodium, QByteArrayLiteral("new"));

        // Disabled duplicate should be ignored
        const QString existingDisabled = QDir(modsDir).filePath(QStringLiteral("sodium-fabric-0.5.8+mc1.20.1.jar.disabled"));
        writeDummyFile(existingDisabled, QByteArrayLiteral("keep-disabled"));
        writeDummyFile(QDir(modsDir).filePath(QStringLiteral("iris-1.7.0.jar.disabled")));
        writeDummyFile(QDir(modsDir).filePath(QStringLiteral("iris-1.7.2.jar")));

        // Distinct sibling mods must NOT collide
        writeDummyFile(QDir(modsDir).filePath(QStringLiteral("create-1.20.1-0.5.1.jar")));
        writeDummyFile(QDir(modsDir).filePath(QStringLiteral("create-addition-1.20.1-1.2.jar")));

        const QList<PreflightIssue> dupIssues = LaunchPreflightCheck::detectDuplicateModsInDirectory(modsDir);
        QCOMPARE(dupIssues.size(), 1);
        QCOMPARE(dupIssues.first().fixType, PreflightFixType::DisableDuplicateMod);
        QCOMPARE(QFileInfo(dupIssues.first().duplicateJarToDisable).fileName(),
                 QStringLiteral("sodium-fabric-0.5.8+mc1.20.1.jar"));

        // Verify applying DisableDuplicateMod + ClampRam (including MinMemAlloc clamping) + SwitchJava fixes on NullInstance
        auto global = makeGlobalSettings(tempDir.filePath(QStringLiteral("global.cfg")));
        const QString instRoot = tempDir.filePath(QStringLiteral("inst"));
        QDir().mkpath(instRoot);
        auto instSettings = std::make_unique<INISettingsObject>(QDir(instRoot).filePath(QStringLiteral("instance.cfg")));
        NullInstance inst(global.get(), std::move(instSettings), instRoot);
        inst.settings()->registerSetting("OverrideMemory", false);
        inst.settings()->registerSetting("MinMemAlloc", 12288);
        inst.settings()->registerSetting("MaxMemAlloc", 15360);
        inst.settings()->registerSetting("OverrideJavaLocation", false);
        inst.settings()->registerSetting("JavaPath", "");

        PreflightReport report;
        report.issues.append(dupIssues.first());

        PreflightIssue ramFix;
        ramFix.fixType = PreflightFixType::ClampRam;
        ramFix.suggestedRamMb = 4096;
        report.issues.append(ramFix);

        PreflightIssue javaFix;
        javaFix.fixType = PreflightFixType::SwitchJava;
        javaFix.suggestedJavaPath = QStringLiteral("C:/Java21/bin/javaw.exe");
        javaFix.suggestedJavaVersion = QStringLiteral("21.0.5");
        report.issues.append(javaFix);

        QVERIFY(report.hasFixableIssues());
        QCOMPARE(LaunchPreflightCheck::applyAllFixes(&inst, report), 3);

        // Existing .disabled file must stay untouched; renamed duplicate gets .1.disabled
        QVERIFY(!QFile::exists(oldSodium));
        QVERIFY(QFile::exists(existingDisabled));
        QFile checkDisabled(existingDisabled);
        QVERIFY(checkDisabled.open(QIODevice::ReadOnly));
        QCOMPARE(checkDisabled.readAll(), QByteArrayLiteral("keep-disabled"));
        QVERIFY(QFile::exists(oldSodium + QStringLiteral(".1.disabled")));
        QVERIFY(QFile::exists(newSodium));
        QCOMPARE(inst.settings()->get("MaxMemAlloc").toInt(), 4096);
        QCOMPARE(inst.settings()->get("MinMemAlloc").toInt(), 4096);
        QCOMPARE(inst.settings()->get("OverrideJavaLocation").toBool(), true);
        QCOMPARE(inst.settings()->get("JavaPath").toString(), QStringLiteral("C:/Java21/bin/javaw.exe"));
    }
};

QTEST_GUILESS_MAIN(LaunchPreflightTest)

#include "LaunchPreflight_test.moc"
