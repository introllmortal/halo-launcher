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
#include "GraphicsModConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>

#include "FileSystem.h"

namespace {
QString gfxTr(const char* text)
{
    return QCoreApplication::translate("GraphicsModConfigManager", text);
}

void flattenJsonObject(const QJsonObject& obj, const QString& prefix, QList<GraphicsModConfigManager::Entry>& out)
{
    QStringList keys = obj.keys();
    std::sort(keys.begin(), keys.end(), [](const QString& a, const QString& b) {
        return QString::compare(a, b, Qt::CaseInsensitive) < 0;
    });
    for (const QString& key : keys) {
        const QString fullKey = prefix.isEmpty() ? key : (prefix + QLatin1Char('.') + key);
        const QJsonValue val = obj.value(key);
        if (val.isObject()) {
            flattenJsonObject(val.toObject(), fullKey, out);
        } else if (val.isBool()) {
            out.append({ fullKey, val.toBool() });
        } else if (val.isDouble()) {
            const double d = val.toDouble();
            const qint64 asInt = static_cast<qint64>(d);
            if (static_cast<double>(asInt) == d) {
                out.append({ fullKey, static_cast<int>(asInt) });
            } else {
                out.append({ fullKey, d });
            }
        } else if (val.isString()) {
            out.append({ fullKey, val.toString() });
        }
    }
}

void setJsonPath(QJsonObject& root, const QStringList& segments, int idx, const QVariant& value)
{
    if (idx < 0 || idx >= segments.size()) {
        return;
    }
    const QString& seg = segments.at(idx);
    if (idx == segments.size() - 1) {
        if (value.typeId() == QMetaType::Bool) {
            root.insert(seg, value.toBool());
        } else if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::LongLong) {
            root.insert(seg, value.toLongLong());
        } else if (value.typeId() == QMetaType::Double) {
            root.insert(seg, value.toDouble());
        } else {
            root.insert(seg, value.toString());
        }
        return;
    }
    QJsonObject child = root.value(seg).toObject();
    setJsonPath(child, segments, idx + 1, value);
    root.insert(seg, child);
}

QVariant parsePropertiesScalar(const QString& raw)
{
    const QString trimmed = raw.trimmed();
    if (trimmed.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (trimmed.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    bool okInt = false;
    const int iVal = trimmed.toInt(&okInt);
    if (okInt) {
        return iVal;
    }
    bool okDbl = false;
    const double dVal = trimmed.toDouble(&okDbl);
    if (okDbl) {
        return dVal;
    }
    return raw;
}

QString formatPropertiesScalar(const QVariant& val)
{
    if (val.typeId() == QMetaType::Bool) {
        return val.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    return val.toString();
}

bool writeAtomic(const QString& filePath, const QByteArray& bytes, QString* errorOut)
{
    const QFileInfo info(filePath);
    if (info.isSymLink()) {
        if (errorOut) {
            *errorOut = gfxTr("Refusing to write through symlink: %1").arg(filePath);
        }
        return false;
    }
    QDir().mkpath(info.absolutePath());
    QSaveFile saver(filePath);
    if (!saver.open(QIODevice::WriteOnly)) {
        if (errorOut) {
            *errorOut = gfxTr("Failed to open %1: %2").arg(filePath, saver.errorString());
        }
        return false;
    }
    if (saver.write(bytes) != bytes.size() || !saver.commit()) {
        if (errorOut) {
            *errorOut = gfxTr("Failed to save %1: %2").arg(filePath, saver.errorString());
        }
        return false;
    }
    return true;
}
}  // namespace

QList<GraphicsModConfigManager::ConfigFileInfo> GraphicsModConfigManager::discoverConfigs(const QString& gameRoot)
{
    if (gameRoot.isEmpty()) {
        return {};
    }
    const QString cfgDir = FS::PathCombine(gameRoot, QStringLiteral("config"));
    if (!QFileInfo::exists(cfgDir)) {
        return {};
    }

    struct KnownSpec {
        const char* fileName;
        const char* id;
        const char* displayName;
        Format format;
        bool isSodiumFamily;
    };
    static constexpr KnownSpec kSpecs[] = {
        { "sodium-options.json", "sodium", "Sodium", Format::Json, true },
        { "embeddium-options.json", "embeddium", "Embeddium", Format::Json, true },
        { "rubidium-options.json", "rubidium", "Rubidium", Format::Json, true },
        { "sodium-extra-options.json", "sodium-extra", "Sodium Extra", Format::Json, false },
        { "embeddium-extra.json", "embeddium-extra", "Embeddium Extra", Format::Json, false },
        { "iridium.json", "iridium-json", "Iridium", Format::Json, false },
        { "iridium.properties", "iridium-prop", "Iridium", Format::Properties, false },
        { "immediatelyfast.json", "immediatelyfast", "ImmediatelyFast", Format::Json, false },
        { "entityculling.json", "entityculling", "EntityCulling", Format::Json, false },
        { "iris.properties", "iris", "Iris", Format::Properties, false },
        { "oculus.properties", "oculus", "Oculus", Format::Properties, false },
    };

    QList<ConfigFileInfo> out;
    for (const auto& spec : kSpecs) {
        const QString fullPath = FS::PathCombine(cfgDir, QLatin1String(spec.fileName));
        const QFileInfo fi(fullPath);
        if (fi.exists() && fi.isFile() && !fi.isSymLink()) {
            out.append({ QLatin1String(spec.id), QLatin1String(spec.displayName), fullPath, spec.format, spec.isSodiumFamily });
        }
    }
    return out;
}

QList<GraphicsModConfigManager::Entry> GraphicsModConfigManager::loadEntries(const QString& filePath,
                                                                              Format format,
                                                                              QString* errorOut)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorOut) {
            *errorOut = gfxTr("Failed to open %1: %2").arg(filePath, file.errorString());
        }
        return {};
    }
    const QByteArray data = file.readAll();

    QList<Entry> out;
    if (format == Format::Json) {
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            if (errorOut) {
                *errorOut = gfxTr("Invalid JSON in %1: %2").arg(filePath, err.errorString());
            }
            return {};
        }
        flattenJsonObject(doc.object(), {}, out);
        return out;
    }

    const QStringList lines = QString::fromUtf8(data).split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        QString line = rawLine;
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0) {
            continue;
        }
        out.append({ line.left(eq).trimmed(), parsePropertiesScalar(line.mid(eq + 1)) });
    }
    return out;
}

