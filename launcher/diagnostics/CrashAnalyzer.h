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
#pragma once

#include <QString>
#include <QStringList>

class BaseInstance;

enum class CrashCategory {
    OutOfMemory,
    JavaVersion,
    DuplicateMods,
    MissingDependency,
    GraphicsDriver,
    ModConflict,
    CorruptedConfig,
    Unknown
};

enum class RecommendedFix {
    None,
    IncreaseRam,
    SwitchJava,
    DisableMod,
    DeleteConfig,
    DisableShaders,
    AssignDedicatedGpu,
    UploadLog
};

struct DiagnosisResult {
    CrashCategory category = CrashCategory::Unknown;
    RecommendedFix fixAction = RecommendedFix::None;
    QString title;
    QString sourceSummary;
    QString whySummary;
    QString description;
    QString solutionText;
    QString actionButtonText;

    // Specific fix data
    int suggestedRamMb = 0;
    QString offendingModName;
    QString offendingModPath;
    QString conflictingModPath;
    QString offendingConfigPath;
    QString requiredJavaVersion;
    QString technicalDetails;

    bool hasFix() const { return fixAction != RecommendedFix::None; }
};

class CrashAnalyzer {
   public:
    static DiagnosisResult analyze(BaseInstance* instance,
                                   const QStringList& logLines,
                                   const QString& crashReportContent,
                                   int exitCode);

   private:
    static DiagnosisResult detectOutOfMemory(const QString& fullText, int exitCode, BaseInstance* instance);
    static DiagnosisResult detectJavaVersionMismatch(const QString& fullText, BaseInstance* instance);
    static DiagnosisResult detectDuplicateMods(const QString& fullText, BaseInstance* instance);
    static DiagnosisResult detectMissingDependency(const QString& fullText, BaseInstance* instance);
    static DiagnosisResult detectCorruptedConfig(const QString& fullText, BaseInstance* instance);
    static DiagnosisResult detectModConflict(const QString& fullText, const QString& crashReport, BaseInstance* instance);
    static DiagnosisResult detectGraphicsDriverCrash(const QString& fullText, int exitCode, BaseInstance* instance);
    static DiagnosisResult createFallbackDiagnosis(const QString& fullText, const QString& crashReport, int exitCode);

    static QString extractModIdentifierFromStackTrace(const QString& fullText);
    static QString findModPathByName(BaseInstance* instance, const QString& nameOrModId);
};
