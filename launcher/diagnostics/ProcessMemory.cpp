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
#include "ProcessMemory.h"

#include <QFile>
#include <QList>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <psapi.h>
#endif

namespace ProcessMemory {

std::optional<qint64> parseLinuxProcStatusVmRssBytes(const QByteArray& statusContent)
{
    const QList<QByteArray> lines = statusContent.split('\n');
    for (const QByteArray& rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (!line.startsWith("VmRSS:")) {
            continue;
        }
        const QByteArray rest = line.mid(6).trimmed();
        const int spaceIdx = rest.indexOf(' ');
        const QByteArray numberToken = (spaceIdx > 0) ? rest.left(spaceIdx) : rest;
        bool ok = false;
        const qint64 kb = numberToken.toLongLong(&ok);
        if (!ok || kb < 0) {
            return std::nullopt;
        }
        return kb * 1024LL;
    }
    return std::nullopt;
}

std::optional<qint64> queryProcessWorkingSetBytes(qint64 pid)
{
    if (pid <= 0) {
        return std::nullopt;
    }
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, static_cast<DWORD>(pid));
    if (!handle) {
        return std::nullopt;
    }
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    std::optional<qint64> result;
    if (GetProcessMemoryInfo(handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        result = static_cast<qint64>(pmc.WorkingSetSize);
    }
    CloseHandle(handle);
    return result;
#elif defined(Q_OS_LINUX)
    QFile statusFile(QStringLiteral("/proc/%1/status").arg(pid));
    if (!statusFile.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    return parseLinuxProcStatusVmRssBytes(statusFile.readAll());
#else
    return std::nullopt;
#endif
}

QString formatMemorySize(qint64 bytes)
{
    if (bytes < 0) {
        return {};
    }
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    if (mb >= 1024.0) {
        return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
    }
    return QStringLiteral("%1 MB").arg(qRound(mb));
}

}  // namespace ProcessMemory
