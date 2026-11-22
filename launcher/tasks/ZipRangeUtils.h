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
#include <QList>
#include <QString>

struct ZipEocdInfo {
    bool valid = false;
    quint64 cdOffset = 0;
    quint64 cdSize = 0;
    quint64 entryCount = 0;
};

struct ZipRemoteEntry {
    QString rawName;
    QString normalizedPath;
    quint32 crc32 = 0;
    quint64 compressedSize = 0;
    quint64 uncompressedSize = 0;
    quint16 method = 0;
    quint64 localHeaderOffset = 0;
};

struct CoalescedByteRange {
    quint64 start = 0;
    quint64 end = 0;  // inclusive byte offset
    QList<int> entryIndices;
};

namespace ZipRangeUtils {

/** Computes IEEE 802.3 CRC-32 and byte length of a local file by streaming 64 KiB chunks. */
bool computeFileCrc32(const QString& filePath, quint32& outCrc32, qint64& outSize);

/** Computes CRC-32 of an in-memory buffer. */
quint32 computeBytesCrc32(const QByteArray& buffer);

/** Parses standard EOCD or ZIP64 EOCD from the tail slice of a remote ZIP archive. */
ZipEocdInfo parseEocd(const QByteArray& tailBytes, quint64 tailStartOffset, quint64 totalZipSize);

/**
 * Strips known instance/modpack root prefixes (.minecraft/, minecraft/, overrides/, or <root>/.minecraft/)
 * and returns a clean relative path under gameRoot, or empty if outside gameRoot.
 */
QString normalizePackRelativePath(const QString& rawZipPath);

/** Parses ZIP Central Directory bytes into normalized entries (skips directories and non-game-root files). */
QList<ZipRemoteEntry> parseCentralDirectory(const QByteArray& cdBytes, bool* outOk = nullptr);

/**
 * Computes an upper-bound inclusive byte range for an entry's local header + compressed payload.
 */
quint64 entryMaxEndOffset(const ZipRemoteEntry& entry, quint64 cdOffset);

/**
 * Groups entries by localHeaderOffset into coalesced HTTP byte ranges to minimize request count.
 */
QList<CoalescedByteRange> coalesceByteRanges(const QList<ZipRemoteEntry>& entries,
                                             quint64 cdOffset,
                                             quint64 gapThresholdBytes = 65536,
                                             quint64 maxChunkBytes = 4ULL * 1024 * 1024);

/**
 * Extracts and verifies (size + CRC-32) a single Store or Deflate ZIP entry from a downloaded range chunk.
 */
bool extractEntryFromChunk(const QByteArray& chunkBytes,
                           quint64 chunkStartOffset,
                           const ZipRemoteEntry& entry,
                           const QString& destPath,
                           QString* outError = nullptr);

}  // namespace ZipRangeUtils
