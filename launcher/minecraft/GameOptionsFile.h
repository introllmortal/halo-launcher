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

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

class BaseInstance;

/**
 * Safe line-preserving reader and atomic writer for Minecraft's options.txt.
 * Unknown/mod-added keys (e.g. key_* bindings) and original ordering are preserved.
 */
class GameOptionsFile {
   public:
    struct Entry {
        QString key;
        QString rawValue;
        bool isRawLine = false;  // true for blank/malformed lines preserved verbatim
    };

    GameOptionsFile() = default;

    bool loadFromFile(const QString& filePath, QString* errorOut = nullptr);
    bool saveToFile(const QString& filePath, bool createBackup = true, QString* errorOut = nullptr) const;
    bool saveForInstance(const BaseInstance* instance, const QString& filePath, QString* errorOut = nullptr) const;

    bool isLoaded() const { return m_loaded; }
    bool contains(const QString& key) const;
    QString rawValue(const QString& key, const QString& defaultValue = {}) const;
    void setRawValue(const QString& key, const QString& value);
    void removeKey(const QString& key);

    std::optional<bool> boolValue(const QString& key) const;
    void setBoolValue(const QString& key, bool value);

    std::optional<int> intValue(const QString& key) const;
    void setIntValue(const QString& key, int value);

    std::optional<double> doubleValue(const QString& key) const;
    void setDoubleValue(const QString& key, double value);

    QStringList stringListValue(const QString& key) const;
    void setStringListValue(const QString& key, const QStringList& values);

    QList<Entry> entries() const { return m_entries; }
    QStringList keys() const;

    static QString optionsFilePath(const BaseInstance* instance);

   private:
    QList<Entry> m_entries;
    QHash<QString, int> m_keyIndex;
    bool m_loaded = false;
};
