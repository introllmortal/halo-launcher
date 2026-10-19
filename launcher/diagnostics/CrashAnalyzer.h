// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <memory>

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
    UploadLog
};

struct DiagnosisResult {
    CrashCategory category = CrashCategory::Unknown;
    RecommendedFix fixAction = RecommendedFix::None;
    QString title;
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
    static DiagnosisResult analyze(BaseInstance* instance, const QStringList& logLines, const QString& crashReportContent, int exitCode);

private:
    static DiagnosisResult detectOutOfMemory(const QString& fullText, int exitCode, BaseInstance* instance);
    static DiagnosisResult detectJavaVersionMismatch(const QString& fullText, BaseInstance* instance);
    static DiagnosisResult detectDuplicateMods(const QString& fullText, BaseInstance* instance);
    static DiagnosisResult detectMissingDependency(const QString& fullText, BaseInstance* instance);
    static DiagnosisResult detectGraphicsDriverCrash(const QString& fullText, int exitCode);
    static DiagnosisResult detectModConflict(const QString& fullText, const QString& crashReport, BaseInstance* instance);
    static DiagnosisResult detectCorruptedConfig(const QString& fullText, BaseInstance* instance);
    static DiagnosisResult createFallbackDiagnosis(const QString& fullText, const QString& crashReport, int exitCode);

    static QString findModPathByName(BaseInstance* instance, const QString& nameOrModId);
};
