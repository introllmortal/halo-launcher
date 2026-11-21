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
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModFolderModel.h"

namespace {
QString crashTr(const char* text)
{
    return QCoreApplication::translate("CrashAnalyzer", text);
}

QString extractCrashReportDescription(const QString& text)
{
    static const QRegularExpression descPattern(R"(^\s*Description:\s*([^\r\n]+))", QRegularExpression::MultilineOption);
    const auto match = descPattern.match(text);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}
}  // namespace

DiagnosisResult CrashAnalyzer::createFallbackDiagnosis(const QString& fullText, const QString& crashReport, int exitCode)
{
    DiagnosisResult res;
    res.category = CrashCategory::Unknown;
    res.fixAction = RecommendedFix::UploadLog;

    QString rootCause;
    int lastCausedByIdx = -1;
    int searchPos = 0;
    while ((searchPos = fullText.indexOf(QStringLiteral("Caused by:"), searchPos, Qt::CaseInsensitive)) != -1) {
        lastCausedByIdx = searchPos;
        searchPos += 10;
    }
    if (lastCausedByIdx != -1) {
        int endOfLine = fullText.indexOf(QLatin1Char('\n'), lastCausedByIdx);
        if (endOfLine == -1) {
            endOfLine = fullText.length();
        }
        rootCause = fullText.mid(lastCausedByIdx, endOfLine - lastCausedByIdx).trimmed();
    }

    const QString reportDesc = extractCrashReportDescription(!crashReport.isEmpty() ? crashReport : fullText);
    const QString stackMod = extractModIdentifierFromStackTrace(!crashReport.isEmpty() ? crashReport : fullText);

    const bool isBinaryCompat = fullText.contains(QStringLiteral("NoSuchMethodError"), Qt::CaseInsensitive) ||
                                fullText.contains(QStringLiteral("NoClassDefFoundError"), Qt::CaseInsensitive) ||
                                fullText.contains(QStringLiteral("AbstractMethodError"), Qt::CaseInsensitive) ||
                                fullText.contains(QStringLiteral("IncompatibleClassChangeError"), Qt::CaseInsensitive);
    const bool isTickingCrash = reportDesc.contains(QStringLiteral("Ticking entity"), Qt::CaseInsensitive) ||
                                reportDesc.contains(QStringLiteral("Ticking block entity"), Qt::CaseInsensitive) ||
                                reportDesc.contains(QStringLiteral("Ticking world"), Qt::CaseInsensitive);

    res.title = crashTr("Unexpected Game Crash");
    if (!stackMod.isEmpty()) {
        res.sourceSummary = crashTr("Suspected component from stack trace: %1 (exit code %2)").arg(stackMod).arg(exitCode);
    } else if (!reportDesc.isEmpty()) {
        res.sourceSummary = crashTr("Crash stage: %1 (exit code %2)").arg(reportDesc).arg(exitCode);
    } else {
        res.sourceSummary = crashTr("Exact source could not be isolated automatically (exit code %1)").arg(exitCode);
    }

    if (isBinaryCompat) {
        res.whySummary = crashTr("A mod called a method or class that does not exist in the installed version of Minecraft, loader, or a library mod.");
        res.solutionText = crashTr("Check that recently added or updated mods match your exact Minecraft and mod loader version, and inspect Technical Details for the missing class/method.");
    } else if (isTickingCrash) {
        res.whySummary = crashTr("An entity or block in the loaded world caused an error during a game tick (%1).").arg(reportDesc);
        res.solutionText = crashTr("Check Technical Details for the coordinates or entity name, try loading a world backup, or check recently updated world/mob mods.");
    } else if (!rootCause.isEmpty()) {
        res.whySummary = crashTr("The deepest error reported in the log was: %1").arg(rootCause);
        res.solutionText = crashTr("Inspect Technical Details, check recently changed mods or configs, or share the full log via mclo.gs to ask for help.");
    } else {
        res.whySummary = crashTr("The game process stopped unexpectedly without a recognized error signature in the log.");
        res.solutionText = crashTr("Check the full log in the instance folder (logs/latest.log or crash-reports/) or share it via mclo.gs.");
    }

    res.description = !rootCause.isEmpty()
                          ? crashTr("Minecraft closed unexpectedly (exit code %1).").arg(exitCode)
                          : crashTr("Minecraft terminated unexpectedly with exit code %1.").arg(exitCode);
    res.actionButtonText = crashTr("Upload Log to mclo.gs");

    QStringList details;
    if (!reportDesc.isEmpty()) {
        details.append(QStringLiteral("Description: ") + reportDesc);
    }
    if (!rootCause.isEmpty()) {
        details.append(rootCause);
    }
    if (details.isEmpty()) {
        details.append(QStringLiteral("Process exited with code %1").arg(exitCode));
    }
    res.technicalDetails = details.join(QLatin1Char('\n'));
    return res;
}

