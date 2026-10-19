// SPDX-License-Identifier: GPL-3.0-only
#include "CrashAnalyzer.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "settings/SettingsObject.h"

DiagnosisResult CrashAnalyzer::analyze(BaseInstance* instance, const QStringList& logLines, const QString& crashReportContent, int exitCode)
{
    QString combinedText = logLines.join("\n");
    if (!crashReportContent.isEmpty()) {
        combinedText += "\n--- CRASH REPORT ---\n" + crashReportContent;
    }

    // 1. Check Out Of Memory (High frequency and critical)
    auto res = detectOutOfMemory(combinedText, exitCode, instance);
    if (res.category != CrashCategory::Unknown) return res;

    // 2. Check Java Version Incompatibility
    res = detectJavaVersionMismatch(combinedText, instance);
    if (res.category != CrashCategory::Unknown) return res;

    // 3. Check Duplicate Mods
    res = detectDuplicateMods(combinedText, instance);
    if (res.category != CrashCategory::Unknown) return res;

    // 4. Check Missing Dependencies
    res = detectMissingDependency(combinedText, instance);
    if (res.category != CrashCategory::Unknown) return res;

    // 5. Check Graphics Driver / OpenGL Failure
    res = detectGraphicsDriverCrash(combinedText, exitCode);
    if (res.category != CrashCategory::Unknown) return res;

    // 6. Check Corrupted Configs
    res = detectCorruptedConfig(combinedText, instance);
    if (res.category != CrashCategory::Unknown) return res;

    // 7. Check Mod Conflict / Mixin Error
    res = detectModConflict(combinedText, crashReportContent, instance);
    if (res.category != CrashCategory::Unknown) return res;

    // 8. Fallback for unknown / generic crashes
    return createFallbackDiagnosis(combinedText, crashReportContent, exitCode);
}

DiagnosisResult CrashAnalyzer::detectOutOfMemory(const QString& fullText, int exitCode, BaseInstance* instance)
{
    DiagnosisResult res;
    bool isOom = false;
    QString oomType = "Java Heap Space";

    if (fullText.contains("java.lang.OutOfMemoryError: Java heap space", Qt::CaseInsensitive)) {
        isOom = true;
        oomType = "Java heap space";
    } else if (fullText.contains("java.lang.OutOfMemoryError: Metaspace", Qt::CaseInsensitive)) {
        isOom = true;
        oomType = "Metaspace";
    } else if (fullText.contains("java.lang.OutOfMemoryError", Qt::CaseInsensitive)) {
        isOom = true;
        oomType = "Direct Buffer / Memory";
    } else if (fullText.contains("GC overhead limit exceeded", Qt::CaseInsensitive)) {
        isOom = true;
        oomType = "GC overhead limit";
    } else if (fullText.contains("insufficient memory for the Java Runtime Environment", Qt::CaseInsensitive) ||
               fullText.contains("Native memory allocation (mmap) failed", Qt::CaseInsensitive) ||
               fullText.contains("Native memory allocation (malloc) failed", Qt::CaseInsensitive)) {
        isOom = true;
        oomType = "Native Memory (RAM)";
    } else if (exitCode == 137 || exitCode == -1073740791 /* 0xC0000017 STATUS_NO_MEMORY */) {
        isOom = true;
        oomType = "System OOM Killer";
    }

    if (!isOom) return res;

    res.category = CrashCategory::OutOfMemory;
    res.fixAction = RecommendedFix::IncreaseRam;

    int currentAlloc = 4096;
    if (instance && instance->settings()) {
        currentAlloc = instance->settings()->get("MaxMemAlloc").toInt();
        if (currentAlloc <= 0) currentAlloc = 4096;
    }

    int suggested = currentAlloc < 6144 ? 8192 : currentAlloc + 2048;
    res.suggestedRamMb = suggested;

    res.title = QCoreApplication::translate("CrashAnalyzer", "Insufficient Memory (Out of Memory)");
    res.description = QCoreApplication::translate("CrashAnalyzer",
        "Minecraft ran out of allocated RAM and terminated (%1).\n"
        "Currently allocated: %2 MB. This modpack requires more memory to run smoothly.")
        .arg(oomType)
        .arg(currentAlloc);

    res.solutionText = QCoreApplication::translate("CrashAnalyzer",
        "Increase allocated memory to %1 MB in the launcher instance settings.")
        .arg(suggested);

    res.actionButtonText = QCoreApplication::translate("CrashAnalyzer", "Allocate %1 MB RAM").arg(suggested);
    res.technicalDetails = QString("Exit Code: %1\nMemory Error: %2\nCurrent MaxMemAlloc: %3 MB\nSuggested: %4 MB")
        .arg(exitCode).arg(oomType).arg(currentAlloc).arg(suggested);

    return res;
}

