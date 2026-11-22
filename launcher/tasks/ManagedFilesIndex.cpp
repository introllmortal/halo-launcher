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

#include "ManagedFilesIndex.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "FileSystem.h"
#include "tasks/DeltaSyncTask.h"
#include "tasks/ZipRangeUtils.h"

namespace ManagedFilesIndex {

namespace {
constexpr const char* kIndexFileName = ".halo_managed_files.json";
}  // namespace

QString indexFilePath(const QString& instanceRoot)
{
    return FS::PathCombine(instanceRoot, QString::fromLatin1(kIndexFileName));
}

ManagedFilesManifest load(const QString& instanceRoot)
{
    ManagedFilesManifest manifest;
    if (instanceRoot.isEmpty()) {
        return manifest;
    }

    QFile indexFile(indexFilePath(instanceRoot));
    if (!indexFile.exists() || !indexFile.open(QIODevice::ReadOnly)) {
        return manifest;
    }

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(indexFile.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return manifest;
    }

    const QJsonObject rootObj = doc.object();
    manifest.version = rootObj.value(QStringLiteral("version")).toString();

    const QJsonArray fileArray = rootObj.value(QStringLiteral("files")).toArray();
    manifest.files.reserve(fileArray.size());
    for (const QJsonValue& itemVal : fileArray) {
        if (!itemVal.isObject()) {
            continue;
        }
        const QJsonObject fileObj = itemVal.toObject();
        const QString relPath = fileObj.value(QStringLiteral("path")).toString();
        if (relPath.isEmpty() || !DeltaSyncTask::isPathSafe(relPath)) {
            continue;
        }
        ManagedFileRecord record;
        record.path = relPath;
        record.size = fileObj.value(QStringLiteral("size")).toVariant().toLongLong();
        record.crc32 = static_cast<quint32>(fileObj.value(QStringLiteral("crc32")).toVariant().toULongLong());
        record.sha1 = fileObj.value(QStringLiteral("sha1")).toString();
        manifest.files.append(record);
    }

    return manifest;
}

bool save(const QString& instanceRoot, const ManagedFilesManifest& manifest)
{
    if (instanceRoot.isEmpty()) {
        return false;
    }

    const QString targetPath = indexFilePath(instanceRoot);
    const QFileInfo targetInfo(targetPath);
    if (targetInfo.isSymLink()) {
        return false;
    }

    QJsonArray fileArray;
    for (const ManagedFileRecord& record : manifest.files) {
        if (record.path.isEmpty() || !DeltaSyncTask::isPathSafe(record.path)) {
            continue;
        }
        QJsonObject fileObj;
        fileObj.insert(QStringLiteral("path"), record.path);
        fileObj.insert(QStringLiteral("size"), record.size);
        if (record.crc32 != 0) {
            fileObj.insert(QStringLiteral("crc32"), static_cast<qint64>(record.crc32));
        }
        if (!record.sha1.isEmpty()) {
            fileObj.insert(QStringLiteral("sha1"), record.sha1);
        }
        fileArray.append(fileObj);
    }

    QJsonObject rootObj;
    rootObj.insert(QStringLiteral("schema"), 1);
    rootObj.insert(QStringLiteral("version"), manifest.version);
    rootObj.insert(QStringLiteral("files"), fileArray);

    FS::ensureFilePathExists(targetPath);
    QSaveFile saveFile(targetPath);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray encoded = QJsonDocument(rootObj).toJson(QJsonDocument::Indented);
    if (saveFile.write(encoded) != encoded.size()) {
        saveFile.cancelWriting();
        return false;
    }
    return saveFile.commit();
}

QStringList computeRemovedPaths(const ManagedFilesManifest& previousManifest, const QSet<QString>& nextPaths)
{
    QStringList removals;
    for (const ManagedFileRecord& prevRecord : previousManifest.files) {
        if (prevRecord.path.isEmpty() || !DeltaSyncTask::isPathSafe(prevRecord.path)) {
            continue;
        }
        if (!nextPaths.contains(prevRecord.path)) {
            removals.append(prevRecord.path);
        }
    }
    return removals;
}

ManagedFilesManifest scanGameRoot(const QString& gameRoot, const QString& version)
{
    ManagedFilesManifest manifest;
    manifest.version = version;
    if (gameRoot.isEmpty()) {
        return manifest;
    }

    static const QStringList managedDirs = {
        QStringLiteral("mods"),
        QStringLiteral("kubejs"),
        QStringLiteral("config"),
        QStringLiteral("defaultconfigs"),
        QStringLiteral("resourcepacks"),
        QStringLiteral("shaderpacks"),
        QStringLiteral("datapacks"),
    };

    const QDir rootDir(gameRoot);
    for (const QString& subDirName : managedDirs) {
        const QString subDirPath = FS::PathCombine(gameRoot, subDirName);
        if (!QDir(subDirPath).exists()) {
            continue;
        }
        QDirIterator dirIt(subDirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (dirIt.hasNext()) {
            const QString absPath = dirIt.next();
            const QString relPath = QDir::cleanPath(rootDir.relativeFilePath(absPath)).replace(QLatin1Char('\\'), QLatin1Char('/'));
            if (!DeltaSyncTask::isPathSafe(relPath)) {
                continue;
            }
            quint32 crcValue = 0;
            qint64 fileSize = 0;
            if (!ZipRangeUtils::computeFileCrc32(absPath, crcValue, fileSize)) {
                continue;
            }
            ManagedFileRecord record;
            record.path = relPath;
            record.size = fileSize;
            record.crc32 = crcValue;
            manifest.files.append(record);
        }
    }
    return manifest;
}

bool hasSymlinkInRelativeChain(const QString& rootDir, const QString& relativePath)
{
    QString current = QDir(rootDir).absolutePath();
    if (QFileInfo(current).isSymLink()) {
        return true;
    }
    const QStringList parts = relativePath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        current = FS::PathCombine(current, part);
        if (QFileInfo(current).isSymLink()) {
            return true;
        }
    }
    return false;
}

}  // namespace ManagedFilesIndex
