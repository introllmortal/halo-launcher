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
#include "PackSnapshot.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStringList>
#include <algorithm>
#include <utility>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModBisectSession.h"

namespace {

bool writeBytesAtomic(const QString& destPath, const QByteArray& bytes)
{
    QSaveFile out(destPath);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (out.write(bytes) != bytes.size()) {
        out.cancelWriting();
        return false;
    }
    return out.commit();
}

bool copyFileAtomic(const QString& srcPath, const QString& destPath)
{
    QFile in(srcPath);
    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }
    return writeBytesAtomic(destPath, in.readAll());
}

void pruneOldSnapshots(const QString& root, int maxToKeep)
{
    if (maxToKeep <= 0 || root.isEmpty()) {
        return;
    }
    QDir rootDir(root);
    if (!rootDir.exists()) {
        return;
    }
    const QFileInfoList dirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    if (dirs.size() <= maxToKeep) {
        return;
    }
    QFileInfoList sorted = dirs;
    std::sort(sorted.begin(), sorted.end(), [](const QFileInfo& a, const QFileInfo& b) {
        return a.fileName() > b.fileName();
    });
    for (int i = maxToKeep; i < sorted.size(); ++i) {
        FS::deletePath(sorted.at(i).absoluteFilePath());
    }
}

}  // namespace