DiagnosisResult CrashAnalyzer::detectJavaVersionMismatch(const QString& fullText, BaseInstance* instance)
{
    DiagnosisResult res;
    static const QRegularExpression javaPattern(R"(class file version (\d+)\.0.*compiled by a more recent version of the Java Runtime)", QRegularExpression::CaseInsensitiveOption);
    auto match = javaPattern.match(fullText);

    bool isJavaMismatch = match.hasMatch() || fullText.contains("UnsupportedClassVersionError", Qt::CaseInsensitive);

    if (!isJavaMismatch) return res;

    int classVer = match.hasMatch() ? match.captured(1).toInt() : 65;
    QString requiredJava = "21";
    if (classVer >= 65) requiredJava = "21";
    else if (classVer >= 61) requiredJava = "17";
    else if (classVer >= 60) requiredJava = "16";
    else if (classVer >= 52) requiredJava = "8";

    res.category = CrashCategory::JavaVersion;
    res.fixAction = RecommendedFix::SwitchJava;
    res.requiredJavaVersion = requiredJava;

    res.title = QCoreApplication::translate("CrashAnalyzer", "Incompatible Java Version");
    res.description = QCoreApplication::translate("CrashAnalyzer",
        "A mod was compiled for Java %1 (class version %2), but Minecraft was launched with an older or incompatible Java runtime.")
        .arg(requiredJava).arg(classVer);

    res.solutionText = QCoreApplication::translate("CrashAnalyzer",
        "Switch the instance Java installation to Java %1 in instance settings.")
        .arg(requiredJava);

    res.actionButtonText = QCoreApplication::translate("CrashAnalyzer", "Switch to Java %1").arg(requiredJava);
    res.technicalDetails = match.hasMatch() ? match.captured(0) : "UnsupportedClassVersionError";

    return res;
}

DiagnosisResult CrashAnalyzer::detectDuplicateMods(const QString& fullText, BaseInstance* instance)
{
    DiagnosisResult res;
    static const QRegularExpression dupPattern(R"(Duplicate\s+mods?\s+found[^\n\r]*:?\s*([^\n\r]+)|Duplicate\s+mod\s+id\s+'([^']+)'|Duplicate\s+versions\s+for\s+mod\s+ID\s+'([^']+)')", QRegularExpression::CaseInsensitiveOption);
    auto match = dupPattern.match(fullText);

    if (!match.hasMatch() && !fullText.contains("DuplicateModsFoundException", Qt::CaseInsensitive)) {
        return res;
    }

    QString rawModInfo;
    if (match.hasMatch()) {
        if (!match.captured(1).isEmpty()) rawModInfo = match.captured(1);
        else if (!match.captured(2).isEmpty()) rawModInfo = match.captured(2);
        else rawModInfo = match.captured(3);
    }
    rawModInfo = rawModInfo.trimmed();

    QString modPath = findModPathByName(instance, rawModInfo);

    res.category = CrashCategory::DuplicateMods;
    res.fixAction = !modPath.isEmpty() ? RecommendedFix::DisableMod : RecommendedFix::None;
    res.offendingModPath = modPath;
    res.offendingModName = !modPath.isEmpty() ? QFileInfo(modPath).fileName() : rawModInfo;

    res.title = QCoreApplication::translate("CrashAnalyzer", "Duplicate Mod Detected");
    res.description = QCoreApplication::translate("CrashAnalyzer",
        "Minecraft found multiple copies or conflicting versions of the same mod in your mods folder:\n%1")
        .arg(res.offendingModName.isEmpty() ? QCoreApplication::translate("CrashAnalyzer", "Multiple duplicate mod files") : res.offendingModName);

    res.solutionText = QCoreApplication::translate("CrashAnalyzer",
        "Remove or disable one of the duplicate mod files.");

    if (!modPath.isEmpty()) {
        res.actionButtonText = QCoreApplication::translate("CrashAnalyzer", "Disable %1").arg(res.offendingModName);
    }
    res.technicalDetails = match.hasMatch() ? match.captured(0) : "DuplicateModsFoundException";

    return res;
}

