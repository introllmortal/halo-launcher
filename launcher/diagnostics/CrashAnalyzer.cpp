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
#include "CrashAnalyzer.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "settings/SettingsObject.h"

namespace {
QString crashTr(const char* text)
{
    return QCoreApplication::translate("CrashAnalyzer", text);
}
}  // namespace

DiagnosisResult CrashAnalyzer::analyze(BaseInstance* instance,
                                       const QStringList& logLines,
                                       const QString& crashReportContent,
                                       int exitCode)
{
    QString combinedText = logLines.join(QLatin1Char('\n'));
    if (!crashReportContent.isEmpty()) {
        combinedText += QStringLiteral("\n--- CRASH REPORT ---\n") + crashReportContent;
    }

    auto res = detectOutOfMemory(combinedText, exitCode, instance);
    if (res.category != CrashCategory::Unknown) {
        return res;
    }

    res = detectJavaVersionMismatch(combinedText, instance);
    if (res.category != CrashCategory::Unknown) {
        return res;
    }

    res = detectDuplicateMods(combinedText, instance);
    if (res.category != CrashCategory::Unknown) {
        return res;
    }

    res = detectMissingDependency(combinedText, instance);
    if (res.category != CrashCategory::Unknown) {
        return res;
    }

    res = detectCorruptedConfig(combinedText, instance);
    if (res.category != CrashCategory::Unknown) {
        return res;
    }

    res = detectModConflict(combinedText, crashReportContent, instance);
    if (res.category != CrashCategory::Unknown) {
        return res;
    }

    res = detectGraphicsDriverCrash(combinedText, exitCode, instance);
    if (res.category != CrashCategory::Unknown) {
        return res;
    }

    return createFallbackDiagnosis(combinedText, crashReportContent, exitCode);
}