namespace PackSnapshot {

QString snapshotsRoot(const BaseInstance* instance)
{
    if (!instance || instance->instanceRoot().isEmpty()) {
        return {};
    }
    return FS::PathCombine(instance->instanceRoot(), QStringLiteral("halo-snapshots"));
}

std::optional<Info> createSnapshot(BaseInstance* instance, const QString& targetVersionLabel, int maxToKeep)
{
    if (!instance || instance->isRunning() || instance->instanceRoot().isEmpty() || ModBisectSession::hasActiveSession(instance)) {
        return std::nullopt;
    }
    instance->saveNow();

    const QString root = snapshotsRoot(instance);
    if (!FS::ensureFolderPathExists(root)) {
        return std::nullopt;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString baseId = now.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    QString snapId = baseId;
    QString snapDir = FS::PathCombine(root, snapId);
    for (int suffix = 1; QFileInfo::exists(snapDir) && suffix < 1000; ++suffix) {
        snapId = QStringLiteral("%1-%2").arg(baseId).arg(suffix);
        snapDir = FS::PathCombine(root, snapId);
    }
    if (!FS::ensureFolderPathExists(snapDir)) {
        return std::nullopt;
    }

    const QString instRoot = instance->instanceRoot();
    const QString gameRoot = instance->gameRoot();

    bool hasInstCfg = false;
    if (const QString src = FS::PathCombine(instRoot, QStringLiteral("instance.cfg")); QFile::exists(src)) {
        if (!copyFileAtomic(src, FS::PathCombine(snapDir, QStringLiteral("instance.cfg")))) {
            FS::deletePath(snapDir);
            return std::nullopt;
        }
        hasInstCfg = true;
    }

    bool hasMmcPack = false;
    if (const QString src = FS::PathCombine(instRoot, QStringLiteral("mmc-pack.json")); QFile::exists(src)) {
        if (!copyFileAtomic(src, FS::PathCombine(snapDir, QStringLiteral("mmc-pack.json")))) {
            FS::deletePath(snapDir);
            return std::nullopt;
        }
        hasMmcPack = true;
    }

    bool hasOptions = false;
    if (const QString src = FS::PathCombine(gameRoot, QStringLiteral("options.txt")); QFile::exists(src)) {
        if (!copyFileAtomic(src, FS::PathCombine(snapDir, QStringLiteral("options.txt")))) {
            FS::deletePath(snapDir);
            return std::nullopt;
        }
        hasOptions = true;
    }

    bool hasConfig = false;
    if (const QString configSrc = FS::PathCombine(gameRoot, QStringLiteral("config")); QDir(configSrc).exists()) {
        const QString configDst = FS::PathCombine(snapDir, QStringLiteral("config"));
        FS::copy copier(configSrc, configDst);
        copier.overwrite(true);
        if (!copier()) {
            FS::deletePath(snapDir);
            return std::nullopt;
        }
        hasConfig = true;
    }

    const QList<ModEntry> mods = scanCurrentMods(instance);
    const qint64 modCount = mods.size();
    QJsonArray modsArray;
    for (const ModEntry& mod : mods) {
        modsArray.append(QJsonObject{ { QStringLiteral("fileName"), mod.fileName },
                                      { QStringLiteral("size"), mod.size },
                                      { QStringLiteral("enabled"), mod.enabled } });
    }
    if (!writeBytesAtomic(FS::PathCombine(snapDir, QStringLiteral("mods-manifest.json")),
                          QJsonDocument(modsArray).toJson(QJsonDocument::Compact))) {
        FS::deletePath(snapDir);
        return std::nullopt;
    }

    QJsonObject meta;
    meta.insert(QStringLiteral("id"), snapId);
    meta.insert(QStringLiteral("createdAtUtc"), now.toString(Qt::ISODateWithMs));
    meta.insert(QStringLiteral("instanceName"), instance->name());
    meta.insert(QStringLiteral("packName"), instance->getManagedPackName());
    meta.insert(QStringLiteral("packVersion"), instance->getManagedPackVersionName());
    meta.insert(QStringLiteral("targetVersion"), targetVersionLabel);
    meta.insert(QStringLiteral("modCount"), modCount);
    meta.insert(QStringLiteral("hasInstanceCfg"), hasInstCfg);
    meta.insert(QStringLiteral("hasMmcPack"), hasMmcPack);
    meta.insert(QStringLiteral("hasOptions"), hasOptions);
    meta.insert(QStringLiteral("hasConfig"), hasConfig);

    if (!writeBytesAtomic(FS::PathCombine(snapDir, QStringLiteral("snapshot.json")),
                          QJsonDocument(meta).toJson(QJsonDocument::Indented))) {
        FS::deletePath(snapDir);
        return std::nullopt;
    }

    pruneOldSnapshots(root, maxToKeep);

    Info info;
    info.id = snapId;
    info.dirPath = snapDir;
    info.createdAt = now.toLocalTime();
    info.packVersion = instance->getManagedPackVersionName();
    info.targetVersion = targetVersionLabel;
    info.modCount = modCount;
    info.hasConfig = hasConfig;
    return info;
}

QList<Info> listSnapshots(const BaseInstance* instance)
{
    QList<Info> result;
    const QString root = snapshotsRoot(instance);
    if (root.isEmpty()) {
        return result;
    }
    QDir rootDir(root);
    if (!rootDir.exists()) {
        return result;
    }

    const QFileInfoList dirs = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& dirInfo : dirs) {
        const QString metaPath = FS::PathCombine(dirInfo.absoluteFilePath(), QStringLiteral("snapshot.json"));
        QFile f(metaPath);
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (!doc.isObject()) {
            continue;
        }
        const QJsonObject obj = doc.object();
        Info info;
        info.id = dirInfo.fileName();
        info.dirPath = dirInfo.absoluteFilePath();
        const QDateTime parsed = QDateTime::fromString(obj.value(QStringLiteral("createdAtUtc")).toString(), Qt::ISODateWithMs);
        info.createdAt = parsed.isValid() ? parsed.toLocalTime() : dirInfo.lastModified();
        info.packVersion = obj.value(QStringLiteral("packVersion")).toString();
        info.targetVersion = obj.value(QStringLiteral("targetVersion")).toString();
        info.modCount = static_cast<qint64>(obj.value(QStringLiteral("modCount")).toDouble());
        info.hasConfig = obj.contains(QStringLiteral("hasConfig"))
                             ? obj.value(QStringLiteral("hasConfig")).toBool()
                             : QDir(FS::PathCombine(info.dirPath, QStringLiteral("config"))).exists();
        result.append(info);
    }

    std::sort(result.begin(), result.end(), [](const Info& a, const Info& b) { return a.id > b.id; });
    return result;
}

bool hasSnapshot(const BaseInstance* instance)
{
    return !listSnapshots(instance).isEmpty();
}

namespace {
struct FileBackup {
    QString destPath;
    bool existedBefore = false;
    QByteArray previousBytes;
    bool modified = false;
};

bool captureFileBackup(const QString& destPath, FileBackup& backup)
{
    backup.destPath = destPath;
    backup.existedBefore = QFile::exists(destPath);
    backup.modified = false;
    if (!backup.existedBefore) {
        return true;
    }
    QFile f(destPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    backup.previousBytes = f.readAll();
    return f.error() == QFile::NoError;
}

void rollbackBackups(const QList<FileBackup>& backups)
{
    for (qsizetype i = backups.size() - 1; i >= 0; --i) {
        const FileBackup& b = backups.at(i);
        if (!b.modified) {
            continue;
        }
        if (b.existedBefore) {
            writeBytesAtomic(b.destPath, b.previousBytes);
        } else {
            QFile::remove(b.destPath);
        }
    }
}
}  // namespace

bool restoreSnapshot(BaseInstance* instance, const Info& snapshot)
{
    if (!instance || instance->isRunning() || snapshot.dirPath.isEmpty()) {
        return false;
    }
    const QString snapDir = snapshot.dirPath;
    const QString metaPath = FS::PathCombine(snapDir, QStringLiteral("snapshot.json"));
    if (!QFileInfo::exists(metaPath)) {
        return false;
    }

    bool metaHasConfig = snapshot.hasConfig;
    if (QFile metaFile(metaPath); metaFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(metaFile.readAll());
        if (doc.isObject() && doc.object().contains(QStringLiteral("hasConfig"))) {
            metaHasConfig = doc.object().value(QStringLiteral("hasConfig")).toBool();
        }
    }

    // Flush any queued PackProfile saves BEFORE overwriting mmc-pack.json so reload() won't overwrite it.
    MinecraftInstance* mcInst = (APPLICATION_DYN != nullptr) ? dynamic_cast<MinecraftInstance*>(instance) : nullptr;
    if (mcInst && mcInst->getPackProfile()) {
        mcInst->getPackProfile()->saveNow();
    }

    const QString instRoot = instance->instanceRoot();
    const QString gameRoot = instance->gameRoot();

    const QString instCfgSrc = FS::PathCombine(snapDir, QStringLiteral("instance.cfg"));
    const QString instCfgDst = FS::PathCombine(instRoot, QStringLiteral("instance.cfg"));
    const QString mmcPackSrc = FS::PathCombine(snapDir, QStringLiteral("mmc-pack.json"));
    const QString mmcPackDst = FS::PathCombine(instRoot, QStringLiteral("mmc-pack.json"));
    const QString optionsSrc = FS::PathCombine(snapDir, QStringLiteral("options.txt"));
    const QString optionsDst = FS::PathCombine(gameRoot, QStringLiteral("options.txt"));

    QList<FileBackup> backups;
    auto stageCopy = [&](const QString& src, const QString& dst) -> bool {
        if (!QFile::exists(src)) {
            return true;
        }
        FileBackup backup;
        if (!captureFileBackup(dst, backup)) {
            return false;
        }
        FS::ensureFolderPathExists(QFileInfo(dst).absolutePath());
        if (!copyFileAtomic(src, dst)) {
            return false;
        }
        backup.modified = true;
        backups.append(std::move(backup));
        return true;
    };

    const QString snapConfig = FS::PathCombine(snapDir, QStringLiteral("config"));
    const QString targetConfig = FS::PathCombine(gameRoot, QStringLiteral("config"));
    const QString backupConfig = FS::PathCombine(gameRoot, QStringLiteral("config.halo_rollback_bak"));
    bool hadTargetConfig = false;
    bool configSwapped = false;

    if (metaHasConfig && QDir(snapConfig).exists()) {
        FS::ensureFolderPathExists(gameRoot);
        FS::deletePath(backupConfig);
        hadTargetConfig = QDir(targetConfig).exists();
        if (hadTargetConfig && !QDir().rename(targetConfig, backupConfig)) {
            return false;
        }
        FS::copy copier(snapConfig, targetConfig);
        copier.overwrite(true);
        if (!copier()) {
            FS::deletePath(targetConfig);
            if (hadTargetConfig) {
                QDir().rename(backupConfig, targetConfig);
            }
            return false;
        }
        configSwapped = true;
    }

    auto abortWithRollback = [&]() -> bool {
        rollbackBackups(backups);
        if (configSwapped) {
            FS::deletePath(targetConfig);
            if (hadTargetConfig) {
                QDir().rename(backupConfig, targetConfig);
            }
        }
        return false;
    };

    if (!stageCopy(instCfgSrc, instCfgDst) || !stageCopy(mmcPackSrc, mmcPackDst) || !stageCopy(optionsSrc, optionsDst)) {
        return abortWithRollback();
    }

    if (configSwapped && hadTargetConfig) {
        FS::deletePath(backupConfig);
    }

    const bool restoredMmcPack = QFile::exists(mmcPackSrc);
    instance->reloadSettings();
    if (restoredMmcPack && mcInst && mcInst->getPackProfile()) {
        mcInst->getPackProfile()->reload(Net::Mode::Offline);
    }
    return true;
}

bool restoreLatestSnapshot(BaseInstance* instance)
{
    const QList<Info> list = listSnapshots(instance);
    if (list.isEmpty()) {
        return false;
    }
    return restoreSnapshot(instance, list.first());
}

}  // namespace PackSnapshot
