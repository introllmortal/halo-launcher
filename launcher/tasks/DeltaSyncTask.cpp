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

#include "DeltaSyncTask.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QUrl>

#if defined(LAUNCHER_APPLICATION)
#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "net/ChecksumValidator.h"
#include "net/Download.h"
#include "tasks/ManagedFilesIndex.h"
#include "tasks/ZipDeltaDownloader.h"

static QString computeFileSha1Hex(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file)) {
        return {};
    }

    return QString::fromLatin1(hash.result().toHex());
}

DeltaSyncTask::DeltaSyncTask(BaseInstance* instance,
                             const QJsonObject& manifest,
                             const QString& targetVersion,
                             QObject* parent)
    : Task(), m_instance(instance), m_manifest(manifest), m_targetVersion(targetVersion)
{
    if (parent) {
        setParent(parent);
    }
}

DeltaSyncTask::DeltaSyncTask(BaseInstance* instance,
                             const ZipDeltaPlan& zipPlan,
                             QNetworkAccessManager* network,
                             QObject* parent)
    : Task(),
      m_instance(instance),
      m_targetVersion(zipPlan.targetVersion),
      m_useZipPlan(true),
      m_zipPlan(zipPlan),
      m_network(network)
{
    if (parent) {
        setParent(parent);
    }
}

DeltaSyncTask::~DeltaSyncTask()
{
    cleanupTemporaryDirs();
}

bool DeltaSyncTask::abort()
{
    if (m_downloadJob && m_downloadJob->isRunning()) {
        m_downloadJob->abort();
    }
    if (m_zipDownloader && m_zipDownloader->isRunning()) {
        m_zipDownloader->abort();
    }
    cleanupTemporaryDirs();
    return Task::abort();
}

