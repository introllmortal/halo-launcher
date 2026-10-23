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
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QUrl>

#include <archive.h>
#include <archive_entry.h>

#if defined(LAUNCHER_APPLICATION)
#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "net/ChecksumValidator.h"
#include "net/Download.h"

static bool writeEntryToArchive(struct archive* a, const QString& fullPath, const QString& relPath)
{
    QFileInfo fi(fullPath);
    if (!fi.exists() || !fi.isFile()) {
        return false;
    }

    struct archive_entry* entry = archive_entry_new();
    if (!entry) {
        return false;
    }
    archive_entry_set_pathname_utf8(entry, relPath.toUtf8().constData());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_size(entry, fi.size());
    archive_entry_set_mtime(entry, fi.lastModified().toSecsSinceEpoch(), 0);
    archive_entry_set_perm(entry, 0644);

    if (archive_write_header(a, entry) != ARCHIVE_OK) {
        archive_entry_free(entry);
        return false;
    }
    archive_entry_free(entry);

    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    constexpr qint64 bufSize = 65536;
    QByteArray buffer(bufSize, Qt::Uninitialized);
    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer.data(), bufSize);
        if (bytesRead <= 0 || archive_write_data(a, buffer.constData(), bytesRead) < 0) {
            return false;
        }
    }
    return true;
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

DeltaSyncTask::~DeltaSyncTask()
{
    cleanupTemporaryDirs();
}

bool DeltaSyncTask::abort()
{
    if (m_downloadJob && m_downloadJob->isRunning()) {
        m_downloadJob->abort();
    }
    cleanupTemporaryDirs();
    return Task::abort();
}

bool DeltaSyncTask::validatePathSecurity(const QString& rawPath) const
{
    if (rawPath.isEmpty() || rawPath.startsWith('/') || rawPath.startsWith('\\') ||
        rawPath.contains("..") || rawPath.contains(':')) {
        return false;
    }

    QString clean = QDir::cleanPath(QString(rawPath).replace('\\', '/'));
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
        "patches/", "saves/", "screenshots/", "backups/", ".delta_staging/", ".delta_backup/"
    };
    for (const auto& prefix : forbiddenPrefixes) {
        if (clean.startsWith(prefix, Qt::CaseInsensitive)) {
            return false;
        }
    }

    return clean.startsWith("mods/", Qt::CaseInsensitive) ||
           clean.startsWith("kubejs/", Qt::CaseInsensitive) ||
           clean.startsWith("config/", Qt::CaseInsensitive);
}

bool DeltaSyncTask::parseManifest()
{
    m_items.clear();
    QSet<QString> seen;

    auto addParsed = [this, &seen](const QString& path, const QString& sha1, qint64 size,
                                   const QString& url, DeltaFileItem::Action action) {
        if (path.isEmpty()) return;
        DeltaFileItem item{path, sha1, size, url, action};
        if (seen.contains(path)) {
            for (auto& existing : m_items) {
                if (existing.path == path) {
                    existing = item;
                    return;
                }
            }
        }
        seen.insert(path);
        m_items.append(item);
    };

    auto parseObject = [&](const QJsonObject& obj, DeltaFileItem::Action defaultAction) {
        QString path = obj.value("path").toString(obj.value("file").toString());
        QString sha1 = obj.value("sha1").toString(obj.value("hash").toString());
        qint64 size = obj.contains("size") ? obj.value("size").toVariant().toLongLong()
                                           : obj.value("fileSize").toVariant().toLongLong();
        QString url = obj.value("url").toString(obj.value("downloadUrl").toString());
        QString act = obj.value("action").toString().toLower();
        auto action = (act == "remove" || act == "delete") ? DeltaFileItem::Action::Remove : defaultAction;
        addParsed(path, sha1, size, url, action);
    };

    for (const auto& key : {"files", "updates"}) {
        if (m_manifest.contains(key) && m_manifest.value(key).isArray()) {
            for (const auto& val : m_manifest.value(key).toArray()) {
                if (val.isObject()) parseObject(val.toObject(), DeltaFileItem::Action::Update);
            }
        }
    }
    for (const auto& key : {"removals", "remove"}) {
        if (m_manifest.contains(key) && m_manifest.value(key).isArray()) {
            for (const auto& val : m_manifest.value(key).toArray()) {
                if (val.isObject()) {
                    parseObject(val.toObject(), DeltaFileItem::Action::Remove);
                } else if (val.isString()) {
                    addParsed(val.toString(), "", 0, "", DeltaFileItem::Action::Remove);
                }
            }
        }
    }
    return true;
}

