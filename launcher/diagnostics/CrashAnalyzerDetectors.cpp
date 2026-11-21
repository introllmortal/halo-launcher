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
#include <QFileInfo>
#include <QRegularExpression>

#include "BaseInstance.h"

namespace {
QString crashTr(const char* text)
{
    return QCoreApplication::translate("CrashAnalyzer", text);
}
}  // namespace

DiagnosisResult CrashAnalyzer::detectMissingDependency(const QString& fullText, BaseInstance* instance)
{
    DiagnosisResult res;

    static const QRegularExpression optionalDepPattern(
        R"(Unsupported\s+installed\s+optional\s+dependencies:\s*[\r\n\s]*Mod\s+ID:\s*'([^']+)',\s*Requested\s+by:\s*'([^']+)'(?:,\s*Expected\s+range:\s*'([^']+)',\s*Actual\s+version:\s*'([^']+)')?)",
        QRegularExpression::CaseInsensitiveOption);
    const auto optMatch = optionalDepPattern.match(fullText);

    static const QRegularExpression fmlIssuePattern(
        R"(Mod\s+([a-zA-Z0-9_\-]+)\s+only\s+supports\s+([a-zA-Z0-9_\-]+)\s+([^\n\r]+?)\s+or\s+above[\r\n\s]+Currently,\s+\2\s+is\s+([^\n\r]+))",
        QRegularExpression::CaseInsensitiveOption);
    const auto fmlIssueMatch = fmlIssuePattern.match(fullText);

    static const QRegularExpression depPattern(
        R"(requires\s+(?:version\s+[^\n\r]+of\s+)?mod\s+'([^']+)'|Mod\s+'([^']+)'[^\n\r]*requires\s+(?:version\s+[^\n\r]+of\s+)?'([^']+)'|ModLoadingException:\s*Mod\s+(\S+)\s+requires\s+(\S+)|Could\s+not\s+find\s+required\s+mod:\s*'([^']+)'|Missing\s+or\s+unsupported\s+mandatory\s+dependencies:\s*[\r\n\s]*Mod\s+ID:\s*'([^']+)',\s*Requested\s+by:\s*'([^']+)'|Missing\s+or\s+unsupported\s+mandatory\s+dependencies:\s*([^\n\r]+)|Incompatible\s+mod\s+set\s+found![^\n\r]*[\r\n\s]*-\s*Mod\s+'([^']+)'[^\n\r]*requires\s+([^\n\r]+))",
        QRegularExpression::CaseInsensitiveOption);
    const auto match = depPattern.match(fullText);

    if (!optMatch.hasMatch() && !fmlIssueMatch.hasMatch() && !match.hasMatch() &&
        !fullText.contains(QStringLiteral("Missing mandatory dependencies"), Qt::CaseInsensitive) &&
        !fullText.contains(QStringLiteral("Unsupported installed optional dependencies"), Qt::CaseInsensitive) &&
        !fullText.contains(QStringLiteral("Incompatible mod set found!"), Qt::CaseInsensitive)) {
        return res;
    }

    QString dependentMod;
    QString requiredMod;
    QString expectedRange;
    QString actualVersion;

    if (optMatch.hasMatch()) {
        requiredMod = optMatch.captured(1);
        dependentMod = optMatch.captured(2);
        expectedRange = optMatch.captured(3);
        actualVersion = optMatch.captured(4);
    } else if (fmlIssueMatch.hasMatch()) {
        dependentMod = fmlIssueMatch.captured(1);
        requiredMod = fmlIssueMatch.captured(2);
        expectedRange = fmlIssueMatch.captured(3).trimmed() + QStringLiteral(" or above");
        actualVersion = fmlIssueMatch.captured(4).trimmed();
    } else if (match.hasMatch()) {
        if (!match.captured(2).isEmpty() && !match.captured(3).isEmpty()) {
            dependentMod = match.captured(2);
            requiredMod = match.captured(3);
        } else if (!match.captured(4).isEmpty() && !match.captured(5).isEmpty()) {
            dependentMod = match.captured(4);
            requiredMod = match.captured(5);
        } else if (!match.captured(7).isEmpty() && !match.captured(8).isEmpty()) {
            requiredMod = match.captured(7);
            dependentMod = match.captured(8);
        } else if (!match.captured(10).isEmpty() && !match.captured(11).isEmpty()) {
            dependentMod = match.captured(10);
            requiredMod = match.captured(11);
        } else if (!match.captured(1).isEmpty()) {
            requiredMod = match.captured(1);
        } else if (!match.captured(6).isEmpty()) {
            requiredMod = match.captured(6);
        } else if (!match.captured(9).isEmpty()) {
            requiredMod = match.captured(9);
        }
    }

    res.category = CrashCategory::MissingDependency;

    const QString dependentJar = findModPathByName(instance, dependentMod);
    const QString requiredJar = findModPathByName(instance, requiredMod);

    QString jarInfoText;
    if (!dependentJar.isEmpty() || !requiredJar.isEmpty()) {
        jarInfoText = QStringLiteral("\n\n") + crashTr("Detected Mod JARs:") + QLatin1Char('\n');
        if (!dependentJar.isEmpty()) {
            jarInfoText += QStringLiteral(" - %1 -> %2\n").arg(dependentMod, QFileInfo(dependentJar).fileName());
        }
        if (!requiredJar.isEmpty()) {
            jarInfoText += QStringLiteral(" - %1 -> %2\n").arg(requiredMod, QFileInfo(requiredJar).fileName());
        }
    }

    if (!expectedRange.isEmpty() && !actualVersion.isEmpty()) {
        const QString offendingJar = !dependentJar.isEmpty() ? dependentJar : requiredJar;
        const QString offendingName =
            !offendingJar.isEmpty() ? QFileInfo(offendingJar).fileName() : (!dependentMod.isEmpty() ? dependentMod : requiredMod);

        res.title = crashTr("Mod Dependency Incompatibility");
        res.sourceSummary = crashTr("Mod '%1' -> requires '%2' (%3), installed: %4")
                                .arg(dependentMod, requiredMod, expectedRange, actualVersion);
        res.whySummary = crashTr("The installed version of '%1' (%2) does not satisfy the version range (%3) required by '%4'.")
                             .arg(requiredMod, actualVersion, expectedRange, dependentMod);
        res.description = crashTr("Mod '%1' is incompatible with the installed version of '%2'.").arg(dependentMod, requiredMod) + jarInfoText;
        res.solutionText = crashTr("Update '%1' to a compatible version, or disable/update '%2'.").arg(requiredMod, offendingName);

        if (!offendingJar.isEmpty()) {
            res.fixAction = RecommendedFix::DisableMod;
            res.offendingModPath = offendingJar;
            res.offendingModName = offendingName;
            res.actionButtonText = crashTr("Disable %1").arg(res.offendingModName);
        } else {
            res.fixAction = RecommendedFix::UploadLog;
            res.actionButtonText = crashTr("Upload Log to mclo.gs");
        }
    } else if (!dependentMod.isEmpty() && !requiredMod.isEmpty()) {
        res.title = crashTr("Missing Required Mod (Dependency)");
        res.sourceSummary = crashTr("Mod '%1' requires library/mod '%2'").arg(dependentMod, requiredMod);
        res.whySummary = crashTr("'%1' depends on '%2', which is missing or incompatible in the mods folder.").arg(dependentMod, requiredMod);
        res.description = crashTr("Required dependency '%1' for mod '%2' was not found.").arg(requiredMod, dependentMod) + jarInfoText;
        res.solutionText = crashTr("Install '%1' for your Minecraft version, or remove/disable '%2' if you do not need it.").arg(requiredMod, dependentMod);
        res.fixAction = RecommendedFix::UploadLog;
        res.actionButtonText = crashTr("Upload Log to mclo.gs");
    } else if (!requiredMod.isEmpty()) {
        res.title = crashTr("Missing Required Mod (Dependency)");
        res.sourceSummary = crashTr("Missing dependency: '%1'").arg(requiredMod);
        res.whySummary = crashTr("One of your installed mods requires '%1', but it is not present in the mods folder.").arg(requiredMod);
        res.description = crashTr("The required mod '%1' is missing or incompatible.").arg(requiredMod) + jarInfoText;
        res.solutionText = crashTr("Install '%1' into your mods folder or check recently added mods.").arg(requiredMod);
        res.fixAction = RecommendedFix::UploadLog;
        res.actionButtonText = crashTr("Upload Log to mclo.gs");
    } else {
        res.title = crashTr("Missing Required Mod (Dependency)");
        res.sourceSummary = crashTr("Mod loader dependency check");
        res.whySummary = crashTr("One or more mods are missing mandatory dependencies or have incompatible versions.");
        res.description = crashTr("Minecraft stopped during mod dependency verification.");
        res.solutionText = crashTr("Check Technical Details for the missing mod IDs and install the matching versions into your mods folder.");
        res.fixAction = RecommendedFix::UploadLog;
        res.actionButtonText = crashTr("Upload Log to mclo.gs");
    }

    if (optMatch.hasMatch()) {
        res.technicalDetails = optMatch.captured(0);
    } else if (fmlIssueMatch.hasMatch()) {
        res.technicalDetails = fmlIssueMatch.captured(0);
    } else if (match.hasMatch()) {
        res.technicalDetails = match.captured(0);
    } else {
        res.technicalDetails = QStringLiteral("Missing mandatory dependencies");
    }

    return res;
}

