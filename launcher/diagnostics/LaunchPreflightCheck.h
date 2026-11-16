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

#include <QList>
#include <QString>
#include <cstdint>
#include <optional>

#include "java/JavaInstall.h"

class BaseInstance;
class JavaInstallList;

enum class PreflightSeverity {
    Warning,
    Critical,
};

enum class PreflightFixType {
    None,
    ClampRam,
    SwitchJava,
    DisableDuplicateMod,
};

struct PreflightIssue {
    PreflightSeverity severity = PreflightSeverity::Warning;
    PreflightFixType fixType = PreflightFixType::None;
    QString title;
    QString description;
    QString fixSummary;

    int suggestedRamMb = 0;
    QString suggestedJavaPath;
    QString suggestedJavaVersion;
    QString duplicateJarToDisable;
    QString duplicateDisplayName;
};

struct PreflightReport {
    QList<PreflightIssue> issues;

    bool isEmpty() const { return issues.isEmpty(); }
    bool hasFixableIssues() const;
};

namespace LaunchPreflightCheck {

/// Returns the minimum Java major version required by a Minecraft release version (0 if unknown).
int requiredJavaMajorForMinecraft(const QString& mcVersion);

/// Selects the highest-priority valid 64-bit (when available) installed Java meeting minMajor.
/// Triggers and awaits loading if loadIfNeeded is true and the list is not loaded yet.
JavaInstallPtr findBestInstalledJava(JavaInstallList* list,
                                     int minMajor,
                                     bool requireExistingBinary = true,
                                     bool loadIfNeeded = false);

/// Pure Java compatibility check for deterministic unit testing.
std::optional<PreflightIssue> checkJavaRequirement(const QString& mcVersion,
                                                   int configuredJavaMajor,
                                                   bool javaPathExists,
                                                   bool automaticJava,
                                                   bool ignoreCompat,
                                                   const JavaInstallPtr& suggestedJava);

/// Pure RAM budget sanity check for deterministic unit testing.
std::optional<PreflightIssue> checkMemoryAllocation(int maxMemMb,
                                                    uint64_t totalRamMiB,
                                                    bool hasModLoader,
                                                    int recommendedMaxMb);

/// Extracts a normalized base mod name slug from a mod jar filename (for candidate duplicate grouping).
QString normalizeModSlug(const QString& fileName);

/// Scans a mods directory for duplicate enabled mod JARs (confirming mod_id when available).
QList<PreflightIssue> detectDuplicateModsInDirectory(const QString& modsDir);

/// Runs all fast local pre-flight checks on an instance before launching.
PreflightReport analyze(BaseInstance* instance);

/// Applies a single pre-flight auto-fix to the instance or its mods directory.
bool applyIssueFix(BaseInstance* instance, const PreflightIssue& issue);

/// Applies all fixable issues in a report and saves instance settings; returns applied fix count.
int applyAllFixes(BaseInstance* instance, const PreflightReport& report);

}  // namespace LaunchPreflightCheck