bool GraphicsModConfigManager::saveEntries(const QString& filePath,
                                           Format format,
                                           const QList<Entry>& entries,
                                           QString* errorOut)
{
    if (format == Format::Json) {
        QJsonObject root;
        if (QFileInfo::exists(filePath)) {
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly)) {
                if (errorOut) {
                    *errorOut = gfxTr("Failed to read %1: %2").arg(filePath, file.errorString());
                }
                return false;
            }
            QJsonParseError err{};
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                if (errorOut) {
                    *errorOut = gfxTr("Invalid JSON in %1: %2").arg(filePath, err.errorString());
                }
                return false;
            }
            root = doc.object();
        }
        for (const Entry& entry : entries) {
            const QStringList segments = entry.keyPath.split(QLatin1Char('.'), Qt::SkipEmptyParts);
            setJsonPath(root, segments, 0, entry.value);
        }
        return writeAtomic(filePath, QJsonDocument(root).toJson(QJsonDocument::Indented), errorOut);
    }

    QStringList lines;
    if (QFileInfo::exists(filePath)) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            if (errorOut) {
                *errorOut = gfxTr("Failed to read %1: %2").arg(filePath, file.errorString());
            }
            return false;
        }
        lines = QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'));
    }

    QHash<QString, QString> updates;
    for (const Entry& entry : entries) {
        updates.insert(entry.keyPath, formatPropertiesScalar(entry.value));
    }

    QByteArray out;
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (i == lines.size() - 1 && line.isEmpty()) {
            continue;
        }
        const QString trimmed = line.trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1Char('#')) && eq > 0) {
            const QString key = line.left(eq).trimmed();
            if (updates.contains(key)) {
                out.append(key.toUtf8());
                out.append('=');
                out.append(updates.take(key).toUtf8());
                out.append('\n');
                continue;
            }
        }
        out.append(line.toUtf8());
        out.append('\n');
    }
    for (auto it = updates.constBegin(); it != updates.constEnd(); ++it) {
        out.append(it.key().toUtf8());
        out.append('=');
        out.append(it.value().toUtf8());
        out.append('\n');
    }
    return writeAtomic(filePath, out, errorOut);
}
