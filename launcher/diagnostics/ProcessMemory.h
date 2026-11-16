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

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <optional>

namespace ProcessMemory {

/// Queries working-set / RSS memory usage in bytes for a live PID, if supported and accessible.
std::optional<qint64> queryProcessWorkingSetBytes(qint64 pid);

/// Parses Linux /proc/<pid>/status VmRSS line into bytes.
std::optional<qint64> parseLinuxProcStatusVmRssBytes(const QByteArray& statusContent);

/// Formats a byte count into compact MB/GB display text.
QString formatMemorySize(qint64 bytes);

}  // namespace ProcessMemory