DiagnosisResult CrashAnalyzer::detectOutOfMemory(const QString& fullText, int exitCode, BaseInstance* instance)
{
    DiagnosisResult res;
    bool isOom = false;
    bool isNativeOom = false;
    QString oomType = QStringLiteral("Java Heap Space");

    if (fullText.contains(QStringLiteral("java.lang.OutOfMemoryError: Java heap space"), Qt::CaseInsensitive)) {
        isOom = true;
        oomType = QStringLiteral("Java heap space");
    } else if (fullText.contains(QStringLiteral("java.lang.OutOfMemoryError: Metaspace"), Qt::CaseInsensitive)) {
        isOom = true;
        oomType = QStringLiteral("Metaspace");
    } else if (fullText.contains(QStringLiteral("java.lang.OutOfMemoryError"), Qt::CaseInsensitive)) {
        isOom = true;
        oomType = QStringLiteral("Direct Buffer / Memory");
    } else if (fullText.contains(QStringLiteral("GC overhead limit exceeded"), Qt::CaseInsensitive)) {
        isOom = true;
        oomType = QStringLiteral("GC overhead limit");
    } else if (fullText.contains(QStringLiteral("insufficient memory for the Java Runtime Environment"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("Native memory allocation (mmap) failed"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("Native memory allocation (malloc) failed"), Qt::CaseInsensitive)) {
        isOom = true;
        isNativeOom = true;
        oomType = QStringLiteral("Native Memory (RAM)");
    } else if (exitCode == 137 || exitCode == -1073740791) {
        isOom = true;
        isNativeOom = true;
        oomType = QStringLiteral("System OOM Killer");
    }

    if (!isOom) {
        return res;
    }

    res.category = CrashCategory::OutOfMemory;
    res.fixAction = RecommendedFix::IncreaseRam;

    int currentAlloc = 4096;
    if (instance && instance->settings()) {
        currentAlloc = instance->settings()->get(QStringLiteral("MaxMemAlloc")).toInt();
        if (currentAlloc <= 0) {
            currentAlloc = 4096;
        }
    }

    const int suggested = currentAlloc < 6144 ? 8192 : currentAlloc + 2048;
    res.suggestedRamMb = suggested;

    res.title = crashTr("Insufficient Memory (Out of Memory)");
    res.sourceSummary = isNativeOom
                            ? crashTr("Operating system RAM / JVM native memory (%1)").arg(oomType)
                            : crashTr("Java memory limit (JVM %1, currently %2 MB)").arg(oomType).arg(currentAlloc);
    res.whySummary = isNativeOom
                         ? crashTr("The operating system could not provide enough free RAM for the game process.")
                         : crashTr("The game and installed mods required more memory than the current %1 MB limit.").arg(currentAlloc);
    res.description = crashTr("Minecraft ran out of available memory and closed (%1).").arg(oomType);
    res.solutionText = isNativeOom
                           ? crashTr("Close memory-heavy background apps, or lower the instance RAM limit if it is set close to total system RAM.")
                           : crashTr("Increase the allocated memory limit in Instance Settings -> Java (for example, to %1 MB), or reduce heavy resource packs and render distance.").arg(suggested);
    res.actionButtonText = crashTr("Allocate %1 MB RAM").arg(suggested);
    res.technicalDetails = QStringLiteral("Exit Code: %1\nMemory Error: %2\nCurrent MaxMemAlloc: %3 MB\nSuggested: %4 MB")
                               .arg(exitCode)
                               .arg(oomType)
                               .arg(currentAlloc)
                               .arg(suggested);
    return res;
}

DiagnosisResult CrashAnalyzer::detectJavaVersionMismatch(const QString& fullText, [[maybe_unused]] BaseInstance* instance)
{
    DiagnosisResult res;
    static const QRegularExpression javaPattern(
        R"((?:compiled by a more recent version of the Java Runtime \(class file version (\d+)\.\d+\)|class file version (\d+)\.\d+.*compiled by a more recent version of the Java Runtime))",
        QRegularExpression::CaseInsensitiveOption);
    const auto match = javaPattern.match(fullText);

    if (!match.hasMatch() && !fullText.contains(QStringLiteral("UnsupportedClassVersionError"), Qt::CaseInsensitive)) {
        return res;
    }

    static const QRegularExpression classPattern(
        R"(UnsupportedClassVersionError:\s*([a-zA-Z0-9_/$]+)\s+has been compiled)", QRegularExpression::CaseInsensitiveOption);
    const auto classMatch = classPattern.match(fullText);
    const QString compiledClass = classMatch.hasMatch() ? classMatch.captured(1).replace(QLatin1Char('/'), QLatin1Char('.')) : QString();

    const QString cap = !match.captured(1).isEmpty() ? match.captured(1) : match.captured(2);
    const int classVer = (match.hasMatch() && !cap.isEmpty()) ? cap.toInt() : 65;
    QString requiredJava = QStringLiteral("21");
    if (classVer >= 66) {
        requiredJava = QString::number(classVer - 44);
    } else if (classVer >= 65) {
        requiredJava = QStringLiteral("21");
    } else if (classVer >= 61) {
        requiredJava = QStringLiteral("17");
    } else if (classVer >= 60) {
        requiredJava = QStringLiteral("16");
    } else if (classVer >= 52) {
        requiredJava = QStringLiteral("8");
    }

    res.category = CrashCategory::JavaVersion;
    res.fixAction = RecommendedFix::SwitchJava;
    res.requiredJavaVersion = requiredJava;

    res.title = crashTr("Incompatible Java Version");
    res.sourceSummary = compiledClass.isEmpty()
                            ? crashTr("Java runtime environment (class file version %1 -> requires Java %2)").arg(classVer).arg(requiredJava)
                            : crashTr("Java runtime / class %1 (requires Java %2)").arg(compiledClass, requiredJava);
    res.whySummary = crashTr("A mod or Minecraft component was compiled for Java %1 (class version %2), while the instance launched with an older Java version.")
                         .arg(requiredJava)
                         .arg(classVer);
    res.description = crashTr("Minecraft could not load a class because the selected Java version is too old.");
    res.solutionText = crashTr("Select Java %1 in Instance Settings -> Java (or enable automatic Java selection in launcher settings).").arg(requiredJava);
    res.actionButtonText = crashTr("Switch to Java %1").arg(requiredJava);
    res.technicalDetails = match.hasMatch() ? match.captured(0) : QStringLiteral("UnsupportedClassVersionError");
    return res;
}

DiagnosisResult CrashAnalyzer::detectDuplicateMods(const QString& fullText, BaseInstance* instance)
{
    DiagnosisResult res;
    static const QRegularExpression dupPattern(
        R"(Duplicate\s+mods?\s+found[^\n\r]*:?\s*([^\n\r]+)|Duplicate\s+mod\s+id\s+'([^']+)'|Duplicate\s+versions\s+for\s+mod\s+ID\s+'([^']+)')",
        QRegularExpression::CaseInsensitiveOption);
    const auto match = dupPattern.match(fullText);

    if (!match.hasMatch() && !fullText.contains(QStringLiteral("DuplicateModsFoundException"), Qt::CaseInsensitive)) {
        return res;
    }

    QString rawModInfo;
    if (match.hasMatch()) {
        if (!match.captured(1).isEmpty()) {
            rawModInfo = match.captured(1);
        } else if (!match.captured(2).isEmpty()) {
            rawModInfo = match.captured(2);
        } else {
            rawModInfo = match.captured(3);
        }
    }
    rawModInfo = rawModInfo.trimmed();

    const QString modPath = findModPathByName(instance, rawModInfo);

    res.category = CrashCategory::DuplicateMods;
    res.fixAction = !modPath.isEmpty() ? RecommendedFix::DisableMod : RecommendedFix::None;
    res.offendingModPath = modPath;
    res.offendingModName = !modPath.isEmpty() ? QFileInfo(modPath).fileName() : rawModInfo;

    const QString modLabel = res.offendingModName.isEmpty() ? crashTr("Multiple duplicate mod files") : res.offendingModName;
    res.title = crashTr("Duplicate Mod Detected");
    res.sourceSummary = crashTr("Mods folder: %1").arg(modLabel);
    res.whySummary = crashTr("Two or more files in the mods folder register the same mod ID, so the mod loader refuses to start.");
    res.description = crashTr("Minecraft found duplicate copies or conflicting versions of the same mod: %1.").arg(modLabel);
    res.solutionText = crashTr("Open the mods folder and remove or disable the older duplicate copy so only one version remains.");
    if (!modPath.isEmpty()) {
        res.actionButtonText = crashTr("Disable %1").arg(res.offendingModName);
    }
    res.technicalDetails = match.hasMatch() ? match.captured(0) : QStringLiteral("DuplicateModsFoundException");
    return res;
}

DiagnosisResult CrashAnalyzer::detectCorruptedConfig(const QString& fullText, BaseInstance* instance)
{
    DiagnosisResult res;
    static const QRegularExpression cfgPattern(
        R"(ParsingException:[^\n\r]*in\s+([^\s,:]+\.toml)|Failed\s+to\s+parse\s+config\s+file:\s+([^\n\r]+)|Exception\s+reading\s+([^\s,:]+\.(?:toml|json|json5)))",
        QRegularExpression::CaseInsensitiveOption);
    const auto match = cfgPattern.match(fullText);

    if (!match.hasMatch()) {
        return res;
    }

    QString cfgName;
    if (!match.captured(1).isEmpty()) {
        cfgName = match.captured(1);
    } else if (!match.captured(2).isEmpty()) {
        cfgName = match.captured(2);
    } else {
        cfgName = match.captured(3);
    }
    cfgName = QFileInfo(cfgName.trimmed()).fileName();

    QString cfgPath;
    if (instance) {
        cfgPath = FS::PathCombine(instance->gameRoot(), QStringLiteral("config/") + cfgName);
        if (!QFile::exists(cfgPath)) {
            cfgPath = FS::PathCombine(instance->instanceRoot(), QStringLiteral(".minecraft/config/") + cfgName);
        }
    }

    res.category = CrashCategory::CorruptedConfig;
    res.fixAction = QFile::exists(cfgPath) ? RecommendedFix::DeleteConfig : RecommendedFix::None;
    res.offendingConfigPath = cfgPath;

    res.title = crashTr("Corrupted Configuration File");
    res.sourceSummary = crashTr("Configuration file: config/%1").arg(cfgName);
    res.whySummary = crashTr("The file contains broken syntax or was truncated (for example after an abrupt shutdown or manual edit), so the mod could not read it.");
    res.description = crashTr("Failed to read configuration file '%1'.").arg(cfgName);
    res.solutionText = crashTr("Open the config folder and either fix the syntax in '%1' or delete/rename it so the mod regenerates a clean default file on next launch.").arg(cfgName);
    if (QFile::exists(cfgPath)) {
        res.actionButtonText = crashTr("Reset Config File");
    }
    res.technicalDetails = match.captured(0);
    return res;
}