QString CrashAnalyzer::extractModIdentifierFromStackTrace(const QString& fullText)
{
    int startPos = fullText.indexOf(QStringLiteral("---- Minecraft Crash Report ----"));
    if (startPos == -1) {
        startPos = fullText.lastIndexOf(QStringLiteral("Caused by:"));
    }
    if (startPos == -1) {
        startPos = fullText.indexOf(QStringLiteral("Exception in thread "));
    }
    if (startPos == -1) {
        startPos = 0;
    }

    const QString textSlice = fullText.mid(startPos, 6000);
    static const QRegularExpression stackFramePattern(
        R"(\tat\s+(?:(?:[a-zA-Z0-9_\-]+/)?([a-zA-Z0-9_\-]+)@[^/]+/|(?:[a-zA-Z0-9_\-]+/)+)?([a-zA-Z0-9_.]+)\.([a-zA-Z0-9_<>$]+)\([^\)]*\)(?:\s*~\[([^%!#\]\s]+\.jar))?)",
        QRegularExpression::CaseInsensitiveOption);
    auto iter = stackFramePattern.globalMatch(textSlice);

    while (iter.hasNext()) {
        const auto m = iter.next();
        const QString modId = m.captured(1);
        const QString fullClass = m.captured(2);
        const QString jarName = m.captured(4);

        if (fullClass.startsWith(QLatin1String("java.")) || fullClass.startsWith(QLatin1String("javax.")) ||
            fullClass.startsWith(QLatin1String("sun.")) || fullClass.startsWith(QLatin1String("jdk.")) ||
            fullClass.startsWith(QLatin1String("net.minecraft.")) || fullClass.startsWith(QLatin1String("com.mojang.")) ||
            fullClass.startsWith(QLatin1String("org.spongepowered.asm.")) || fullClass.startsWith(QLatin1String("cpw.mods.")) ||
            fullClass.startsWith(QLatin1String("net.neoforged.")) || fullClass.startsWith(QLatin1String("net.minecraftforge.")) ||
            fullClass.startsWith(QLatin1String("org.lwjgl.")) || fullClass.startsWith(QLatin1String("io.netty.")) ||
            fullClass.startsWith(QLatin1String("org.apache.logging.")) || fullClass.startsWith(QLatin1String("org.slf4j."))) {
            continue;
        }

        if (!jarName.isEmpty() && !jarName.startsWith(QLatin1String("client-")) && !jarName.startsWith(QLatin1String("server-")) &&
            !jarName.startsWith(QLatin1String("neoforge-")) && !jarName.startsWith(QLatin1String("forge-"))) {
            return jarName;
        }
        if (!modId.isEmpty() && modId != QLatin1String("minecraft") && modId != QLatin1String("neoforge") &&
            modId != QLatin1String("forge")) {
            return modId;
        }

        const QStringList parts = fullClass.split(QLatin1Char('.'));
        if (parts.size() >= 3) {
            if (parts.size() >= 4 && parts[0] == QLatin1String("dev") && parts[2] == QLatin1String("mods")) {
                return parts[3];
            }
            if (parts[0] == QLatin1String("com") || parts[0] == QLatin1String("org") || parts[0] == QLatin1String("net") ||
                parts[0] == QLatin1String("io")) {
                return parts[2];
            }
            return parts.value(parts.size() - 2);
        }
    }

    return {};
}

