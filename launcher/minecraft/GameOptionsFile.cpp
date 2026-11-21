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
#include "GameOptionsFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include "BaseInstance.h"
#include "FileSystem.h"

namespace {
QString optionsTr(const char* text)
{
    return QCoreApplication::translate("GameOptionsFile", text);
}
}  // namespace

bool GameOptionsFile::loadFromFile(const QString& filePath, QString* errorOut)
{
    m_entries.clear();
    m_keyIndex.clear();
    m_loaded = false;

    if (filePath.isEmpty()) {
        if (errorOut) {
            *errorOut = optionsTr("Options file path is empty.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.exists()) {
        // Missing options.txt before first launch is valid; start empty so callers can still write defaults.
        m_loaded = true;
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = optionsTr("Failed to open %1: %2").arg(filePath, file.errorString());
        }
        return false;
    }

    const QString text = QString::fromUtf8(file.readAll());
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (i == lines.size() - 1 && line.isEmpty()) {
            continue;
        }
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon <= 0) {
            m_entries.append({ line, QString(), true });
            continue;
        }
        const QString key = line.left(colon);
        const QString val = line.mid(colon + 1);
        if (m_keyIndex.contains(key)) {
            m_entries[m_keyIndex.value(key)].rawValue = val;
        } else {
            m_keyIndex.insert(key, m_entries.size());
            m_entries.append({ key, val, false });
        }
    }

    m_loaded = true;
    return true;
}

bool GameOptionsFile::saveToFile(const QString& filePath, bool createBackup, QString* errorOut) const
{
    if (filePath.isEmpty()) {
        if (errorOut) {
            *errorOut = optionsTr("Options file path is empty.");
        }
        return false;
    }

    const QFileInfo info(filePath);
    if (info.isSymLink()) {
        if (errorOut) {
            *errorOut = optionsTr("Refusing to write through symlink: %1").arg(filePath);
        }
        return false;
    }
    QDir().mkpath(info.absolutePath());

    if (createBackup && info.exists()) {
        const QString backupPath = filePath + QStringLiteral(".bak");
        if (!QFileInfo::exists(backupPath)) {
            QFile::copy(filePath, backupPath);
        }
    }

    QByteArray out;
    for (const Entry& entry : m_entries) {
        if (entry.isRawLine) {
            out.append(entry.key.toUtf8());
        } else {
            out.append(entry.key.toUtf8());
            out.append(':');
            out.append(entry.rawValue.toUtf8());
        }
        out.append('\n');
    }

    QSaveFile saver(filePath);
    if (!saver.open(QIODevice::WriteOnly)) {
        if (errorOut) {
            *errorOut = optionsTr("Failed to open %1 for writing: %2").arg(filePath, saver.errorString());
        }
        return false;
    }
    if (saver.write(out) != out.size() || !saver.commit()) {
        if (errorOut) {
            *errorOut = optionsTr("Failed to save %1: %2").arg(filePath, saver.errorString());
        }
        return false;
    }
    return true;
}

bool GameOptionsFile::saveForInstance(const BaseInstance* instance, const QString& filePath, QString* errorOut) const
{
    if (!instance) {
        if (errorOut) {
            *errorOut = optionsTr("Instance is null.");
        }
        return false;
    }
    if (instance->isRunning()) {
        if (errorOut) {
            *errorOut = optionsTr("Cannot edit game options while the instance is running.");
        }
        return false;
    }
    return saveToFile(filePath, true, errorOut);
}

bool GameOptionsFile::contains(const QString& key) const
{
    return m_keyIndex.contains(key);
}

QString GameOptionsFile::rawValue(const QString& key, const QString& defaultValue) const
{
    const auto it = m_keyIndex.constFind(key);
    if (it == m_keyIndex.constEnd()) {
        return defaultValue;
    }
    return m_entries.at(it.value()).rawValue;
}

void GameOptionsFile::setRawValue(const QString& key, const QString& value)
{
    if (key.isEmpty()) {
        return;
    }
    const auto it = m_keyIndex.constFind(key);
    if (it != m_keyIndex.constEnd()) {
        m_entries[it.value()].rawValue = value;
        m_entries[it.value()].isRawLine = false;
        return;
    }
    m_keyIndex.insert(key, m_entries.size());
    m_entries.append({ key, value, false });
}

void GameOptionsFile::removeKey(const QString& key)
{
    const auto it = m_keyIndex.constFind(key);
    if (it == m_keyIndex.constEnd()) {
        return;
    }
    const int idx = it.value();
    m_entries.removeAt(idx);
    m_keyIndex.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        if (!m_entries.at(i).isRawLine) {
            m_keyIndex.insert(m_entries.at(i).key, i);
        }
    }
}

std::optional<bool> GameOptionsFile::boolValue(const QString& key) const
{
    if (!contains(key)) {
        return std::nullopt;
    }
    const QString v = rawValue(key).trimmed().toLower();
    if (v == QStringLiteral("true")) {
        return true;
    }
    if (v == QStringLiteral("false")) {
        return false;
    }
    return std::nullopt;
}

void GameOptionsFile::setBoolValue(const QString& key, bool value)
{
    setRawValue(key, value ? QStringLiteral("true") : QStringLiteral("false"));
}

std::optional<int> GameOptionsFile::intValue(const QString& key) const
{
    if (!contains(key)) {
        return std::nullopt;
    }
    bool ok = false;
    const int val = rawValue(key).trimmed().toInt(&ok);
    return ok ? std::optional<int>(val) : std::nullopt;
}

void GameOptionsFile::setIntValue(const QString& key, int value)
{
    setRawValue(key, QString::number(value));
}

std::optional<double> GameOptionsFile::doubleValue(const QString& key) const
{
    if (!contains(key)) {
        return std::nullopt;
    }
    bool ok = false;
    const double val = rawValue(key).trimmed().toDouble(&ok);
    return ok ? std::optional<double>(val) : std::nullopt;
}

void GameOptionsFile::setDoubleValue(const QString& key, double value)
{
    setRawValue(key, QString::number(value, 'g', 6));
}

QStringList GameOptionsFile::stringListValue(const QString& key) const
{
    const QString raw = rawValue(key).trimmed();
    if (raw.isEmpty()) {
        return {};
    }
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        return {};
    }
    QStringList result;
    const QJsonArray arr = doc.array();
    result.reserve(arr.size());
    for (const QJsonValue& val : arr) {
        if (val.isString()) {
            result.append(val.toString());
        }
    }
    return result;
}

void GameOptionsFile::setStringListValue(const QString& key, const QStringList& values)
{
    QJsonArray arr;
    for (const QString& item : values) {
        arr.append(item);
    }
    const QByteArray compact = QJsonDocument(arr).toJson(QJsonDocument::Compact);
    setRawValue(key, QString::fromUtf8(compact));
}

QStringList GameOptionsFile::keys() const
{
    QStringList out;
    out.reserve(m_entries.size());
    for (const Entry& entry : m_entries) {
        if (!entry.isRawLine) {
            out.append(entry.key);
        }
    }
    return out;
}

QString GameOptionsFile::optionsFilePath(const BaseInstance* instance)
{
    if (!instance || instance->gameRoot().isEmpty()) {
        return {};
    }
    return FS::PathCombine(instance->gameRoot(), QStringLiteral("options.txt"));
}
