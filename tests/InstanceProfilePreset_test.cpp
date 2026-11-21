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
#include <memory>
#include <utility>

#include "FileSystem.h"
#include "NullInstance.h"
#include "settings/INISettingsObject.h"
#include "ui/pages/instance/InstanceProfilePreset.h"

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

void writeFileBytes(const QString& path, const QByteArray& bytes)
{
    const QFileInfo fi(path);
    QVERIFY(FS::ensureFolderPathExists(fi.absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(bytes), bytes.size());
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

class PresetTestInstance final : public NullInstance {
   public:
    PresetTestInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir)
        : NullInstance(globalSettings, std::move(settings), rootDir)
    {
    }
    QString modsRoot() const override { return FS::PathCombine(gameRoot(), QStringLiteral("mods")); }
};

}  // namespace

class InstanceProfilePresetTest : public QObject {
    Q_OBJECT

   private slots:
    void saveAndSwitchPresetsWithShadersAndMods()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst1"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        PresetTestInstance inst(global.get(), std::move(instSettings), instDir);

        // State 1: Shaders profile (sodium + iris enabled, ferritecore disabled, BSL.zip shader)
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("sodium.jar")), QByteArrayLiteral("sodium"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("iris.jar")), QByteArrayLiteral("iris"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("ferritecore.jar.disabled")), QByteArrayLiteral("ferrite"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("optionsshaders.txt")),
                       QByteArrayLiteral("antialiasingLevel=0\nshaderPack=BSL.zip\nshadowResMul=1.0\n"));

        QString err;
        const auto shadersPreset = InstanceProfilePreset::saveCurrentAsPreset(&inst, QStringLiteral("Shaders"), true, &err);
        QVERIFY2(shadersPreset.has_value(), qPrintable(err));
        QCOMPARE(shadersPreset->id, QStringLiteral("shaders"));
        QCOMPARE(shadersPreset->name, QStringLiteral("Shaders"));
        QCOMPARE(shadersPreset->shaderPack, std::optional<QString>(QStringLiteral("BSL.zip")));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), QStringLiteral("shaders"));

        // Verify stored outside mods/, config/, saves/
        const QString expectedFile =
            FS::PathCombine(instDir, QStringLiteral("halo-presets"), QStringLiteral("shaders.json"));
        QVERIFY(QFileInfo::exists(expectedFile));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("shaders.json"))));

        // State 2: HighFPS profile (sodium + ferritecore enabled, iris disabled, shaderPack=off)
        QVERIFY(QFile::remove(FS::PathCombine(inst.modsRoot(), QStringLiteral("iris.jar"))));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("iris.jar.disabled")), QByteArrayLiteral("iris"));
        QVERIFY(QFile::remove(FS::PathCombine(inst.modsRoot(), QStringLiteral("ferritecore.jar.disabled"))));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("ferritecore.jar")), QByteArrayLiteral("ferrite"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("optionsshaders.txt")),
                       QByteArrayLiteral("antialiasingLevel=0\nshaderPack=off\nshadowResMul=1.0\n"));

        const auto highFpsPreset = InstanceProfilePreset::saveCurrentAsPreset(&inst, QStringLiteral("HighFPS"), true, &err);
        QVERIFY2(highFpsPreset.has_value(), qPrintable(err));
        QCOMPARE(highFpsPreset->id, QStringLiteral("highfps"));
        QCOMPARE(highFpsPreset->shaderPack, std::optional<QString>(QStringLiteral("off")));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), QStringLiteral("highfps"));

        // List presets sorted by name ("HighFPS" < "Shaders")
        const auto listed = InstanceProfilePreset::listPresets(&inst);
        QCOMPARE(listed.size(), 2);
        QCOMPARE(listed.at(0).name, QStringLiteral("HighFPS"));
        QCOMPARE(listed.at(1).name, QStringLiteral("Shaders"));

        // Switch back to Shaders
        QVERIFY2(InstanceProfilePreset::applyPreset(&inst, QStringLiteral("shaders"), &err), qPrintable(err));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), QStringLiteral("shaders"));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("sodium.jar"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("iris.jar"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("iris.jar.disabled"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("ferritecore.jar.disabled"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("ferritecore.jar"))));
        QCOMPARE(readFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("optionsshaders.txt"))),
                 QByteArrayLiteral("antialiasingLevel=0\nshaderPack=BSL.zip\nshadowResMul=1.0\n"));

        // Switch back to HighFPS
        QVERIFY2(InstanceProfilePreset::applyPreset(&inst, QStringLiteral("highfps"), &err), qPrintable(err));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), QStringLiteral("highfps"));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("sodium.jar"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("ferritecore.jar"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("iris.jar.disabled"))));
        QCOMPARE(readFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("optionsshaders.txt"))),
                 QByteArrayLiteral("antialiasingLevel=0\nshaderPack=off\nshadowResMul=1.0\n"));

        // Delete active preset clears activePresetId
        QVERIFY(InstanceProfilePreset::deletePreset(&inst, QStringLiteral("highfps")));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), QString());
        QCOMPARE(InstanceProfilePreset::listPresets(&inst).size(), 1);
    }

    void newlyAddedModRemainsEnabledAcrossPresetSwitches()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst2"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        PresetTestInstance inst(global.get(), std::move(instSettings), instDir);

        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("alpha.jar")), QByteArrayLiteral("alpha"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("beta.jar.disabled")), QByteArrayLiteral("beta"));
        QVERIFY(InstanceProfilePreset::saveCurrentAsPreset(&inst, QStringLiteral("Preset A"), false).has_value());

        QVERIFY(QFile::rename(FS::PathCombine(inst.modsRoot(), QStringLiteral("alpha.jar")),
                              FS::PathCombine(inst.modsRoot(), QStringLiteral("alpha.jar.disabled"))));
        QVERIFY(QFile::rename(FS::PathCombine(inst.modsRoot(), QStringLiteral("beta.jar.disabled")),
                              FS::PathCombine(inst.modsRoot(), QStringLiteral("beta.jar"))));
        QVERIFY(InstanceProfilePreset::saveCurrentAsPreset(&inst, QStringLiteral("Preset B"), false).has_value());

        // Add a brand new mod after both presets were saved
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("newmod.jar")), QByteArrayLiteral("newmod"));

        QString err;
        QVERIFY2(InstanceProfilePreset::applyPreset(&inst, QStringLiteral("preset-a"), &err), qPrintable(err));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("alpha.jar"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("beta.jar.disabled"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("newmod.jar"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("newmod.jar.disabled"))));
    }

    void refusesApplyWhenRunningOrBisectActive()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst3"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        PresetTestInstance inst(global.get(), std::move(instSettings), instDir);

        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("core.jar")), QByteArrayLiteral("core"));
        const auto saved = InstanceProfilePreset::saveCurrentAsPreset(&inst, QStringLiteral("Base"), false);
        QVERIFY(saved.has_value());

        QVERIFY(QFile::rename(FS::PathCombine(inst.modsRoot(), QStringLiteral("core.jar")),
                              FS::PathCombine(inst.modsRoot(), QStringLiteral("core.jar.disabled"))));
        InstanceProfilePreset::setActivePresetId(&inst, QString());

        // 1) Refuses when instance is running
        inst.setRunning(true);
        QString err;
        QVERIFY(!InstanceProfilePreset::applyPreset(&inst, saved->id, &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("core.jar.disabled"))));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), QString());
        inst.setRunning(false);

        // 2) Refuses when halo-bisect.json exists
        const QString bisectPath = FS::PathCombine(instDir, QStringLiteral("halo-bisect.json"));
        writeFileBytes(bisectPath, QByteArrayLiteral("{\"active\":true}"));
        err.clear();
        QVERIFY(!InstanceProfilePreset::applyPreset(&inst, saved->id, &err));
        QVERIFY(!err.isEmpty());
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("core.jar.disabled"))));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), QString());

        // Succeeds once bisect lock is removed
        QVERIFY(QFile::remove(bisectPath));
        QVERIFY2(InstanceProfilePreset::applyPreset(&inst, saved->id, &err), qPrintable(err));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("core.jar"))));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), saved->id);
    }

    void noClobberAndNoPartialMutationOnTargetCollision()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto global = makeGlobalSettings(FS::PathCombine(tempDir.path(), QStringLiteral("global.cfg")));
        const QString instDir = FS::PathCombine(tempDir.path(), QStringLiteral("inst4"));
        QVERIFY(FS::ensureFolderPathExists(instDir));
        auto instSettings = std::make_unique<INISettingsObject>(FS::PathCombine(instDir, QStringLiteral("instance.cfg")));
        PresetTestInstance inst(global.get(), std::move(instSettings), instDir);

        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("modA.jar")), QByteArrayLiteral("modA-v1"));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("modB.jar.disabled")), QByteArrayLiteral("modB-v1"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("optionsshaders.txt")),
                       QByteArrayLiteral("shaderPack=Complementary.zip\n"));

        const auto targetPreset = InstanceProfilePreset::saveCurrentAsPreset(&inst, QStringLiteral("Target"), true);
        QVERIFY(targetPreset.has_value());

        // Move to a conflicting state: modA is disabled, modB is enabled AND a stale modB.jar.disabled exists
        QVERIFY(QFile::rename(FS::PathCombine(inst.modsRoot(), QStringLiteral("modA.jar")),
                              FS::PathCombine(inst.modsRoot(), QStringLiteral("modA.jar.disabled"))));
        QVERIFY(QFile::rename(FS::PathCombine(inst.modsRoot(), QStringLiteral("modB.jar.disabled")),
                              FS::PathCombine(inst.modsRoot(), QStringLiteral("modB.jar"))));
        writeFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("modB.jar.disabled")),
                       QByteArrayLiteral("sentinel-do-not-overwrite"));
        writeFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("optionsshaders.txt")), QByteArrayLiteral("shaderPack=off\n"));
        InstanceProfilePreset::setActivePresetId(&inst, QStringLiteral("other"));

        QString err;
        QVERIFY(!InstanceProfilePreset::applyPreset(&inst, targetPreset->id, &err));
        QVERIFY(!err.isEmpty());

        // Existing target was NOT clobbered, modA was NOT partially enabled, optionsshaders & active ID untouched
        QCOMPARE(readFileBytes(FS::PathCombine(inst.modsRoot(), QStringLiteral("modB.jar.disabled"))),
                 QByteArrayLiteral("sentinel-do-not-overwrite"));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("modB.jar"))));
        QVERIFY(QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("modA.jar.disabled"))));
        QVERIFY(!QFileInfo::exists(FS::PathCombine(inst.modsRoot(), QStringLiteral("modA.jar"))));
        QCOMPARE(readFileBytes(FS::PathCombine(inst.gameRoot(), QStringLiteral("optionsshaders.txt"))),
                 QByteArrayLiteral("shaderPack=off\n"));
        QCOMPARE(InstanceProfilePreset::activePresetId(&inst), QStringLiteral("other"));
    }
};

QTEST_GUILESS_MAIN(InstanceProfilePresetTest)

#include "InstanceProfilePreset_test.moc"