QString CrashAnalyzer::findModPathByName(BaseInstance* instance, const QString& nameOrModId)
{
    if (!instance || nameOrModId.isEmpty()) {
        return {};
    }

    QString cleanQuery = nameOrModId;
    if (cleanQuery.startsWith(QLatin1String("mixins."), Qt::CaseInsensitive)) {
        cleanQuery = cleanQuery.mid(7);
    } else if (cleanQuery.startsWith(QLatin1String("mixin."), Qt::CaseInsensitive)) {
        cleanQuery = cleanQuery.mid(6);
    }
    const int mixinIdx = cleanQuery.indexOf(QLatin1String(".mixin"), 0, Qt::CaseInsensitive);
    if (mixinIdx > 0) {
        cleanQuery = cleanQuery.left(mixinIdx).trimmed();
    }
    if (cleanQuery.endsWith(QLatin1String(".json"), Qt::CaseInsensitive)) {
        cleanQuery.chop(5);
    }
    const int colonIdx = cleanQuery.indexOf(QLatin1Char(':'));
    if (colonIdx > 0) {
        cleanQuery = cleanQuery.left(colonIdx).trimmed();
    }
    const int parenIdx = cleanQuery.indexOf(QLatin1Char('('));
    if (parenIdx > 0) {
        cleanQuery = cleanQuery.left(parenIdx).trimmed();
    }
    cleanQuery.remove(QLatin1Char(':')).remove(QLatin1Char('\'')).remove(QLatin1Char('"')).remove(QLatin1Char('`'));
    cleanQuery = cleanQuery.trimmed();
    if (cleanQuery.isEmpty()) {
        return {};
    }

    if (auto* mcInst = dynamic_cast<MinecraftInstance*>(instance)) {
        if (auto* modList = mcInst->loaderModList()) {
            for (auto* mod : modList->allMods()) {
                if (!mod) {
                    continue;
                }
                if (mod->mod_id().compare(cleanQuery, Qt::CaseInsensitive) == 0 ||
                    mod->name().compare(cleanQuery, Qt::CaseInsensitive) == 0 ||
                    mod->fileinfo().fileName().compare(cleanQuery, Qt::CaseInsensitive) == 0) {
                    return mod->fileinfo().absoluteFilePath();
                }
            }
        }
    }

    const QStringList searchFolders = {
        FS::PathCombine(instance->gameRoot(), QStringLiteral("mods")),
        FS::PathCombine(instance->instanceRoot(), QStringLiteral(".minecraft/mods")),
        FS::PathCombine(instance->instanceRoot(), QStringLiteral("mods")),
    };

    for (const auto& folder : searchFolders) {
        QDir dir(folder);
        if (!dir.exists()) {
            continue;
        }
        const QStringList files = dir.entryList({ QStringLiteral("*.jar"), QStringLiteral("*.jar.disabled") }, QDir::Files);

        for (const auto& file : files) {
            if (file.compare(cleanQuery, Qt::CaseInsensitive) == 0) {
                return dir.absoluteFilePath(file);
            }
            QString base = file;
            if (base.endsWith(QLatin1String(".disabled"))) {
                base.chop(9);
            }
            if (base.endsWith(QLatin1String(".jar"))) {
                base.chop(4);
            }
            if (base.compare(cleanQuery, Qt::CaseInsensitive) == 0) {
                return dir.absoluteFilePath(file);
            }
        }
        for (const auto& file : files) {
            if (file.startsWith(cleanQuery + QLatin1Char('-'), Qt::CaseInsensitive) ||
                file.startsWith(cleanQuery + QLatin1Char('_'), Qt::CaseInsensitive)) {
                return dir.absoluteFilePath(file);
            }
        }
        for (const auto& file : files) {
            if (file.contains(cleanQuery, Qt::CaseInsensitive)) {
                return dir.absoluteFilePath(file);
            }
        }
        QString cleanQ = cleanQuery;
        cleanQ.remove(QLatin1Char('-')).remove(QLatin1Char('_')).remove(QLatin1Char('.')).remove(QLatin1Char(' '));
        if (cleanQ.length() >= 3) {
            for (const auto& file : files) {
                QString cleanF = file;
                cleanF.remove(QLatin1Char('-')).remove(QLatin1Char('_')).remove(QLatin1Char('.')).remove(QLatin1Char(' '));
                if (cleanF.contains(cleanQ, Qt::CaseInsensitive)) {
                    return dir.absoluteFilePath(file);
                }
            }
        }
    }

    return {};
}
