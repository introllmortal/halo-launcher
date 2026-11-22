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
#include <QSet>
#include <QString>
#include <QStringList>

struct ManagedFileRecord {
    QString path;
    qint64 size = 0;
    quint32 crc32 = 0;
    QString sha1;
};

struct ManagedFilesManifest {
    QString version;
    QList<ManagedFileRecord> files;
};

namespace ManagedFilesIndex {

QString indexFilePath(const QString& instanceRoot);

ManagedFilesManifest load(const QString& instanceRoot);
bool save(const QString& instanceRoot, const ManagedFilesManifest& manifest);

/**
 * Returns managed paths present in previousManifest that are absent from nextPaths
 * and pass path safety checks (personal player files are never included).
 */
QStringList computeRemovedPaths(const ManagedFilesManifest& previousManifest, const QSet<QString>& nextPaths);

/**
 * Scans managed directories inside gameRoot to build an index after a full archive import.
 */
ManagedFilesManifest scanGameRoot(const QString& gameRoot, const QString& version);

/** Returns true if rootDir or any component along relativePath is a symbolic link. */
bool hasSymlinkInRelativeChain(const QString& rootDir, const QString& relativePath);

}  // namespace ManagedFilesIndex