void DeltaSyncTask::rotateBackups(const QString& backupsDir, int maxKeep)
{
    QDir dir(backupsDir);
    QFileInfoList list = dir.entryInfoList(QStringList() << "auto_pre_update_*.zip", QDir::Files, QDir::Name);
    while (list.size() > maxKeep) {
        QFile::remove(list.first().absoluteFilePath());
        list.removeFirst();
    }
}

bool DeltaSyncTask::createPreUpdateSnapshot()
{
    QString gameRoot = m_instance->gameRoot();
    QString backupsDir = FS::PathCombine(gameRoot, "backups");
    if (!QDir().mkpath(backupsDir)) {
        return false;
    }

    rotateBackups(backupsDir, 2);

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString archivePath = FS::PathCombine(backupsDir, QString("auto_pre_update_%1.zip").arg(timestamp));

    struct archive* a = archive_write_new();
    if (!a) {
        return false;
    }
    archive_write_set_format_by_name(a, "zip");
    archive_write_set_options(a, "zip:compression=store");
    archive_write_set_options(a, "hdrcharset=UTF-8");

#if defined(Q_OS_WIN)
    int r = archive_write_open_filename_w(a, archivePath.toStdWString().data());
#else
    int r = archive_write_open_filename(a, archivePath.toUtf8().constData());
#endif
    if (r != ARCHIVE_OK) {
        archive_write_free(a);
        return false;
    }

    QString optionsFile = FS::PathCombine(gameRoot, "options.txt");
    if (QFile::exists(optionsFile)) {
        writeEntryToArchive(a, optionsFile, "options.txt");
    }

    QString savesDir = FS::PathCombine(gameRoot, "saves");
    QDir saves(savesDir);
    if (saves.exists()) {
        QDirIterator it(savesDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            writeEntryToArchive(a, it.filePath(), "saves/" + saves.relativeFilePath(it.filePath()));
        }
    }

    archive_write_close(a);
    archive_write_free(a);

    rotateBackups(backupsDir, 3);
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

        QString stagedPath = FS::PathCombine(m_stagingDir, item.path);
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
            QFile file(stagedPath);
            if (!file.open(QIODevice::ReadOnly)) {
                cleanupTemporaryDirs();
                emitFailed(tr("Validation failed: Cannot open '%1' for hash verification. Please use full archive import.").arg(item.path));
                return false;
            }

            QCryptographicHash hash(QCryptographicHash::Sha1);
            if (!hash.addData(&file)) {
                cleanupTemporaryDirs();
                emitFailed(tr("Validation failed: Error computing hash for '%1'. Please use full archive import.").arg(item.path));
                return false;
            }

            QString actualSha1 = QString::fromLatin1(hash.result().toHex());
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

void DeltaSyncTask::rollbackJournal(const QStringList& movedFiles)
{
    setStatus(tr("Rolling back changes..."));
    QString gameRoot = m_instance->gameRoot();

    for (const QString& relPath : movedFiles) {
        QString backupPath = FS::PathCombine(m_backupDir, relPath);
        QString originalPath = FS::PathCombine(gameRoot, relPath);
        if (QFile::exists(backupPath)) {
            FS::ensureFilePathExists(originalPath);
            if (QFile::exists(originalPath)) {
                QFile::remove(originalPath);
            }
            FS::move(backupPath, originalPath);
        }
    }

    cleanupTemporaryDirs();
    emitFailed(tr("File locked or access denied. Delta sync rolled back. Please use full archive import."));
}

bool DeltaSyncTask::executeRollbackJournalAndCommit()
{
    QString gameRoot = m_instance->gameRoot();

    setStatus(tr("Preparing rollback journal..."));
    if (QDir(m_backupDir).exists()) {
        FS::deletePath(m_backupDir);
    }
    if (!QDir().mkpath(m_backupDir)) {
        cleanupTemporaryDirs();
        emitFailed(tr("File locked or access denied. Delta sync rolled back. Please use full archive import."));
        return false;
    }

    QStringList movedFiles;
    for (const auto& item : m_items) {
        QString targetPath = FS::PathCombine(gameRoot, item.path);
        if (QFile::exists(targetPath)) {
            QString backupPath = FS::PathCombine(m_backupDir, item.path);
            FS::ensureFilePathExists(backupPath);
            if (QFile::exists(backupPath)) {
                QFile::remove(backupPath);
            }
            if (!FS::move(targetPath, backupPath)) {
                rollbackJournal(movedFiles);
                return false;
            }
            movedFiles.append(item.path);
        }
    }

    setStatus(tr("Committing updates..."));
    for (const auto& item : m_items) {
        if (item.action == DeltaFileItem::Action::Update) {
            QString stagedPath = FS::PathCombine(m_stagingDir, item.path);
            QString targetPath = FS::PathCombine(gameRoot, item.path);
            if (QFile::exists(stagedPath)) {
                FS::ensureFilePathExists(targetPath);
                if (QFile::exists(targetPath)) {
                    QFile::remove(targetPath);
                }
                if (!FS::move(stagedPath, targetPath)) {
                    rollbackJournal(movedFiles);
                    return false;
                }
            }
        } else if (item.action == DeltaFileItem::Action::Remove) {
            QString targetPath = FS::PathCombine(gameRoot, item.path);
            if (QFile::exists(targetPath)) {
                QFile::remove(targetPath);
            }
        }
    }

    m_instance->setManagedPackVersionID(m_targetVersion);
    m_instance->setManagedPackVersionName(m_targetVersion);
    m_instance->saveNow();

    cleanupTemporaryDirs();
    emitSucceeded();
    return true;
}

void DeltaSyncTask::cleanupTemporaryDirs()
{
    if (!m_stagingDir.isEmpty() && QDir(m_stagingDir).exists()) {
        FS::deletePath(m_stagingDir);
    }
    if (!m_backupDir.isEmpty() && QDir(m_backupDir).exists()) {
        FS::deletePath(m_backupDir);
    }
}

void DeltaSyncTask::executeTask()
{
    if (!m_instance) {
        emitFailed(tr("Instance is invalid."));
        return;
    }

    m_stagingDir = FS::PathCombine(m_instance->gameRoot(), ".delta_staging");
    m_backupDir = FS::PathCombine(m_instance->gameRoot(), ".delta_backup");

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

    setStatus(tr("Creating pre-update backup snapshot..."));
    if (!createPreUpdateSnapshot()) {
        logWarning("Pre-update snapshot completed with warnings or was skipped.");
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
DeltaSyncTask::~DeltaSyncTask() = default;
bool DeltaSyncTask::abort() { return Task::abort(); }
void DeltaSyncTask::executeTask() { emitFailed("Not supported"); }
void DeltaSyncTask::onDownloadJobSucceeded() {}
void DeltaSyncTask::onDownloadJobFailed(const QString&) {}
void DeltaSyncTask::onDownloadJobAborted() {}
bool DeltaSyncTask::parseManifest() { return false; }
bool DeltaSyncTask::validatePathSecurity(const QString&) const { return false; }
bool DeltaSyncTask::createPreUpdateSnapshot() { return false; }
void DeltaSyncTask::rotateBackups(const QString&, int) {}
void DeltaSyncTask::startStagedDownload() {}
bool DeltaSyncTask::validateDownloadedFiles() { return false; }
bool DeltaSyncTask::executeRollbackJournalAndCommit() { return false; }
void DeltaSyncTask::rollbackJournal(const QStringList&) {}
void DeltaSyncTask::cleanupTemporaryDirs() {}

#endif
