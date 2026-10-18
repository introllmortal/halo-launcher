
// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 r58Playz <r58playz@gmail.com>
 *  Copyright (C) 2024 timoreo <contact@timoreo.fr>
 *  Copyright (C) 2024 Trial97 <alexandru.tripon97@gmail.com>
 *  Copyright (C) 2025 TheKodeToad <TheKodeToad@proton.me>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include <QString>

#include "HardwareInfo.h"

#ifdef Q_OS_MACOS
#include <sys/sysctl.h>

bool rosettaDetect()
{
    int ret = 0;
    size_t size = sizeof(ret);
    if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) == -1) {
        return false;
    }
    return ret == 1;
}
#endif

namespace SysInfo {
QString currentSystem()
{
#if defined(Q_OS_LINUX)
    return "linux";
#elif defined(Q_OS_MACOS)
    return "osx";
#elif defined(Q_OS_WINDOWS)
    return "windows";
#elif defined(Q_OS_FREEBSD)
    return "freebsd";
#elif defined(Q_OS_OPENBSD)
    return "openbsd";
#else
    return "unknown";
#endif
}

QString useQTForArch()
{
#if defined(Q_OS_MACOS) && !defined(Q_PROCESSOR_ARM)
    if (rosettaDetect()) {
        return "arm64";
    } else {
        return "x86_64";
    }
#endif
    return QSysInfo::currentCpuArchitecture();
}

int defaultMaxJvmMem()
{
    const auto totalRam = HardwareInfo::totalRamMiB();
    if (totalRam == 0) {
        return 4096;
    }

    // Adaptive RAM allocation based on player's physical hardware:
    if (totalRam <= 4096) {
        // <= 4 GB system: allocate 2048 MB to Minecraft, leave ~2 GB for OS
        return 2048;
    } else if (totalRam <= 8192) {
        // 6-8 GB system: allocate 4096-5120 MB, reserve at least 3 GB for Windows/background
        return static_cast<int>(std::max<uint64_t>(3072, totalRam - 3072));
    } else if (totalRam <= 16384) {
        // 12-16 GB system: allocate 8192 MB (sweet spot for heavy 400+ modpacks)
        return 8192;
    } else if (totalRam <= 32768) {
        // 24-32 GB system: allocate 10240 MB (ideal for heavy shaders and large draw distance)
        return 10240;
    } else {
        // 48-64+ GB system: cap at 14336 MB to prevent GC pause spikes on huge heaps
        return 14336;
    }
}

QString getSupportedJavaArchitecture()
{
    auto sys = currentSystem();
    auto arch = useQTForArch();
    if (sys == "windows") {
        if (arch == "x86_64")
            return "windows-x64";
        if (arch == "i386")
            return "windows-x86";
        // Unknown, maybe arm, appending arch
        return "windows-" + arch;
    }
    if (sys == "osx") {
        if (arch == "arm64")
            return "mac-os-arm64";
        if (arch.contains("64"))
            return "mac-os-x64";
        if (arch.contains("86"))
            return "mac-os-x86";
        // Unknown, maybe something new, appending arch
        return "mac-os-" + arch;
    } else if (sys == "linux") {
        if (arch == "x86_64")
            return "linux-x64";
        if (arch == "i386")
            return "linux-x86";
        // will work for arm32 arm(64)
        return "linux-" + arch;
    }
    return {};
}
}  // namespace SysInfo
