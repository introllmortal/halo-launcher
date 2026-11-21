// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "AccountList.h"
#include "AccountData.h"

#include <FileSystem.h>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>

namespace {
constexpr int kAccountListVersion = 3;

bool loadJsonFromPath(const QString& path, QJsonObject& outRoot, bool* outUnknownFormat = nullptr)
{
    if (outUnknownFormat) {
        *outUnknownFormat = false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << QString("Failed to read the account list file %1 (%2).").arg(path, file.errorString()).toUtf8();
        return false;
    }
    const QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qCritical() << QString("Failed to parse account list file: %1 at offset %2")
                           .arg(parseError.errorString(), QString::number(parseError.offset))
                           .toUtf8();
        return false;
    }
    if (!jsonDoc.isObject()) {
        qCritical() << "Invalid account list JSON: Root should be an object.";
        return false;
    }
    outRoot = jsonDoc.object();
    if (outRoot.value("formatVersion").toVariant().toInt() != kAccountListVersion) {
        if (outUnknownFormat) {
            *outUnknownFormat = true;
        }
        return false;
    }
    return true;
}
}  // namespace

bool AccountList::loadList()
{
    if (m_listFilePath.isEmpty()) {
        qCritical() << "Can't load Mojang account list. No file path given and no default set.";
        return false;
    }

    QJsonObject root;
    bool unknownFormat = false;
    if (loadJsonFromPath(m_listFilePath, root, &unknownFormat)) {
        return loadV3(root);
    }

    // Attempt recovery from accounts.json.bak if primary file is missing or corrupted.
    const QString backupPath = m_listFilePath + QStringLiteral(".bak");
    if (QFileInfo::exists(backupPath)) {
        if (loadJsonFromPath(backupPath, root)) {
            qWarning() << "Recovered account list from backup:" << backupPath;
            return loadV3(root);
        }
    }

    if (unknownFormat) {
        QFile file(m_listFilePath);
        QString newName = "accounts-old.json";
        qWarning() << "Unknown format version when loading account list. Existing one will be renamed to" << newName;
        file.rename(newName);
    }
    return false;
}

bool AccountList::loadV3(QJsonObject& root)
{
    beginResetModel();
    QJsonArray accounts = root.value("accounts").toArray();
    for (QJsonValue accountVal : accounts) {
        QJsonObject accountObj = accountVal.toObject();
        MinecraftAccountPtr account = MinecraftAccount::loadFromJsonV3(accountObj);
        if (account.get() != nullptr) {
            auto profileId = account->profileId();
            if (profileId.size()) {
                if (findAccountByProfileId(profileId) != -1) {
                    continue;
                }
            }
            connect(account.get(), &MinecraftAccount::changed, this, &AccountList::accountChanged);
            connect(account.get(), &MinecraftAccount::activityChanged, this, &AccountList::accountActivityChanged);
            m_accounts.append(account);
            if (accountObj.value("active").toBool(false)) {
                m_defaultAccount = account;
            }
        } else {
            qWarning() << "Failed to load an account.";
        }
    }
    endResetModel();
    return true;
}

bool AccountList::saveList()
{
    if (m_listFilePath.isEmpty()) {
        qCritical() << "Can't save Mojang account list. No file path given and no default set.";
        return false;
    }

    if (!FS::ensureFilePathExists(m_listFilePath))
        return false;

    QFileInfo finfo(m_listFilePath);
    if (finfo.isDir()) {
        QDir badDir(m_listFilePath);
        badDir.removeRecursively();
    }

    // Keep an atomic backup of the last valid accounts.json before overwriting.
    if (finfo.exists() && finfo.isFile() && finfo.size() > 0 && !finfo.isSymLink()) {
        QFile existing(m_listFilePath);
        if (existing.open(QIODevice::ReadOnly)) {
            const QByteArray existingBytes = existing.readAll();
            existing.close();
            const QJsonDocument existingDoc = QJsonDocument::fromJson(existingBytes);
            if (existingDoc.isObject() &&
                existingDoc.object().value("formatVersion").toVariant().toInt() == kAccountListVersion) {
                QSaveFile backupFile(m_listFilePath + QStringLiteral(".bak"));
                if (backupFile.open(QIODevice::WriteOnly)) {
                    backupFile.write(existingBytes);
                    backupFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
                    if (!backupFile.commit()) {
                        qWarning() << "Failed to commit account list backup:" << backupFile.errorString();
                    }
                }
            }
        }
    }

    qDebug() << "Writing account list to" << m_listFilePath;

    QJsonObject root;
    root.insert("formatVersion", kAccountListVersion);

    QJsonArray accounts;
    for (MinecraftAccountPtr account : m_accounts) {
        QJsonObject accountObj = account->saveToJson();
        if (m_defaultAccount == account) {
            accountObj["active"] = true;
        }
        accounts.append(accountObj);
    }
    root.insert("accounts", accounts);

    QJsonDocument doc(root);
    QSaveFile file(m_listFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << QString("Failed to save the account list file %1 (%2).").arg(m_listFilePath, file.errorString()).toUtf8();
        return false;
    }

    file.write(doc.toJson());
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ReadUser | QFile::WriteUser);
    if (file.commit()) {
        qDebug() << "Saved account list to" << m_listFilePath;
        return true;
    }
    qDebug() << "Failed to save accounts to" << m_listFilePath << "error:" << file.errorString();
    return false;
}

void AccountList::setListFilePath(QString path, bool autosave)
{
    m_listFilePath = path;
    m_autosave = autosave;
}