DiagnosisResult CrashAnalyzer::detectMissingDependency(const QString& fullText, BaseInstance* instance)
{
    DiagnosisResult res;
    static const QRegularExpression depPattern(R"(requires\s+(?:version\s+[^\n\r]+of\s+)?mod\s+'([^']+)'|Mod\s+'([^']+)'[^\n\r]*requires\s+'([^']+)'|ModLoadingException:\s*Mod\s+(\S+)\s+requires\s+(\S+)|Could\s+not\s+find\s+required\s+mod:\s*'([^']+)'|Missing\s+or\s+unsupported\s+mandatory\s+dependencies:\s*([^\n\r]+))", QRegularExpression::CaseInsensitiveOption);
    auto match = depPattern.match(fullText);

    if (!match.hasMatch() && !fullText.contains("Missing mandatory dependencies", Qt::CaseInsensitive)) {
        return res;
    }

    QString dependentMod;
    QString requiredMod;

    if (!match.captured(2).isEmpty() && !match.captured(3).isEmpty()) {
        dependentMod = match.captured(2);
        requiredMod = match.captured(3);
    } else if (!match.captured(4).isEmpty() && !match.captured(5).isEmpty()) {
        dependentMod = match.captured(4);
        requiredMod = match.captured(5);
    } else if (!match.captured(1).isEmpty()) {
        requiredMod = match.captured(1);
    } else if (!match.captured(6).isEmpty()) {
        requiredMod = match.captured(6);
    }

    res.category = CrashCategory::MissingDependency;
    res.fixAction = RecommendedFix::None;

    res.title = QCoreApplication::translate("CrashAnalyzer", "Missing Required Mod (Dependency)");
    if (!dependentMod.isEmpty() && !requiredMod.isEmpty()) {
        res.description = QCoreApplication::translate("CrashAnalyzer",
            "Mod '%1' requires '%2' to be installed, but it is missing or incompatible.")
            .arg(dependentMod, requiredMod);
    } else if (!requiredMod.isEmpty()) {
        res.description = QCoreApplication::translate("CrashAnalyzer",
            "The mod '%1' is required but is missing or incompatible.")
            .arg(requiredMod);
    } else {
        res.description = QCoreApplication::translate("CrashAnalyzer",
            "One or more mods are missing their mandatory library dependencies:\n%1")
            .arg(match.hasMatch() ? match.captured(0) : "Missing mandatory dependencies");
    }

    res.solutionText = QCoreApplication::translate("CrashAnalyzer",
        "Install the missing dependency mod into the mods folder or update the modpack.");

    res.technicalDetails = match.captured(0);

    return res;
}