bool DeltaSyncTask::isPathSafe(const QString& rawPath)
{
    if (rawPath.isEmpty() || rawPath.startsWith('/') || rawPath.startsWith('\\') ||
        rawPath.contains("..") || rawPath.contains(':')) {
        return false;
    }

    const QString clean = QDir::cleanPath(QString(rawPath).replace('\\', '/'));
    if (clean.startsWith('/') || clean.startsWith('\\') || clean.contains("..") || clean.contains(':') || clean.endsWith('/')) {
        return false;
    }

    static const QStringList forbiddenExact = {
        "instance.cfg", "mmc-pack.json", "user_jvm_args.txt", "servers.dat", "options.txt"
    };
    for (const auto& forbidden : forbiddenExact) {
        if (clean.compare(forbidden, Qt::CaseInsensitive) == 0) {
            return false;
        }
    }

    static const QStringList forbiddenPrefixes = {
        "patches/", "saves/", "screenshots/", "backups/", "logs/", "crash-reports/", ".delta_staging/", ".delta_backup/"
    };
    for (const auto& prefix : forbiddenPrefixes) {
        if (clean.startsWith(prefix, Qt::CaseInsensitive)) {
            return false;
        }
    }

    static const QStringList allowedPrefixes = {
        "mods/", "kubejs/", "config/", "defaultconfigs/", "resourcepacks/", "shaderpacks/", "datapacks/"
    };
    for (const auto& allowed : allowedPrefixes) {
        if (clean.startsWith(allowed, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool DeltaSyncTask::validatePathSecurity(const QString& rawPath) const
{
    return isPathSafe(rawPath);
}

QList<DeltaFileItem> DeltaSyncTask::parseManifestItems(const QJsonObject& manifest)
{
    QList<DeltaFileItem> items;
    QSet<QString> seen;

    auto addParsed = [&items, &seen](const QString& path, const QString& sha1, qint64 size,
                                     const QString& url, DeltaFileItem::Action action, bool overwrite) {
        if (path.isEmpty()) {
            return;
        }
        DeltaFileItem item{ path, sha1, size, url, action, overwrite };
        if (seen.contains(path)) {
            for (auto& existing : items) {
                if (existing.path == path) {
                    existing = item;
                    return;
                }
            }
        }
        seen.insert(path);
        items.append(item);
    };

    auto parseObject = [&](const QJsonObject& entryObj, DeltaFileItem::Action defaultAction) {
        const QString path = entryObj.value("path").toString(entryObj.value("file").toString());
        const QString sha1 = entryObj.value("sha1").toString(entryObj.value("hash").toString());
        const qint64 size = entryObj.contains("size") ? entryObj.value("size").toVariant().toLongLong()
                                                      : entryObj.value("fileSize").toVariant().toLongLong();
        const QString url = entryObj.value("url").toString(entryObj.value("downloadUrl").toString());
        const QString act = entryObj.value("action").toString().toLower();
        const bool preserve = entryObj.value("preserveIfExists").toBool(false);
        const bool overwrite = entryObj.value("overwrite").toBool(true) && !preserve;
        const auto action = (act == "remove" || act == "delete") ? DeltaFileItem::Action::Remove : defaultAction;
        addParsed(path, sha1, size, url, action, overwrite);
    };

    for (const auto& key : { "files", "updates" }) {
        if (manifest.contains(key) && manifest.value(key).isArray()) {
            for (const auto& itemValue : manifest.value(key).toArray()) {
                if (itemValue.isObject()) {
                    parseObject(itemValue.toObject(), DeltaFileItem::Action::Update);
                }
            }
        }
    }
    for (const auto& key : { "removals", "remove" }) {
        if (manifest.contains(key) && manifest.value(key).isArray()) {
            for (const auto& itemValue : manifest.value(key).toArray()) {
                if (itemValue.isObject()) {
                    parseObject(itemValue.toObject(), DeltaFileItem::Action::Remove);
                } else if (itemValue.isString()) {
                    addParsed(itemValue.toString(), "", 0, "", DeltaFileItem::Action::Remove, true);
                }
            }
        }
    }
    return items;
}

bool DeltaSyncTask::isLocalFileUpToDate(const QString& targetPath, const DeltaFileItem& item)
{
    if (item.action != DeltaFileItem::Action::Update) {
        return false;
    }

    QFileInfo fi(targetPath);
    if (!fi.exists() || !fi.isFile()) {
        return false;
    }

    if (!item.overwrite) {
        return true;
    }

    if (item.sha1.isEmpty()) {
        return false;
    }

    if (item.size > 0 && fi.size() != item.size) {
        return false;
    }

    const QString localSha1 = computeFileSha1Hex(targetPath);
    return !localSha1.isEmpty() && localSha1.compare(item.sha1, Qt::CaseInsensitive) == 0;
}

int DeltaSyncTask::filterUnchangedUpdates(const QString& gameRoot, QList<DeltaFileItem>& items)
{
    if (gameRoot.isEmpty()) {
        return 0;
    }

    int skipped = 0;
    QList<DeltaFileItem> filtered;
    filtered.reserve(items.size());
    for (const auto& item : items) {
        if (item.action == DeltaFileItem::Action::Update) {
            const QString targetPath = FS::PathCombine(gameRoot, item.path);
            if (isLocalFileUpToDate(targetPath, item)) {
                ++skipped;
                continue;
            }
        }
        filtered.append(item);
    }
    items = std::move(filtered);
    return skipped;
}

bool DeltaSyncTask::parseManifest()
{
    m_items = parseManifestItems(m_manifest);
    return true;
}

void DeltaSyncTask::startStagedDownload()
{
    bool hasUpdates = false;
    for (const auto& item : m_items) {
        if (item.action == DeltaFileItem::Action::Update) {
            hasUpdates = true;
            break;
        }
    }
    if (!hasUpdates) {
        executeRollbackJournalAndCommit();
        return;
    }

    setStatus(tr("Downloading updates..."));
    cleanupTemporaryDirs();
    if (!QDir().mkpath(m_stagingDir)) {
        emitFailed(tr("Failed to create delta staging directory."));
        return;
    }

    if (!APPLICATION || !APPLICATION->network()) {
        emitFailed(tr("Network subsystem unavailable. Please use full archive import."));
        return;
    }

    m_downloadJob = makeShared<NetJob>(tr("Delta Sync Updates"), APPLICATION->network());
    connect(m_downloadJob.get(), &NetJob::succeeded, this, &DeltaSyncTask::onDownloadJobSucceeded);
    connect(m_downloadJob.get(), &NetJob::failed, this, &DeltaSyncTask::onDownloadJobFailed);
    connect(m_downloadJob.get(), &NetJob::aborted, this, &DeltaSyncTask::onDownloadJobAborted);
    connect(m_downloadJob.get(), &NetJob::progress, this, &DeltaSyncTask::setProgress);
    connect(m_downloadJob.get(), &NetJob::status, this, &DeltaSyncTask::setStatus);
    connect(m_downloadJob.get(), &NetJob::details, this, &DeltaSyncTask::setDetails);

    for (const auto& item : m_items) {
        if (item.action != DeltaFileItem::Action::Update) {
            continue;
        }
        QString stagedFile = FS::PathCombine(m_stagingDir, item.path);
        FS::ensureFilePathExists(stagedFile);
        auto dl = Net::Download::makeFile(QUrl(item.url), stagedFile);
        if (!item.sha1.isEmpty()) {
            dl->addValidator(new Net::ChecksumValidator(QCryptographicHash::Sha1, item.sha1));
        }
        m_downloadJob->addNetAction(dl);
    }

    setAbortable(true);
    m_downloadJob->start();
}

void DeltaSyncTask::startZipPlanDownload()
{
    if (m_zipPlan.entriesToDownload.isEmpty()) {
        executeRollbackJournalAndCommit();
        return;
    }

    QNetworkAccessManager* network = m_network ? m_network.data() : (APPLICATION ? APPLICATION->network() : nullptr);
    if (!network) {
        emitFailed(tr("Network subsystem unavailable. Please use full archive import."));
        return;
    }

    cleanupTemporaryDirs();
    if (!QDir().mkpath(m_stagingDir)) {
        emitFailed(tr("Failed to create delta staging directory."));
        return;
    }

    m_zipDownloader = new ZipDeltaDownloadTask(network, m_zipPlan, m_stagingDir, this);
    connect(m_zipDownloader, &Task::progress, this, &DeltaSyncTask::setProgress);
    connect(m_zipDownloader, &Task::status, this, &DeltaSyncTask::setStatus);
    connect(m_zipDownloader, &Task::succeeded, this, [this]() {
        m_zipDownloader->deleteLater();
        m_zipDownloader.clear();
        setAbortable(false);
        executeRollbackJournalAndCommit();
    });
    connect(m_zipDownloader, &Task::failed, this, [this](const QString& reason) {
        m_zipDownloader->deleteLater();
        m_zipDownloader.clear();
        onDownloadJobFailed(reason);
    });
    connect(m_zipDownloader, &Task::aborted, this, [this]() {
        m_zipDownloader->deleteLater();
        m_zipDownloader.clear();
        onDownloadJobAborted();
    });

    setAbortable(true);
    m_zipDownloader->start();
}

void DeltaSyncTask::onDownloadJobSucceeded()
{
    setAbortable(false);
    setStatus(tr("Validating downloaded files..."));
    if (validateDownloadedFiles()) {
        executeRollbackJournalAndCommit();
    }
}

void DeltaSyncTask::onDownloadJobFailed(const QString& reason)
{
    setAbortable(false);
    cleanupTemporaryDirs();
    emitFailed(tr("Delta sync download failed: %1. Please use full archive import.").arg(reason));
}

void DeltaSyncTask::onDownloadJobAborted()
{
    setAbortable(false);
    cleanupTemporaryDirs();
    emitAborted();
}

bool DeltaSyncTask::validateDownloadedFiles()
{
    for (const auto& item : m_items) {
        if (item.action != DeltaFileItem::Action::Update) {
            continue;
        }

        const QString stagedPath = FS::PathCombine(m_stagingDir, item.path);
        QFileInfo fi(stagedPath);
        if (!fi.exists() || !fi.isFile()) {
            cleanupTemporaryDirs();
            emitFailed(tr("Validation failed: Staged file '%1' does not exist. Please use full archive import.").arg(item.path));
            return false;
        }

        if (item.size > 0 && fi.size() != item.size) {
            cleanupTemporaryDirs();
            emitFailed(tr("Validation failed: File size mismatch for '%1' (expected %2, got %3). Please use full archive import.")
                           .arg(item.path)
                           .arg(item.size)
                           .arg(fi.size()));
            return false;
        }

        if (!item.sha1.isEmpty()) {
            const QString actualSha1 = computeFileSha1Hex(stagedPath);
            if (actualSha1.isEmpty()) {
                cleanupTemporaryDirs();
                emitFailed(tr("Validation failed: Error computing hash for '%1'. Please use full archive import.").arg(item.path));
                return false;
            }

            if (actualSha1.compare(item.sha1, Qt::CaseInsensitive) != 0) {
                cleanupTemporaryDirs();
                emitFailed(tr("Validation failed: SHA-1 hash mismatch for '%1' (expected %2, got %3). Please use full archive import.")
                               .arg(item.path)
                               .arg(item.sha1)
                               .arg(actualSha1));
                return false;
            }
        }
    }
    return true;
}

void DeltaSyncTask::executeTask()
{
    if (!m_instance) {
        emitFailed(tr("Instance is invalid."));
        return;
    }

    m_stagingDir = FS::PathCombine(m_instance->gameRoot(), ".delta_staging");
    m_backupDir = FS::PathCombine(m_instance->gameRoot(), ".delta_backup");

    if (m_useZipPlan) {
        if (!m_zipPlan.valid) {
            emitFailed(tr("Invalid ZIP delta plan."));
            return;
        }
        m_items.clear();
        for (const ZipRemoteEntry& entry : m_zipPlan.entriesToDownload) {
            if (!validatePathSecurity(entry.normalizedPath)) {
                emitFailed(tr("Security violation: Archive attempts to modify protected path '%1'").arg(entry.normalizedPath));
                return;
            }
            DeltaFileItem item;
            item.path = entry.normalizedPath;
            item.size = static_cast<qint64>(entry.uncompressedSize);
            item.action = DeltaFileItem::Action::Update;
            m_items.append(item);
        }
        for (const QString& remPath : m_zipPlan.pathsToRemove) {
            if (!validatePathSecurity(remPath)) {
                emitFailed(tr("Security violation: Archive attempts to remove protected path '%1'").arg(remPath));
                return;
            }
            DeltaFileItem item;
            item.path = remPath;
            item.action = DeltaFileItem::Action::Remove;
            m_items.append(item);
        }
        if (m_items.isEmpty()) {
            m_instance->setManagedPackVersionID(m_targetVersion);
            m_instance->setManagedPackVersionName(m_targetVersion);
            m_instance->saveNow();
            persistManagedIndex();
            emitSucceeded();
            return;
        }
        startZipPlanDownload();
        return;
    }

    setStatus(tr("Validating delta sync manifest..."));
    if (!parseManifest()) {
        emitFailed(tr("Failed to parse delta sync manifest."));
        return;
    }

    for (const auto& item : m_items) {
        if (!validatePathSecurity(item.path)) {
            emitFailed(tr("Security violation: Manifest attempts to modify protected system path '%1'").arg(item.path));
            return;
        }
    }

    const int skippedCount = filterUnchangedUpdates(m_instance->gameRoot(), m_items);
    if (skippedCount > 0) {
        qDebug() << "Delta sync skipped" << skippedCount << "up-to-date files matching local SHA-1.";
    }

    if (m_items.isEmpty()) {
        m_instance->setManagedPackVersionID(m_targetVersion);
        m_instance->setManagedPackVersionName(m_targetVersion);
        m_instance->saveNow();
        persistManagedIndex();
        emitSucceeded();
        return;
    }

    startStagedDownload();
}

#else

DeltaSyncTask::DeltaSyncTask(BaseInstance* instance,
                             const QJsonObject& manifest,
                             const QString& targetVersion,
                             QObject* parent)
    : Task(), m_instance(instance), m_manifest(manifest), m_targetVersion(targetVersion)
{
    if (parent) {
        setParent(parent);
    }
}
DeltaSyncTask::DeltaSyncTask(BaseInstance* instance,
                             const ZipDeltaPlan& zipPlan,
                             QNetworkAccessManager* network,
                             QObject* parent)
    : Task(), m_instance(instance), m_targetVersion(zipPlan.targetVersion), m_useZipPlan(true), m_zipPlan(zipPlan), m_network(network)
{
    if (parent) {
        setParent(parent);
    }
}
DeltaSyncTask::~DeltaSyncTask() = default;
bool DeltaSyncTask::abort() { return Task::abort(); }
void DeltaSyncTask::executeTask() { emitFailed("Not supported"); }
void DeltaSyncTask::onDownloadJobSucceeded() {}
void DeltaSyncTask::onDownloadJobFailed(const QString&) {}
void DeltaSyncTask::onDownloadJobAborted() {}
bool DeltaSyncTask::parseManifest() { return false; }
bool DeltaSyncTask::validatePathSecurity(const QString&) const { return false; }
void DeltaSyncTask::startStagedDownload() {}
void DeltaSyncTask::startZipPlanDownload() {}
bool DeltaSyncTask::validateDownloadedFiles() { return false; }
bool DeltaSyncTask::executeRollbackJournalAndCommit() { return false; }
void DeltaSyncTask::rollbackJournal(const QStringList&) {}
void DeltaSyncTask::persistManagedIndex() {}
void DeltaSyncTask::cleanupTemporaryDirs() {}

#endif
