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

#include "ZipRangeUtils.h"

#include <QDir>
#include <QFile>
#include <QtEndian>
#include <algorithm>
#include <cstring>
#include <zlib.h>

#include "FileSystem.h"

namespace ZipRangeUtils {

namespace {

quint16 readU16LE(const char* ptr)
{
    quint16 valLE = 0;
    std::memcpy(&valLE, ptr, sizeof(valLE));
    return qFromLittleEndian(valLE);
}

quint32 readU32LE(const char* ptr)
{
    quint32 valLE = 0;
    std::memcpy(&valLE, ptr, sizeof(valLE));
    return qFromLittleEndian(valLE);
}

quint64 readU64LE(const char* ptr)
{
    quint64 valLE = 0;
    std::memcpy(&valLE, ptr, sizeof(valLE));
    return qFromLittleEndian(valLE);
}

bool isManagedGameFolder(const QString& segment)
{
    static const QStringList knownFolders = {
        QStringLiteral("mods"),         QStringLiteral("kubejs"),      QStringLiteral("config"),
        QStringLiteral("defaultconfigs"), QStringLiteral("resourcepacks"), QStringLiteral("shaderpacks"),
        QStringLiteral("datapacks"),
    };
    for (const QString& folder : knownFolders) {
        if (segment.compare(folder, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool computeFileCrc32(const QString& filePath, quint32& outCrc32, qint64& outSize)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    uLong runningCrc = ::crc32(0L, Z_NULL, 0);
    qint64 totalBytes = 0;
    constexpr qint64 kBufferBytes = 65536;
    QByteArray readBuffer(kBufferBytes, Qt::Uninitialized);

    while (!file.atEnd()) {
        const qint64 bytesRead = file.read(readBuffer.data(), kBufferBytes);
        if (bytesRead < 0) {
            return false;
        }
        if (bytesRead > 0) {
            runningCrc = ::crc32(runningCrc, reinterpret_cast<const Bytef*>(readBuffer.constData()), static_cast<uInt>(bytesRead));
            totalBytes += bytesRead;
        }
    }

    outCrc32 = static_cast<quint32>(runningCrc);
    outSize = totalBytes;
    return true;
}

quint32 computeBytesCrc32(const QByteArray& buffer)
{
    if (buffer.isEmpty()) {
        return 0;
    }
    const uLong runningCrc = ::crc32(0L, reinterpret_cast<const Bytef*>(buffer.constData()), static_cast<uInt>(buffer.size()));
    return static_cast<quint32>(runningCrc);
}

ZipEocdInfo parseEocd(const QByteArray& tailBytes, quint64 tailStartOffset, quint64 totalZipSize)
{
    ZipEocdInfo info;
    if (tailBytes.size() < 22) {
        return info;
    }

    const char* rawBytes = tailBytes.constData();
    int eocdPos = -1;
    for (int pos = tailBytes.size() - 22; pos >= 0; --pos) {
        if (rawBytes[pos] == 'P' && rawBytes[pos + 1] == 'K' && rawBytes[pos + 2] == 0x05 && rawBytes[pos + 3] == 0x06) {
            const quint16 commentLen = readU16LE(rawBytes + pos + 20);
            if (pos + 22 + static_cast<int>(commentLen) <= tailBytes.size()) {
                eocdPos = pos;
                break;
            }
        }
    }
    if (eocdPos < 0) {
        return info;
    }

    const quint16 entryCount16 = readU16LE(rawBytes + eocdPos + 10);
    const quint32 cdSize32 = readU32LE(rawBytes + eocdPos + 12);
    const quint32 cdOffset32 = readU32LE(rawBytes + eocdPos + 16);

    info.entryCount = entryCount16;
    info.cdSize = cdSize32;
    info.cdOffset = cdOffset32;

    const bool needsZip64 = (entryCount16 == 0xFFFFu || cdSize32 == 0xFFFFFFFFu || cdOffset32 == 0xFFFFFFFFu);
    if (needsZip64 && eocdPos >= 20) {
        const int locPos = eocdPos - 20;
        if (rawBytes[locPos] == 'P' && rawBytes[locPos + 1] == 'K' && rawBytes[locPos + 2] == 0x06 && rawBytes[locPos + 3] == 0x07) {
            const quint64 zip64EocdAbsOffset = readU64LE(rawBytes + locPos + 8);
            if (zip64EocdAbsOffset >= tailStartOffset) {
                const qint64 relPos = static_cast<qint64>(zip64EocdAbsOffset - tailStartOffset);
                if (relPos >= 0 && relPos + 56 <= tailBytes.size() && rawBytes[relPos] == 'P' && rawBytes[relPos + 1] == 'K' &&
                    rawBytes[relPos + 2] == 0x06 && rawBytes[relPos + 3] == 0x06) {
                    info.entryCount = readU64LE(rawBytes + relPos + 32);
                    info.cdSize = readU64LE(rawBytes + relPos + 40);
                    info.cdOffset = readU64LE(rawBytes + relPos + 48);
                }
            }
        }
    }

    if (info.cdSize > 0 && info.cdOffset <= totalZipSize && info.cdSize <= totalZipSize - info.cdOffset) {
        info.valid = true;
    }
    return info;
}

QString normalizePackRelativePath(const QString& rawZipPath)
{
    QString cleaned = rawZipPath;
    cleaned.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (cleaned.startsWith(QStringLiteral("./"))) {
        cleaned.remove(0, 2);
    }
    if (cleaned.isEmpty() || cleaned.endsWith(QLatin1Char('/'))) {
        return {};
    }

    const QStringList parts = cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return {};
    }

    if (parts.first() == QStringLiteral(".minecraft") || parts.first() == QStringLiteral("minecraft") ||
        parts.first() == QStringLiteral("overrides")) {
        return parts.mid(1).join(QLatin1Char('/'));
    }

    if (parts.size() >= 2 &&
        (parts.at(1) == QStringLiteral(".minecraft") || parts.at(1) == QStringLiteral("minecraft") ||
         parts.at(1) == QStringLiteral("overrides"))) {
        return parts.mid(2).join(QLatin1Char('/'));
    }

    if (isManagedGameFolder(parts.first())) {
        return parts.join(QLatin1Char('/'));
    }

    return {};
}

QList<ZipRemoteEntry> parseCentralDirectory(const QByteArray& cdBytes, bool* outOk)
{
    QList<ZipRemoteEntry> entries;
    if (outOk) {
        *outOk = false;
    }

    const char* rawBytes = cdBytes.constData();
    const int totalLen = cdBytes.size();
    int cursor = 0;

    while (cursor + 46 <= totalLen) {
        if (rawBytes[cursor] != 'P' || rawBytes[cursor + 1] != 'K' || rawBytes[cursor + 2] != 0x01 || rawBytes[cursor + 3] != 0x02) {
            return {};
        }

        const quint16 versionMadeBy = readU16LE(rawBytes + cursor + 4);
        const quint16 method = readU16LE(rawBytes + cursor + 10);
        const quint32 crcValue = readU32LE(rawBytes + cursor + 16);
        quint64 cSize = readU32LE(rawBytes + cursor + 20);
        quint64 uSize = readU32LE(rawBytes + cursor + 24);
        const quint16 nameLen = readU16LE(rawBytes + cursor + 28);
        const quint16 extraLen = readU16LE(rawBytes + cursor + 30);
        const quint16 commentLen = readU16LE(rawBytes + cursor + 32);
        const quint32 externalAttributes = readU32LE(rawBytes + cursor + 38);
        quint64 localOffset = readU32LE(rawBytes + cursor + 42);
        const bool isUnixSymlink = ((versionMadeBy >> 8) == 3) && (((externalAttributes >> 16) & 0xF000u) == 0xA000u);

        const int recordEnd = cursor + 46 + static_cast<int>(nameLen) + static_cast<int>(extraLen) + static_cast<int>(commentLen);
        if (recordEnd > totalLen) {
            return {};
        }

        const QString rawName = QString::fromUtf8(rawBytes + cursor + 46, static_cast<int>(nameLen));

        if (extraLen > 0 && (uSize == 0xFFFFFFFFu || cSize == 0xFFFFFFFFu || localOffset == 0xFFFFFFFFu)) {
            int extraCursor = cursor + 46 + static_cast<int>(nameLen);
            const int extraEnd = extraCursor + static_cast<int>(extraLen);
            while (extraCursor + 4 <= extraEnd) {
                const quint16 headerId = readU16LE(rawBytes + extraCursor);
                const quint16 fieldSize = readU16LE(rawBytes + extraCursor + 2);
                const int fieldDataStart = extraCursor + 4;
                if (fieldDataStart + static_cast<int>(fieldSize) > extraEnd) {
                    break;
                }
                if (headerId == 0x0001) {
                    int z64Cursor = fieldDataStart;
                    if (uSize == 0xFFFFFFFFu && z64Cursor + 8 <= fieldDataStart + static_cast<int>(fieldSize)) {
                        uSize = readU64LE(rawBytes + z64Cursor);
                        z64Cursor += 8;
                    }
                    if (cSize == 0xFFFFFFFFu && z64Cursor + 8 <= fieldDataStart + static_cast<int>(fieldSize)) {
                        cSize = readU64LE(rawBytes + z64Cursor);
                        z64Cursor += 8;
                    }
                    if (localOffset == 0xFFFFFFFFu && z64Cursor + 8 <= fieldDataStart + static_cast<int>(fieldSize)) {
                        localOffset = readU64LE(rawBytes + z64Cursor);
                    }
                    break;
                }
                extraCursor = fieldDataStart + static_cast<int>(fieldSize);
            }
        }

        const QString normalized = normalizePackRelativePath(rawName);
        if (!isUnixSymlink && !normalized.isEmpty()) {
            ZipRemoteEntry entry;
            entry.rawName = rawName;
            entry.normalizedPath = normalized;
            entry.crc32 = crcValue;
            entry.compressedSize = cSize;
            entry.uncompressedSize = uSize;
            entry.method = method;
            entry.localHeaderOffset = localOffset;
            entries.append(entry);
        }

        cursor = recordEnd;
    }

    if (cursor != totalLen) {
        return {};
    }

    if (outOk) {
        *outOk = true;
    }
    return entries;
}

quint64 entryMaxEndOffset(const ZipRemoteEntry& entry, quint64 cdOffset)
{
    const quint64 nameBytes = static_cast<quint64>(entry.rawName.toUtf8().size());
    const quint64 worstCaseHeader = 30ULL + nameBytes + 65535ULL;
    const quint64 maxEnd = entry.localHeaderOffset + worstCaseHeader + entry.compressedSize;
    if (cdOffset > 0 && maxEnd >= cdOffset) {
        return cdOffset - 1;
    }
    return maxEnd > 0 ? (maxEnd - 1) : 0;
}

QList<CoalescedByteRange> coalesceByteRanges(const QList<ZipRemoteEntry>& entries,
                                             quint64 cdOffset,
                                             quint64 gapThresholdBytes,
                                             quint64 maxChunkBytes)
{
    QList<int> order;
    order.reserve(entries.size());
    for (int idx = 0; idx < entries.size(); ++idx) {
        order.append(idx);
    }
    std::sort(order.begin(), order.end(), [&entries](int left, int right) {
        return entries[left].localHeaderOffset < entries[right].localHeaderOffset;
    });

    QList<CoalescedByteRange> ranges;
    for (int idx : order) {
        const ZipRemoteEntry& entry = entries[idx];
        const quint64 spanStart = entry.localHeaderOffset;
        const quint64 spanEnd = entryMaxEndOffset(entry, cdOffset);
        if (spanEnd < spanStart) {
            continue;
        }

        if (!ranges.isEmpty()) {
            CoalescedByteRange& current = ranges.last();
            const quint64 gap = (spanStart > current.end) ? (spanStart - current.end - 1) : 0;
            const quint64 mergedEnd = std::max(current.end, spanEnd);
            const quint64 mergedSize = mergedEnd - current.start + 1;
            if (gap <= gapThresholdBytes && mergedSize <= maxChunkBytes) {
                current.end = mergedEnd;
                current.entryIndices.append(idx);
                continue;
            }
        }

        CoalescedByteRange fresh;
        fresh.start = spanStart;
        fresh.end = spanEnd;
        fresh.entryIndices.append(idx);
        ranges.append(fresh);
    }
    return ranges;
}

bool extractEntryFromChunk(const QByteArray& chunkBytes,
                           quint64 chunkStartOffset,
                           const ZipRemoteEntry& entry,
                           const QString& destPath,
                           QString* outError)
{
    auto fail = [outError](const QString& msg) {
        if (outError) {
            *outError = msg;
        }
        return false;
    };

    if (entry.localHeaderOffset < chunkStartOffset) {
        return fail(QStringLiteral("Entry local header precedes downloaded chunk"));
    }

    const quint64 relOffset = entry.localHeaderOffset - chunkStartOffset;
    if (relOffset + 30ULL > static_cast<quint64>(chunkBytes.size())) {
        return fail(QStringLiteral("Truncated local ZIP header for %1").arg(entry.normalizedPath));
    }

    const char* rawChunk = chunkBytes.constData();
    const qint64 headerPos = static_cast<qint64>(relOffset);
    if (rawChunk[headerPos] != 'P' || rawChunk[headerPos + 1] != 'K' || rawChunk[headerPos + 2] != 0x03 || rawChunk[headerPos + 3] != 0x04) {
        return fail(QStringLiteral("Invalid local ZIP signature for %1").arg(entry.normalizedPath));
    }

    const quint16 localNameLen = readU16LE(rawChunk + headerPos + 26);
    const quint16 localExtraLen = readU16LE(rawChunk + headerPos + 28);
    const quint64 payloadOffset = relOffset + 30ULL + static_cast<quint64>(localNameLen) + static_cast<quint64>(localExtraLen);

    if (payloadOffset + entry.compressedSize > static_cast<quint64>(chunkBytes.size())) {
        return fail(QStringLiteral("Truncated ZIP payload for %1").arg(entry.normalizedPath));
    }

    FS::ensureFilePathExists(destPath);
    QFile outFile(destPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return fail(QStringLiteral("Failed to open staging output %1").arg(destPath));
    }

    auto failOutput = [&outFile, &destPath, &fail](const QString& reason) -> bool {
        if (outFile.isOpen()) {
            outFile.close();
        }
        QFile::remove(destPath);
        return fail(reason);
    };

    uLong actualCrc = ::crc32(0L, Z_NULL, 0);
    quint64 actualWritten = 0;

    if (entry.uncompressedSize == 0 && entry.compressedSize == 0) {
        outFile.close();
        if (entry.crc32 != 0) {
            QFile::remove(destPath);
            return fail(QStringLiteral("CRC-32 mismatch for empty entry %1").arg(entry.normalizedPath));
        }
        return true;
    }

    const char* payloadPtr = rawChunk + static_cast<qint64>(payloadOffset);

    if (entry.method == 0) {
        if (entry.compressedSize != entry.uncompressedSize) {
            return failOutput(QStringLiteral("Store size mismatch for %1").arg(entry.normalizedPath));
        }
        if (entry.compressedSize > 0) {
            const qint64 written = outFile.write(payloadPtr, static_cast<qint64>(entry.compressedSize));
            if (written != static_cast<qint64>(entry.compressedSize)) {
                return failOutput(QStringLiteral("Write failed for %1").arg(entry.normalizedPath));
            }
            actualCrc = ::crc32(actualCrc, reinterpret_cast<const Bytef*>(payloadPtr), static_cast<uInt>(entry.compressedSize));
            actualWritten = entry.compressedSize;
        }
    } else if (entry.method == 8) {
        z_stream stream{};
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
            return failOutput(QStringLiteral("zlib inflateInit2 failed for %1").arg(entry.normalizedPath));
        }

        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(payloadPtr));
        stream.avail_in = static_cast<uInt>(entry.compressedSize);

        constexpr qint64 kOutChunkBytes = 65536;
        QByteArray outBuffer(kOutChunkBytes, Qt::Uninitialized);
        int zStatus = Z_OK;

        while (zStatus != Z_STREAM_END) {
            stream.next_out = reinterpret_cast<Bytef*>(outBuffer.data());
            stream.avail_out = static_cast<uInt>(kOutChunkBytes);
            zStatus = inflate(&stream, Z_NO_FLUSH);

            const qint64 produced = kOutChunkBytes - static_cast<qint64>(stream.avail_out);
            if (produced > 0) {
                if (actualWritten + static_cast<quint64>(produced) > entry.uncompressedSize) {
                    inflateEnd(&stream);
                    return failOutput(QStringLiteral("Decompressed size overflow for %1").arg(entry.normalizedPath));
                }
                if (outFile.write(outBuffer.constData(), produced) != produced) {
                    inflateEnd(&stream);
                    return failOutput(QStringLiteral("Write failed for %1").arg(entry.normalizedPath));
                }
                actualCrc = ::crc32(actualCrc, reinterpret_cast<const Bytef*>(outBuffer.constData()), static_cast<uInt>(produced));
                actualWritten += static_cast<quint64>(produced);
            }

            if (zStatus == Z_STREAM_END) {
                break;
            }
            if (zStatus != Z_OK) {
                inflateEnd(&stream);
                return failOutput(QStringLiteral("zlib inflate failed (%1) for %2").arg(zStatus).arg(entry.normalizedPath));
            }
            if (stream.avail_in == 0 && stream.avail_out != 0) {
                inflateEnd(&stream);
                return failOutput(QStringLiteral("Incomplete Deflate stream for %1").arg(entry.normalizedPath));
            }
        }
        inflateEnd(&stream);
    } else {
        return failOutput(QStringLiteral("Unsupported ZIP compression method %1 for %2").arg(entry.method).arg(entry.normalizedPath));
    }

    outFile.close();

    if (actualWritten != entry.uncompressedSize) {
        QFile::remove(destPath);
        return fail(QStringLiteral("Uncompressed size mismatch for %1").arg(entry.normalizedPath));
    }
    if (static_cast<quint32>(actualCrc) != entry.crc32) {
        QFile::remove(destPath);
        return fail(QStringLiteral("CRC-32 mismatch for %1").arg(entry.normalizedPath));
    }
    return true;
}

}  // namespace ZipRangeUtils