DiagnosisResult CrashAnalyzer::detectGraphicsDriverCrash(const QString& fullText, int exitCode)
{
    DiagnosisResult res;
    bool isGpuCrash = false;
    QString reason;

    if (fullText.contains("GLFW error 65542", Qt::CaseInsensitive) || fullText.contains("WGL: The driver does not appear to support OpenGL", Qt::CaseInsensitive)) {
        isGpuCrash = true;
        reason = "GLFW error 65542 (Missing OpenGL acceleration)";
    } else if (fullText.contains("Pixel format not accelerated", Qt::CaseInsensitive)) {
        isGpuCrash = true;
        reason = "Pixel format not accelerated";
    } else if (fullText.contains("nvoglv64.dll", Qt::CaseInsensitive) || fullText.contains("atio6axx.dll", Qt::CaseInsensitive) || fullText.contains("ig9ic64.dll", Qt::CaseInsensitive) || fullText.contains("ig7ic64.dll", Qt::CaseInsensitive) || fullText.contains("ig4icd64.dll", Qt::CaseInsensitive)) {
        isGpuCrash = true;
        reason = "Graphics driver crashed in native GPU DLL";
    }

    if (!isGpuCrash) return res;

    res.category = CrashCategory::GraphicsDriver;
    res.fixAction = RecommendedFix::DisableShaders;

    res.title = QCoreApplication::translate("CrashAnalyzer", "Graphics Driver / OpenGL Error");
    res.description = QCoreApplication::translate("CrashAnalyzer",
        "Minecraft failed to communicate with your graphics card (%1).\n"
        "This commonly happens if graphics drivers are outdated, OpenGL is unsupported, or shaders are corrupted.")
        .arg(reason);

    res.solutionText = QCoreApplication::translate("CrashAnalyzer",
        "Update your graphics card drivers (NVIDIA/AMD/Intel) and disable active shaders.");

    res.actionButtonText = QCoreApplication::translate("CrashAnalyzer", "Disable Shaders");
    res.technicalDetails = QString("GPU Error: %1\nExit Code: %2").arg(reason).arg(exitCode);

    return res;
}

DiagnosisResult CrashAnalyzer::detectCorruptedConfig(const QString& fullText, BaseInstance* instance)
{
    DiagnosisResult res;
    static const QRegularExpression cfgPattern(R"(ParsingException:[^\n\r]*in\s+([^\s,:]+\.toml)|Failed\s+to\s+parse\s+config\s+file:\s+([^\n\r]+))", QRegularExpression::CaseInsensitiveOption);
    auto match = cfgPattern.match(fullText);

    if (!match.hasMatch()) return res;

    QString cfgName = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
    cfgName = cfgName.trimmed();

    QString cfgPath;
    if (instance) {
        cfgPath = FS::PathCombine(instance->gameRoot(), "config/" + cfgName);
        if (!QFile::exists(cfgPath)) {
            cfgPath = FS::PathCombine(instance->instanceRoot(), ".minecraft/config/" + cfgName);
        }
    }

    res.category = CrashCategory::CorruptedConfig;
    res.fixAction = QFile::exists(cfgPath) ? RecommendedFix::DeleteConfig : RecommendedFix::None;
    res.offendingConfigPath = cfgPath;

    res.title = QCoreApplication::translate("CrashAnalyzer", "Corrupted Configuration File");
    res.description = QCoreApplication::translate("CrashAnalyzer",
        "The configuration file '%1' contains invalid syntax or has been corrupted.")
        .arg(cfgName);

    res.solutionText = QCoreApplication::translate("CrashAnalyzer",
        "Reset or delete the corrupted configuration file so Minecraft can recreate a clean default one.");

    if (QFile::exists(cfgPath)) {
        res.actionButtonText = QCoreApplication::translate("CrashAnalyzer", "Reset Config File");
    }
    res.technicalDetails = match.captured(0);

    return res;
}

