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
#include <QRegularExpression>

#include "Application.h"
#include "BaseInstance.h"
#include "GpuSanityChecker.h"
#include "settings/SettingsObject.h"

namespace {
QString crashTr(const char* text)
{
    return QCoreApplication::translate("CrashAnalyzer", text);
}
}  // namespace

DiagnosisResult CrashAnalyzer::detectGraphicsDriverCrash(const QString& fullText, int exitCode, BaseInstance* instance)
{
    DiagnosisResult res;
    bool isGpuCrash = false;
    QString reason;
    bool isNativeDriverDll = false;
    bool isNvidiaDll = false;
    bool isIntelDll = false;

    if (fullText.contains(QStringLiteral("GLFW error 65542"), Qt::CaseInsensitive) ||
        fullText.contains(QStringLiteral("WGL: The driver does not appear to support OpenGL"), Qt::CaseInsensitive)) {
        isGpuCrash = true;
        reason = QStringLiteral("GLFW error 65542 (Missing OpenGL acceleration)");
    } else if (fullText.contains(QStringLiteral("Pixel format not accelerated"), Qt::CaseInsensitive)) {
        isGpuCrash = true;
        reason = QStringLiteral("Pixel format not accelerated");
    } else if (fullText.contains(QStringLiteral("# Problematic frame:"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("EXCEPTION_ACCESS_VIOLATION"), Qt::CaseInsensitive)) {
        static const QRegularExpression driverDllPattern(
            R"((?:#\s*Problematic\s+frame:[\s\S]{0,100}?(?:#\s*[A-Z]\s+\[|\b)|(?:EXCEPTION_ACCESS_VIOLATION[\s\S]{0,200}?\b))([a-zA-Z0-9_\-]+\.dll))",
            QRegularExpression::CaseInsensitiveOption);
        const auto match = driverDllPattern.match(fullText);
        const QString dll = match.hasMatch() ? match.captured(1).toLower() : QString();

        if (dll.contains(QLatin1String("nvoglv64")) ||
            (fullText.contains(QStringLiteral("nvoglv64.dll"), Qt::CaseInsensitive) &&
             fullText.contains(QStringLiteral("# Problematic frame:"), Qt::CaseInsensitive))) {
            isGpuCrash = true;
            isNativeDriverDll = true;
            isNvidiaDll = true;
            reason = QStringLiteral("NVIDIA OpenGL driver crashed (nvoglv64.dll)");
        } else if (dll.contains(QLatin1String("atio6axx")) ||
                   (fullText.contains(QStringLiteral("atio6axx.dll"), Qt::CaseInsensitive) &&
                    fullText.contains(QStringLiteral("# Problematic frame:"), Qt::CaseInsensitive))) {
            isGpuCrash = true;
            isNativeDriverDll = true;
            reason = QStringLiteral("AMD OpenGL driver crashed (atio6axx.dll)");
        } else if (dll.contains(QLatin1String("ig9ic64")) || dll.contains(QLatin1String("ig7ic64")) ||
                   dll.contains(QLatin1String("ig4icd64")) ||
                   ((fullText.contains(QStringLiteral("ig9ic64.dll"), Qt::CaseInsensitive) ||
                     fullText.contains(QStringLiteral("ig7ic64.dll"), Qt::CaseInsensitive) ||
                     fullText.contains(QStringLiteral("ig4icd64.dll"), Qt::CaseInsensitive)) &&
                    fullText.contains(QStringLiteral("# Problematic frame:"), Qt::CaseInsensitive))) {
            isGpuCrash = true;
            isNativeDriverDll = true;
            isIntelDll = true;
            reason = QStringLiteral("Intel OpenGL driver crashed (") + (dll.isEmpty() ? QStringLiteral("Intel HD Graphics DLL") : dll) +
                     QLatin1Char(')');
        }
    } else if (fullText.contains(QStringLiteral("org.lwjgl.opengl.OpenGLException"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("GL_OUT_OF_MEMORY"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("OpenGLException: Out of memory"), Qt::CaseInsensitive)) {
        isGpuCrash = true;
        reason = QStringLiteral("OpenGL Exception: Out of GPU Video Memory (VRAM)");
    } else if (fullText.contains(QStringLiteral("Shader compilation failed"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("Failed to compile shader"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("Program linking failed"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("Failed to compile fragment shader"), Qt::CaseInsensitive) ||
               fullText.contains(QStringLiteral("Failed to compile vertex shader"), Qt::CaseInsensitive)) {
        isGpuCrash = true;
        reason = QStringLiteral("Shader compilation or linking failure");
    }

    if (!isGpuCrash) {
        return res;
    }

    res.category = CrashCategory::GraphicsDriver;

    QString javaPath;
    if (instance && instance->settings() && instance->settings()->get(QStringLiteral("OverrideJavaLocation")).toBool()) {
        javaPath = instance->settings()->get(QStringLiteral("JavaPath")).toString();
    }
    if (javaPath.isEmpty() && APPLICATION && APPLICATION->settings()) {
        javaPath = APPLICATION->settings()->get(QStringLiteral("JavaPath")).toString();
    }

    const bool dualGpu = GpuSanityChecker::isDualGpu();
    const bool highPerfEnforced = GpuSanityChecker::isHighPerformanceEnforced(javaPath);

    if (dualGpu && !highPerfEnforced && (!isNativeDriverDll || isIntelDll)) {
        res.fixAction = RecommendedFix::AssignDedicatedGpu;
        const QString dgpu = GpuSanityChecker::discreteGpuNames().join(QStringLiteral(", "));
        const QString igpu = GpuSanityChecker::integratedGpuNames().join(QStringLiteral(", "));

        res.title = crashTr("Hybrid GPU Detected (Integrated Graphics in Use)");
        res.sourceSummary = crashTr("Graphics / OpenGL driver (%1)").arg(reason);
        res.whySummary = crashTr("Your system has both dedicated (%1) and integrated (%2) graphics, and Java likely ran on the integrated GPU.")
                             .arg(dgpu.isEmpty() ? QStringLiteral("dGPU") : dgpu, igpu.isEmpty() ? QStringLiteral("iGPU") : igpu);
        res.description = crashTr("Minecraft failed to initialize or render on the selected graphics adapter.");
        res.solutionText = crashTr("Assign Java to the high-performance dedicated GPU in Windows Graphics Settings, or update your GPU drivers.");
        res.actionButtonText = crashTr("Enforce Dedicated GPU");
        res.technicalDetails = QStringLiteral("GPU Reason: %1\nDiscrete GPU: %2\nIntegrated GPU: %3\nExit Code: %4")
                                   .arg(reason, dgpu, igpu)
                                   .arg(exitCode);
        return res;
    }

    if (reason.contains(QLatin1String("Shader"), Qt::CaseInsensitive)) {
        res.fixAction = RecommendedFix::DisableShaders;
        res.title = crashTr("Shader Compilation Failure");
        res.sourceSummary = crashTr("Shader pack / Iris / Oculus (%1)").arg(reason);
        res.whySummary = crashTr("The active shader pack failed to compile or link on your GPU driver or shader mod version.");
        res.description = crashTr("Minecraft crashed while compiling shaders.");
        res.solutionText = crashTr("Disable or switch the active shader pack in the Shader Packs tab, or update Iris/Oculus and your GPU driver.");
        res.actionButtonText = crashTr("Disable Shaders");
        res.technicalDetails = QStringLiteral("Shader Error: %1\nExit Code: %2").arg(reason).arg(exitCode);
        return res;
    }

    res.fixAction = RecommendedFix::None;
    res.title = crashTr("Graphics Driver Error");
    res.sourceSummary = crashTr("Graphics driver / OpenGL (%1)").arg(reason);
    if (isNvidiaDll) {
        res.whySummary = crashTr("The crash happened inside the native NVIDIA OpenGL driver (nvoglv64.dll), usually due to a driver bug, shader issue, or incompatible graphics mod.");
        res.description = crashTr("Minecraft crashed inside the NVIDIA graphics driver.");
        res.solutionText = crashTr("Update or cleanly reinstall NVIDIA drivers, check active shaders, and verify rendering mods (Sodium/Iris).");
    } else {
        res.whySummary = crashTr("The graphics driver or OpenGL layer encountered a fatal error (%1).").arg(reason);
        res.description = crashTr("Minecraft crashed due to a graphics driver or OpenGL error.");
        res.solutionText = crashTr("Update your graphics card drivers, check that your GPU supports OpenGL 4.5+, and test without shaders.");
    }
    res.technicalDetails = QStringLiteral("GPU Error: %1\nExit Code: %2").arg(reason).arg(exitCode);
    return res;
}
