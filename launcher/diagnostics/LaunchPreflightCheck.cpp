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
#include "LaunchPreflightCheck.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <algorithm>

#include "Application.h"
#include "BaseInstance.h"
#include "HardwareInfo.h"
#include "SysInfo.h"
#include "java/JavaInstallList.h"
#include "java/JavaVersion.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/tasks/LocalModParseTask.h"
#include "settings/SettingsObject.h"
#include "tasks/Task.h"

namespace {
QString preflightTr(const char* text)
{
    return QCoreApplication::translate("LaunchPreflightCheck", text);
}

bool instanceHasModLoader(PackProfile* profile)
{
    if (!profile) {
        return false;
    }
    static const QString kLoaderUids[] = {
        QStringLiteral("net.neoforge"),
        QStringLiteral("net.minecraftforge"),
        QStringLiteral("net.fabricmc.fabric-loader"),
        QStringLiteral("org.quiltmc.quilt-loader"),
    };
    for (const QString& uid : kLoaderUids) {
        if (!profile->getComponentVersion(uid).isEmpty()) {
            return true;
        }
    }
    return false;
}

int parseConfiguredJavaMajor(SettingsObject* settings, bool& pathMissing)
{
    pathMissing = false;
    if (!settings) {
        return 0;
    }
    const QString javaPath = settings->get("JavaPath").toString().trimmed();
    if (!javaPath.isEmpty()) {
        const bool existsOnDisk = QFileInfo::exists(javaPath) || !QStandardPaths::findExecutable(javaPath).isEmpty();
        pathMissing = !existsOnDisk;
    }
    const QString versionStr = settings->get("JavaVersion").toString().trimmed();
    if (!versionStr.isEmpty()) {
        return JavaVersion(versionStr).major();
    }
    return 0;
}
}  // namespace

bool PreflightReport::hasFixableIssues() const
{
    for (const PreflightIssue& issue : issues) {
        if (issue.fixType != PreflightFixType::None) {
            return true;
        }
    }
    return false;
}