DiagnosisResult CrashAnalyzer::detectModConflict(const QString& fullText, const QString& crashReport, BaseInstance* instance)
{
    DiagnosisResult res;

    // Check Forge / NeoForge "Suspected Mods:" section
    static const QRegularExpression suspectedPattern(R"(Suspected\s+Mods?:\s*(?:None|[\r\n]+\s*([^\r\n,]+)|([^\r\n,]+)))", QRegularExpression::CaseInsensitiveOption);
    auto match = suspectedPattern.match(fullText);

    // Check NeoForge / FML ModLoadingException
    static const QRegularExpression fmlPattern(R"(ModLoadingException:\s*Mod\s+['"]?([a-zA-Z0-9_\-]+)['"]?\s+failed\s+to\s+load|Failed\s+to\s+load\s+mod:\s*['"]?([a-zA-Z0-9_\-]+)['"]?|ModIdList:\s*\[([a-zA-Z0-9_\-]+)\])", QRegularExpression::CaseInsensitiveOption);
    auto fmlMatch = fmlPattern.match(fullText);

    // Check Mixin application errors
    static const QRegularExpression mixinPattern(R"(Critical\s+injection\s+failure:\s*Callback\s*(\S+)\s+in\s+(\S+)|MixinApplyError:\s*Mixin\s+\[([^\]]+)\]|merged\s+from\s+(\S+\.mixins\.json)|MixinTransformerError:\s*An\s+unexpected\s+critical\s+error\s+was\s+encountered\s+in\s+(\S+))", QRegularExpression::CaseInsensitiveOption);
    auto mixinMatch = mixinPattern.match(fullText);

    QString modIdentifier;
    if (match.hasMatch()) {
        QString cand = match.captured(1).isEmpty() ? match.captured(2).trimmed() : match.captured(1).trimmed();
        if (cand.compare("None", Qt::CaseInsensitive) != 0 && !cand.isEmpty()) {
            modIdentifier = cand;
        }
    }
    if (modIdentifier.isEmpty() && fmlMatch.hasMatch()) {
        if (!fmlMatch.captured(1).isEmpty()) modIdentifier = fmlMatch.captured(1);
        else if (!fmlMatch.captured(2).isEmpty()) modIdentifier = fmlMatch.captured(2);
        else modIdentifier = fmlMatch.captured(3);
    }
    if (modIdentifier.isEmpty() && mixinMatch.hasMatch()) {
        if (!mixinMatch.captured(4).isEmpty()) modIdentifier = mixinMatch.captured(4);
        else if (!mixinMatch.captured(5).isEmpty()) modIdentifier = mixinMatch.captured(5);
        else modIdentifier = mixinMatch.captured(3).isEmpty() ? mixinMatch.captured(2) : mixinMatch.captured(3);
    }

    if (modIdentifier.isEmpty()) {
        return res;
    }

    QString modPath = findModPathByName(instance, modIdentifier);

    res.category = CrashCategory::ModConflict;
    res.fixAction = !modPath.isEmpty() ? RecommendedFix::DisableMod : RecommendedFix::None;
    res.offendingModPath = modPath;
    res.offendingModName = !modPath.isEmpty() ? QFileInfo(modPath).fileName() : modIdentifier;

    res.title = QCoreApplication::translate("CrashAnalyzer", "Mod Crash / Conflict Detected");
    res.description = QCoreApplication::translate("CrashAnalyzer",
        "A game crash occurred caused by mod:\n%1\n"
        "The mod encountered an internal error or conflicted with another installed mod.")
        .arg(res.offendingModName);

    res.solutionText = QCoreApplication::translate("CrashAnalyzer",
        "Temporarily disable or update this mod to resolve the crash.");

    if (!modPath.isEmpty()) {
        res.actionButtonText = QCoreApplication::translate("CrashAnalyzer", "Disable %1").arg(res.offendingModName);
    }

    if (match.hasMatch() && !match.captured(0).contains("None", Qt::CaseInsensitive)) {
        res.technicalDetails = match.captured(0);
    } else if (fmlMatch.hasMatch()) {
        res.technicalDetails = fmlMatch.captured(0);
    } else if (mixinMatch.hasMatch()) {
        res.technicalDetails = mixinMatch.captured(0);
    }

    return res;
}