DiagnosisResult CrashAnalyzer::detectModConflict(const QString& fullText, const QString& crashReport, BaseInstance* instance)
{
    DiagnosisResult res;

    static const QRegularExpression suspectedPattern(
        R"(Suspected\s+Mods?:\s*(?:None|[\r\n]+\s*([^\r\n,]+)|([^\r\n,]+)))", QRegularExpression::CaseInsensitiveOption);
    const auto match = suspectedPattern.match(fullText);

    static const QRegularExpression fmlPattern(
        R"(ModLoadingException:\s*Mod\s+['"]?([a-zA-Z0-9_\-]+)['"]?\s+failed\s+to\s+load|Failed\s+to\s+load\s+mod:\s*['"]?([a-zA-Z0-9_\-]+)['"]?|ModIdList:\s*\[([a-zA-Z0-9_\-]+)\]|Error\s+during\s+pre-loading\s+phase:\s*([^\n\r]+))",
        QRegularExpression::CaseInsensitiveOption);
    const auto fmlMatch = fmlPattern.match(fullText);

    static const QRegularExpression mixinPattern(
        R"(Critical\s+injection\s+failure:[^\r\n]*?\s+in\s+([^\s:]+)|MixinApplyError:\s*Mixin\s+\[([^\]]+)\]\s+from\s+phase\s+[^\s]+\s+in\s+config\s+\[([^\]]+)\]|merged\s+from\s+(\S+\.mixins?\.json)|MixinTransformerError:\s*An\s+unexpected\s+critical\s+error\s+was\s+encountered\s+in\s+(\S+)|Error\s+initialising\s+mixin\s+config\s+([^\s,]+)|ClassMetadataNotFoundException:\s*(\S+))",
        QRegularExpression::CaseInsensitiveOption);
    const auto mixinMatch = mixinPattern.match(fullText);

    QString modIdentifier;
    enum class ClueKind { None, Suspected, ModLoading, Mixin, StackTrace };
    ClueKind clueKind = ClueKind::None;

    if (match.hasMatch()) {
        const QString cand = match.captured(1).isEmpty() ? match.captured(2).trimmed() : match.captured(1).trimmed();
        if (cand.compare(QLatin1String("None"), Qt::CaseInsensitive) != 0 && !cand.isEmpty()) {
            modIdentifier = cand;
            clueKind = ClueKind::Suspected;
        }
    }

    if (modIdentifier.isEmpty() && fmlMatch.hasMatch()) {
        if (!fmlMatch.captured(1).isEmpty()) {
            modIdentifier = fmlMatch.captured(1);
        } else if (!fmlMatch.captured(2).isEmpty()) {
            modIdentifier = fmlMatch.captured(2);
        } else if (!fmlMatch.captured(3).isEmpty()) {
            modIdentifier = fmlMatch.captured(3);
        } else {
            modIdentifier = fmlMatch.captured(4);
        }
        clueKind = ClueKind::ModLoading;
    }

    if (modIdentifier.isEmpty() && mixinMatch.hasMatch()) {
        for (int i = mixinMatch.lastCapturedIndex(); i >= 1; --i) {
            if (!mixinMatch.captured(i).isEmpty()) {
                modIdentifier = mixinMatch.captured(i);
                clueKind = ClueKind::Mixin;
                break;
            }
        }
    }

    if (modIdentifier.isEmpty()) {
        if (!crashReport.isEmpty()) {
            modIdentifier = extractModIdentifierFromStackTrace(crashReport);
        }
        if (modIdentifier.isEmpty()) {
            modIdentifier = extractModIdentifierFromStackTrace(fullText);
        }
        if (!modIdentifier.isEmpty()) {
            clueKind = ClueKind::StackTrace;
        }
    }

    if (modIdentifier.isEmpty()) {
        return res;
    }

    const QString modPath = findModPathByName(instance, modIdentifier);

    res.category = CrashCategory::ModConflict;
    res.fixAction = !modPath.isEmpty() ? RecommendedFix::DisableMod : RecommendedFix::None;
    res.offendingModPath = modPath;
    res.offendingModName = !modPath.isEmpty() ? QFileInfo(modPath).fileName() : modIdentifier;

    res.title = crashTr("Mod Crash / Conflict Detected");
    if (clueKind == ClueKind::Mixin) {
        res.sourceSummary = crashTr("Mixin patch from mod: %1").arg(res.offendingModName);
        res.whySummary = crashTr("A Mixin injection failed while patching Minecraft or another mod. This usually happens when two mods patch the same code or a mod targets a different game/loader version.");
    } else if (clueKind == ClueKind::ModLoading) {
        res.sourceSummary = crashTr("Mod initialization phase: %1").arg(res.offendingModName);
        res.whySummary = crashTr("The mod threw an error during startup loading (ModLoadingException), often due to an incompatible version, missing library, or broken config.");
    } else if (clueKind == ClueKind::Suspected) {
        res.sourceSummary = crashTr("Reported by crash report (Suspected Mods): %1").arg(res.offendingModName);
        res.whySummary = crashTr("The crash report identified this mod on the failing call stack.");
    } else {
        res.sourceSummary = crashTr("Mod code on crash stack trace: %1").arg(res.offendingModName);
        res.whySummary = crashTr("An unhandled exception occurred while executing code belonging to this mod.");
    }

    res.description = crashTr("The game crashed in mod '%1'.").arg(res.offendingModName);
    res.solutionText = crashTr("Check whether '%1' matches your Minecraft/loader version, test disabling it temporarily in the Mods tab, or check for conflicts with recently added mods.")
                           .arg(res.offendingModName);

    if (!modPath.isEmpty()) {
        res.actionButtonText = crashTr("Disable %1").arg(res.offendingModName);
    }

    if (match.hasMatch() && !match.captured(0).contains(QLatin1String("None"), Qt::CaseInsensitive)) {
        res.technicalDetails = match.captured(0);
    } else if (fmlMatch.hasMatch()) {
        res.technicalDetails = fmlMatch.captured(0);
    } else if (mixinMatch.hasMatch()) {
        res.technicalDetails = mixinMatch.captured(0);
    } else {
        res.technicalDetails = modIdentifier;
    }

    return res;
}