namespace LaunchPreflightCheck {

int requiredJavaMajorForMinecraft(const QString& mcVersion)
{
    static const QRegularExpression semverRe(QStringLiteral(R"(^\s*(\d+)\.(\d+)(?:\.(\d+))?)"));
    const auto match = semverRe.match(mcVersion);
    if (!match.hasMatch()) {
        return 0;
    }

    const int major = match.captured(1).toInt();
    const int minor = match.captured(2).toInt();
    const int patch = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();

    if (major > 1 || (major == 1 && (minor > 20 || (minor == 20 && patch >= 5)))) {
        return 21;
    }
    if (major == 1 && minor >= 18) {
        return 17;
    }
    if (major == 1 && minor == 17) {
        return 16;
    }
    if (major >= 1) {
        return 8;
    }
    return 0;
}

JavaInstallPtr findBestInstalledJava(JavaInstallList* list, int minMajor, bool requireExistingBinary, bool loadIfNeeded)
{
    if (!list || minMajor <= 0) {
        return {};
    }

    QPointer<JavaInstallList> guardedList(list);
    if (loadIfNeeded && !guardedList->isLoaded()) {
        if (Task::Ptr loadTask = guardedList->getLoadTask(); loadTask && loadTask->isRunning()) {
            QEventLoop loop;
            QTimer timer;
            timer.setSingleShot(true);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            QObject::connect(loadTask.get(), &Task::finished, &loop, &QEventLoop::quit);
            timer.start(3500);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
        }
    }

    if (!guardedList) {
        return {};
    }

    JavaInstallPtr best;
    const int total = guardedList->count();
    for (int i = 0; i < total; ++i) {
        auto candidate = std::dynamic_pointer_cast<JavaInstall>(guardedList->at(i));
        if (!candidate || candidate->id.major() < minMajor) {
            continue;
        }
        if (requireExistingBinary && (candidate->path.isEmpty() || !QFileInfo::exists(candidate->path))) {
            continue;
        }
        if (!best) {
            best = candidate;
            continue;
        }
        if (candidate->is_64bit != best->is_64bit) {
            if (candidate->is_64bit) {
                best = candidate;
            }
            continue;
        }
        // Prefer exact/lowest compatible major first so e.g. Java 21 is chosen cleanly for 1.21
        if (candidate->id.major() != best->id.major()) {
            if (candidate->id.major() < best->id.major()) {
                best = candidate;
            }
            continue;
        }
        if (candidate->id > best->id) {
            best = candidate;
        }
    }
    return best;
}

std::optional<PreflightIssue> checkJavaRequirement(const QString& mcVersion,
                                                   int configuredJavaMajor,
                                                   bool javaPathExists,
                                                   bool automaticJava,
                                                   bool ignoreCompat,
                                                   const JavaInstallPtr& suggestedJava)
{
    if (automaticJava) {
        return std::nullopt;
    }

    const int requiredMajor = requiredJavaMajorForMinecraft(mcVersion);
    const bool versionTooOld = (requiredMajor > 0 && configuredJavaMajor > 0 && configuredJavaMajor < requiredMajor && !ignoreCompat);
    const bool binaryMissing = !javaPathExists;

    if (!binaryMissing && !versionTooOld) {
        return std::nullopt;
    }

    PreflightIssue issue;
    issue.severity = binaryMissing ? PreflightSeverity::Critical : PreflightSeverity::Warning;
    issue.title = binaryMissing ? preflightTr("Configured Java executable was not found")
                                : preflightTr("Java %1+ is required for Minecraft %2").arg(requiredMajor).arg(mcVersion);

    if (binaryMissing) {
        issue.description = preflightTr("The selected Java binary is missing or inaccessible on disk.");
    } else {
        issue.description = preflightTr("Current runtime is Java %1, while Minecraft %2 expects at least Java %3.")
                                .arg(configuredJavaMajor)
                                .arg(mcVersion)
                                .arg(requiredMajor);
    }

    if (suggestedJava) {
        issue.fixType = PreflightFixType::SwitchJava;
        issue.suggestedJavaPath = suggestedJava->path;
        issue.suggestedJavaVersion = suggestedJava->id.toString();
        issue.fixSummary = preflightTr("Switch to detected Java %1").arg(issue.suggestedJavaVersion);
    }
    return issue;
}

std::optional<PreflightIssue> checkMemoryAllocation(int maxMemMb,
                                                    uint64_t totalRamMiB,
                                                    bool hasModLoader,
                                                    int recommendedMaxMb)
{
    if (totalRamMiB > 0 && maxMemMb > 0) {
        const uint64_t safeCeiling = std::max<uint64_t>(1024, (totalRamMiB * 85) / 100);
        if (static_cast<uint64_t>(maxMemMb) > safeCeiling) {
            const int clamped = std::max(1024, std::min(recommendedMaxMb > 0 ? recommendedMaxMb : 4096, static_cast<int>(safeCeiling)));
            PreflightIssue issue;
            issue.severity = static_cast<uint64_t>(maxMemMb) > totalRamMiB ? PreflightSeverity::Critical : PreflightSeverity::Warning;
            issue.fixType = PreflightFixType::ClampRam;
            issue.suggestedRamMb = clamped;
            issue.title = preflightTr("Allocated RAM (%1 MB) is too high for system memory").arg(maxMemMb);
            issue.description = preflightTr("System has %1 MB RAM total. High -Xmx leaves too little memory for the OS and GPU.").arg(totalRamMiB);
            issue.fixSummary = preflightTr("Set maximum RAM to %1 MB").arg(clamped);
            return issue;
        }
    }

    if (hasModLoader && maxMemMb > 0 && maxMemMb < 1024) {
        int bumped = 2048;
        if (totalRamMiB > 0) {
            bumped = std::min<int>(2048, static_cast<int>(std::max<uint64_t>(1024, (totalRamMiB * 60) / 100)));
        }
        if (bumped > maxMemMb) {
            PreflightIssue issue;
            issue.severity = PreflightSeverity::Warning;
            issue.fixType = PreflightFixType::ClampRam;
            issue.suggestedRamMb = bumped;
            issue.title = preflightTr("Low RAM limit (%1 MB) for modded Minecraft").arg(maxMemMb);
            issue.description = preflightTr("Modded instances typically crash with OutOfMemoryError below 1024 MB.");
            issue.fixSummary = preflightTr("Increase maximum RAM to %1 MB").arg(bumped);
            return issue;
        }
    }

    return std::nullopt;
}

QString normalizeModSlug(const QString& fileName)
{
    QString stem = fileName;
    if (stem.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) {
        stem.chop(4);
    }
    static const QRegularExpression versionBoundary(QStringLiteral(R"((?:[-_+](?:v?\d[\d.]*|mc\d[\d.]*|build\.\d+)).*$)"),
                                                    QRegularExpression::CaseInsensitiveOption);
    stem.remove(versionBoundary);
    return stem.trimmed().toLower();
}

QList<PreflightIssue> detectDuplicateModsInDirectory(const QString& modsDir)
{
    QList<PreflightIssue> out;
    if (modsDir.isEmpty()) {
        return out;
    }
    QDir dir(modsDir);
    if (!dir.exists()) {
        return out;
    }

    const QFileInfoList jars = dir.entryInfoList({ QStringLiteral("*.jar") }, QDir::Files | QDir::Readable, QDir::Name);
    if (jars.size() < 2) {
        return out;
    }

    QHash<QString, QList<QFileInfo>> bySlug;
    for (const QFileInfo& fi : jars) {
        const QString slug = normalizeModSlug(fi.fileName());
        if (!slug.isEmpty()) {
            bySlug[slug].append(fi);
        }
    }

    for (auto it = bySlug.cbegin(); it != bySlug.cend(); ++it) {
        if (it->size() < 2) {
            continue;
        }

        // Only inspect archive metadata on filename slug collisions to keep pre-flight fast.
        QHash<QString, QFileInfoList> groups;
        for (const QFileInfo& fi : *it) {
            Mod mod(fi);
            ModUtils::process(mod, ModUtils::ProcessingLevel::BasicInfoOnly);
            const QString id = mod.details().mod_id.trimmed().toLower();
            const QString groupKey = id.isEmpty() ? QStringLiteral("__slug__:%1").arg(it.key()) : QStringLiteral("id:%1").arg(id);
            groups[groupKey].append(fi);
        }

        for (auto git = groups.cbegin(); git != groups.cend(); ++git) {
            QFileInfoList list = git.value();
            if (list.size() < 2) {
                continue;
            }
            // Keep the newest copy enabled; flag older duplicates for disabling.
            std::sort(list.begin(), list.end(), [](const QFileInfo& a, const QFileInfo& b) {
                if (a.lastModified() != b.lastModified()) {
                    return a.lastModified() > b.lastModified();
                }
                return a.fileName() > b.fileName();
            });

            const QFileInfo& keep = list.first();
            for (int i = 1; i < list.size(); ++i) {
                const QFileInfo& dup = list.at(i);
                PreflightIssue issue;
                issue.severity = PreflightSeverity::Warning;
                issue.fixType = PreflightFixType::DisableDuplicateMod;
                issue.duplicateJarToDisable = dup.absoluteFilePath();
                issue.duplicateDisplayName = dup.fileName();
                issue.title = preflightTr("Duplicate mod file: %1").arg(dup.fileName());
                issue.description = preflightTr("Conflicts with %1 in the mods folder and may prevent startup.").arg(keep.fileName());
                issue.fixSummary = preflightTr("Disable duplicate (%1.disabled)").arg(dup.fileName());
                out.append(issue);
            }
        }
    }

    return out;
}

PreflightReport analyze(BaseInstance* instance)
{
    PreflightReport report;
    if (!instance || !instance->settings()) {
        return report;
    }

    auto* settings = instance->settings();
    auto* mcInst = dynamic_cast<MinecraftInstance*>(instance);
    PackProfile* profile = mcInst ? mcInst->getPackProfile() : nullptr;

    const QString mcVersion = profile ? profile->getComponentVersion(QStringLiteral("net.minecraft")) : QString();
    const bool hasLoader = instanceHasModLoader(profile);

    bool javaPathMissing = false;
    const int configuredMajor = parseConfiguredJavaMajor(settings, javaPathMissing);
    const bool overrideJava = settings->get("OverrideJavaLocation").toBool();
    const bool globalAutoJavaSwitch =
        (APPLICATION_DYN != nullptr && APPLICATION_DYN->settings())
            ? APPLICATION_DYN->settings()->get("AutomaticJavaSwitch").toBool()
            : false;
    const bool autoJavaSwitchActive = globalAutoJavaSwitch && (!overrideJava || javaPathMissing);

    const bool ignoreCompat = settings->get("IgnoreJavaCompatibility").toBool();
    const int requiredMajor = requiredJavaMajorForMinecraft(mcVersion);
    JavaInstallPtr suggestedJava;
    if (APPLICATION_DYN != nullptr && APPLICATION_DYN->javalist()) {
        const bool needsJavaSuggestion =
            !autoJavaSwitchActive && (javaPathMissing || (requiredMajor > 0 && configuredMajor > 0 && configuredMajor < requiredMajor && !ignoreCompat));
        suggestedJava = findBestInstalledJava(APPLICATION_DYN->javalist(),
                                              requiredMajor > 0 ? requiredMajor : 17,
                                              true,
                                              needsJavaSuggestion);
    }
    if (auto javaIssue =
            checkJavaRequirement(mcVersion, configuredMajor, !javaPathMissing, autoJavaSwitchActive, ignoreCompat, suggestedJava)) {
        report.issues.append(*javaIssue);
    }

    const int maxMemMb = settings->get("MaxMemAlloc").toInt();
    if (auto memIssue = checkMemoryAllocation(maxMemMb, HardwareInfo::totalRamMiB(), hasLoader, SysInfo::defaultMaxJvmMem())) {
        report.issues.append(*memIssue);
    }

    report.issues.append(detectDuplicateModsInDirectory(instance->modsRoot()));
    return report;
}

bool applyIssueFix(BaseInstance* instance, const PreflightIssue& issue)
{
    switch (issue.fixType) {
        case PreflightFixType::ClampRam: {
            if (!instance || !instance->settings() || issue.suggestedRamMb <= 0) {
                return false;
            }
            instance->settings()->set("OverrideMemory", true);
            instance->settings()->set("MaxMemAlloc", issue.suggestedRamMb);
            if (instance->settings()->contains("MinMemAlloc") &&
                instance->settings()->get("MinMemAlloc").toInt() > issue.suggestedRamMb) {
                instance->settings()->set("MinMemAlloc", issue.suggestedRamMb);
            }
            instance->saveNow();
            return true;
        }
        case PreflightFixType::SwitchJava: {
            if (!instance || !instance->settings() || issue.suggestedJavaPath.isEmpty()) {
                return false;
            }
            instance->settings()->set("OverrideJavaLocation", true);
            instance->settings()->set("JavaPath", issue.suggestedJavaPath);
            instance->saveNow();
            return true;
        }
        case PreflightFixType::DisableDuplicateMod: {
            if (issue.duplicateJarToDisable.isEmpty() || !QFile::exists(issue.duplicateJarToDisable)) {
                return false;
            }
            QString disabledPath = issue.duplicateJarToDisable + QStringLiteral(".disabled");
            for (int suffix = 1; QFile::exists(disabledPath) && suffix < 1000; ++suffix) {
                disabledPath = QStringLiteral("%1.%2.disabled").arg(issue.duplicateJarToDisable).arg(suffix);
            }
            if (QFile::exists(disabledPath)) {
                return false;
            }
            return QFile::rename(issue.duplicateJarToDisable, disabledPath);
        }
        case PreflightFixType::None:
            return false;
    }
    return false;
}

int applyAllFixes(BaseInstance* instance, const PreflightReport& report)
{
    int applied = 0;
    for (const PreflightIssue& issue : report.issues) {
        if (applyIssueFix(instance, issue)) {
            ++applied;
        }
    }
    return applied;
}

}  // namespace LaunchPreflightCheck