DiagnosisResult CrashAnalyzer::createFallbackDiagnosis(const QString& fullText, const QString& crashReport, int exitCode)
{
    DiagnosisResult res;
    res.category = CrashCategory::Unknown;
    res.fixAction = RecommendedFix::UploadLog;

    // Find deepest "Caused by: ..."
    QString rootCause;
    int lastCausedByIdx = -1;
    int searchPos = 0;
    while ((searchPos = fullText.indexOf("Caused by:", searchPos, Qt::CaseInsensitive)) != -1) {
        lastCausedByIdx = searchPos;
        searchPos += 10;
    }

    if (lastCausedByIdx != -1) {
        int endOfLine = fullText.indexOf("\n", lastCausedByIdx);
        if (endOfLine == -1) endOfLine = fullText.length();
        rootCause = fullText.mid(lastCausedByIdx, endOfLine - lastCausedByIdx).trimmed();
    }

    res.title = QCoreApplication::translate("CrashAnalyzer", "Unexpected Game Crash");
    if (!rootCause.isEmpty()) {
        res.description = QCoreApplication::translate("CrashAnalyzer",
            "Minecraft closed unexpectedly (Exit code %1).\nRoot cause:\n%2")
            .arg(exitCode).arg(rootCause);
    } else {
        res.description = QCoreApplication::translate("CrashAnalyzer",
            "Minecraft terminated unexpectedly with exit code %1.\n"
            "The specific cause could not be determined automatically.")
            .arg(exitCode);
    }

    res.solutionText = QCoreApplication::translate("CrashAnalyzer",
        "Upload the crash log to generate a clean shareable link for server administrators or developers.");

    res.actionButtonText = QCoreApplication::translate("CrashAnalyzer", "Upload Log to mclo.gs");
    res.technicalDetails = !rootCause.isEmpty() ? rootCause : QString("Process exited with code %1").arg(exitCode);

    return res;
}

QString CrashAnalyzer::findModPathByName(BaseInstance* instance, const QString& nameOrModId)
{
    if (!instance || nameOrModId.isEmpty()) return QString();

    QString cleanQuery = nameOrModId;
    // If it's a mixin config reference like sodium.mixins.json:features.buffer...
    int mixinIdx = cleanQuery.indexOf(".mixins.json", 0, Qt::CaseInsensitive);
    if (mixinIdx > 0) {
        cleanQuery = cleanQuery.left(mixinIdx).trimmed();
    }
    // If it has colon e.g. modid:Class
    int colonIdx = cleanQuery.indexOf(':');
    if (colonIdx > 0) {
        cleanQuery = cleanQuery.left(colonIdx).trimmed();
    }
    // Strip versions or parentheses like ModName (modid)
    int parenIdx = cleanQuery.indexOf('(');
    if (parenIdx > 0) {
        cleanQuery = cleanQuery.left(parenIdx).trimmed();
    }
    // Remove punctuation
    cleanQuery.remove(':').remove('\'').remove('"');
    cleanQuery = cleanQuery.trimmed();
    if (cleanQuery.isEmpty()) {
        return QString();
    }

    QStringList searchFolders = {
        FS::PathCombine(instance->gameRoot(), "mods"),
        FS::PathCombine(instance->instanceRoot(), ".minecraft/mods"),
        FS::PathCombine(instance->instanceRoot(), "mods")
    };

    for (const auto& folder : searchFolders) {
        if (!QDir(folder).exists()) continue;
        QDir dir(folder);
        QStringList files = dir.entryList({ "*.jar" }, QDir::Files);
        for (const auto& file : files) {
            if (file.contains(cleanQuery, Qt::CaseInsensitive)) {
                return dir.absoluteFilePath(file);
            }
        }

        // Normalize underscores and hyphens
        QString normQuery = cleanQuery;
        normQuery.replace('_', '-');
        for (const auto& file : files) {
            QString normFile = file;
            normFile.replace('_', '-');
            if (normFile.contains(normQuery, Qt::CaseInsensitive)) {
                return dir.absoluteFilePath(file);
            }
        }
    }

    return QString();
}
