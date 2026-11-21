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
#include "InstanceProfilePreset.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <algorithm>
#include <utility>

#include "BaseInstance.h"
#include "FileSystem.h"

namespace {

constexpr int kMaxSlugLength = 64;

void setError(QString* errorOut, const QString& message)
{
    if (errorOut) {
        *errorOut = message;
    }
}

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

bool isValidPresetId(const QString& id)
{
    if (id.isEmpty() || id.startsWith(QLatin1Char('.'))) {
        return false;
    }
    if (id.contains(QLatin1Char('/')) || id.contains(QLatin1Char('\\')) || id.contains(QStringLiteral(".."))) {
        return false;
    }
    if (id.compare(QStringLiteral("active"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    return true;
}

bool isValidModFileName(const QString& fileName)
{
    if (fileName.isEmpty() || fileName == QLatin1String(".") || fileName == QLatin1String("..")) {
        return false;
    }
    if (fileName.contains(QLatin1Char('/')) || fileName.contains(QLatin1Char('\\'))) {
        return false;
    }
    return true;
}

QString slugifyDisplayName(const QString& displayName)
{
    const QString trimmed = displayName.trimmed();
    QString slug;
    slug.reserve(trimmed.size());

    for (const QChar ch : trimmed.toLower()) {
        const bool isAsciiAlphaNum =
            (ch >= QLatin1Char('a') && ch <= QLatin1Char('z')) || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'));
        if (isAsciiAlphaNum || ch == QLatin1Char('_')) {
            slug.append(ch);
        } else if (ch == QLatin1Char('-') || !slug.isEmpty()) {
            if (!slug.endsWith(QLatin1Char('-'))) {
                slug.append(QLatin1Char('-'));
            }
        }
    }

    while (slug.startsWith(QLatin1Char('-')) || slug.startsWith(QLatin1Char('_'))) {
        slug.remove(0, 1);
    }
    while (slug.endsWith(QLatin1Char('-')) || slug.endsWith(QLatin1Char('_'))) {
        slug.chop(1);
    }

    if (slug.size() > kMaxSlugLength) {
        slug.truncate(kMaxSlugLength);
        while (slug.endsWith(QLatin1Char('-')) || slug.endsWith(QLatin1Char('_'))) {
            slug.chop(1);
        }
    }

    if (slug.isEmpty()) {
        const QByteArray digest = QCryptographicHash::hash(trimmed.toUtf8(), QCryptographicHash::Sha1).toHex();
        slug = QStringLiteral("preset-") + QString::fromLatin1(digest.left(8));
    }
    if (slug.compare(QStringLiteral("active"), Qt::CaseInsensitive) == 0) {
        slug = QStringLiteral("active-preset");
    }
    return slug;
}

QString activeFilePath(const BaseInstance* instance)
{
    const QString root = InstanceProfilePreset::presetsRoot(instance);
    if (root.isEmpty()) {
        return {};
    }
    return FS::PathCombine(root, QStringLiteral("active.json"));
}

QString presetFilePath(const BaseInstance* instance, const QString& id)
{
    const QString root = InstanceProfilePreset::presetsRoot(instance);
    if (root.isEmpty() || !isValidPresetId(id)) {
        return {};
    }
    return FS::PathCombine(root, id + QStringLiteral(".json"));
}

QStringList readModList(const QJsonArray& array)
{
    QStringList result;
    QSet<QString> seen;
    result.reserve(array.size());
    for (const QJsonValue& val : array) {
        if (!val.isString()) {
            continue;
        }
        const QString modName = val.toString();
        if (!isValidModFileName(modName) || seen.contains(modName)) {
            continue;
        }
        seen.insert(modName);
        result.append(modName);
    }
    return result;
}

std::optional<InstanceProfilePreset::Preset> parsePresetFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return std::nullopt;
    }
    const QJsonObject obj = doc.object();
    const QFileInfo fi(filePath);

    InstanceProfilePreset::Preset preset;
    preset.id = obj.value(QStringLiteral("id")).toString();
    if (preset.id.isEmpty()) {
        preset.id = fi.completeBaseName();
    }
    if (!isValidPresetId(preset.id)) {
        return std::nullopt;
    }

    preset.name = obj.value(QStringLiteral("name")).toString();
    if (preset.name.isEmpty()) {
        preset.name = preset.id;
    }

    const QDateTime parsed = QDateTime::fromString(obj.value(QStringLiteral("updatedAtUtc")).toString(), Qt::ISODateWithMs);
    preset.updatedAt = parsed.isValid() ? parsed.toLocalTime() : fi.lastModified();
    preset.enabledMods = readModList(obj.value(QStringLiteral("enabledMods")).toArray());
    preset.knownMods = readModList(obj.value(QStringLiteral("knownMods")).toArray());

    if (obj.contains(QStringLiteral("shaderPack")) && obj.value(QStringLiteral("shaderPack")).isString()) {
        preset.shaderPack = obj.value(QStringLiteral("shaderPack")).toString();
    }
    return preset;
}

std::optional<QString> readShaderPackSetting(const QString& gameRoot)
{
    if (gameRoot.isEmpty()) {
        return std::nullopt;
    }
    const QString path = FS::PathCombine(gameRoot, QStringLiteral("optionsshaders.txt"));
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QString content = QString::fromUtf8(file.readAll());
    const QStringList lines = content.split(QLatin1Char('\n'));
    const QString prefix = QStringLiteral("shaderPack=");
    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        if (line.startsWith(prefix)) {
            return line.mid(prefix.size());
        }
    }
    return std::nullopt;
}

bool writeShaderPackSetting(const QString& gameRoot, const QString& shaderPackValue)
{
    if (gameRoot.isEmpty() || !FS::ensureFolderPathExists(gameRoot)) {
        return false;
    }
    const QString path = FS::PathCombine(gameRoot, QStringLiteral("optionsshaders.txt"));
    const QString prefix = QStringLiteral("shaderPack=");
    const QString targetLine = prefix + shaderPackValue;

    QString newline = QStringLiteral("\n");
    QStringList lines;
    bool hadTrailingNewline = true;

    if (QFileInfo::exists(path)) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QByteArray raw = file.readAll();
        if (file.error() != QFile::NoError) {
            return false;
        }
        const QString content = QString::fromUtf8(raw);
        if (content.contains(QStringLiteral("\r\n"))) {
            newline = QStringLiteral("\r\n");
        }
        if (!content.isEmpty()) {
            QString normalized = content;
            normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
            lines = normalized.split(QLatin1Char('\n'));
            hadTrailingNewline = !lines.isEmpty() && lines.last().isEmpty();
            if (hadTrailingNewline) {
                lines.removeLast();
            }
            for (QString& line : lines) {
                if (line.endsWith(QLatin1Char('\r'))) {
                    line.chop(1);
                }
            }
        }
    }

    bool replaced = false;
    for (QString& line : lines) {
        if (line.startsWith(prefix)) {
            line = targetLine;
            replaced = true;
        }
    }
    if (!replaced) {
        lines.append(targetLine);
    }

    QString output = lines.join(newline);
    if (hadTrailingNewline || !output.isEmpty()) {
        output.append(newline);
    }
    return writeBytesAtomic(path, output.toUtf8());
}

struct RenameOp {
    QString srcPath;
    QString dstPath;
};

void rollbackRenames(const QList<RenameOp>& completed)
{
    for (qsizetype i = completed.size() - 1; i >= 0; --i) {
        const RenameOp& op = completed.at(i);
        QFile::rename(op.dstPath, op.srcPath);
    }
}

}  // namespace

namespace InstanceProfilePreset {

QString presetsRoot(const BaseInstance* instance)
{
    if (!instance || instance->instanceRoot().isEmpty()) {
        return {};
    }
    return FS::PathCombine(instance->instanceRoot(), QStringLiteral("halo-presets"));
}

QList<Preset> listPresets(const BaseInstance* instance)
{
    QList<Preset> result;
    const QString root = presetsRoot(instance);
    if (root.isEmpty()) {
        return result;
    }
    const QDir dir(root);
    if (!dir.exists()) {
        return result;
    }

    const QFileInfoList entries =
        dir.entryInfoList(QStringList{ QStringLiteral("*.json") }, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo& fi : entries) {
        const QString fileName = fi.fileName();
        if (fileName.startsWith(QLatin1Char('.')) || fileName.compare(QStringLiteral("active.json"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        if (const auto preset = parsePresetFile(fi.absoluteFilePath())) {
            result.append(*preset);
        }
    }

    std::sort(result.begin(), result.end(), [](const Preset& a, const Preset& b) {
        const int cmp = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        if (cmp != 0) {
            return cmp < 0;
        }
        return a.id < b.id;
    });
    return result;
}

QString activePresetId(const BaseInstance* instance)
{
    const QString path = activeFilePath(instance);
    if (path.isEmpty()) {
        return {};
    }
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return {};
    }
    const QString id = doc.object().value(QStringLiteral("activePresetId")).toString();
    return isValidPresetId(id) ? id : QString();
}

void setActivePresetId(BaseInstance* instance, const QString& id)
{
    const QString path = activeFilePath(instance);
    if (path.isEmpty()) {
        return;
    }
    if (id.isEmpty()) {
        if (QFileInfo::exists(path)) {
            QFile::remove(path);
        }
        return;
    }
    if (!isValidPresetId(id) || !FS::ensureFolderPathExists(presetsRoot(instance))) {
        return;
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("activePresetId"), id);
    writeBytesAtomic(path, QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

std::optional<Preset> saveCurrentAsPreset(BaseInstance* instance,
                                          const QString& displayName,
                                          bool captureShaderSetting,
                                          QString* errorOut)
{
    if (!instance || instance->instanceRoot().isEmpty()) {
        setError(errorOut, QStringLiteral("Instance is null or has no root directory."));
        return std::nullopt;
    }
    if (QFileInfo::exists(FS::PathCombine(instance->instanceRoot(), QStringLiteral("halo-bisect.json")))) {
        setError(errorOut, QStringLiteral("Cannot save a preset while a mod bisect session is active."));
        return std::nullopt;
    }
    const QString trimmedName = displayName.trimmed();
    if (trimmedName.isEmpty()) {
        setError(errorOut, QStringLiteral("Preset name cannot be empty."));
        return std::nullopt;
    }

    const QString root = presetsRoot(instance);
    if (!FS::ensureFolderPathExists(root)) {
        setError(errorOut, QStringLiteral("Failed to create presets directory."));
        return std::nullopt;
    }

    const QString id = slugifyDisplayName(trimmedName);
    const QString destPath = presetFilePath(instance, id);
    if (destPath.isEmpty()) {
        setError(errorOut, QStringLiteral("Generated preset identifier is invalid."));
        return std::nullopt;
    }

    QStringList enabledMods;
    QStringList knownMods;
    QSet<QString> enabledSeen;
    QSet<QString> knownSeen;

    const QString modsPath = instance->modsRoot();
    if (!modsPath.isEmpty() && QDir(modsPath).exists()) {
        const QFileInfoList entries = QDir(modsPath).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& fi : entries) {
            const QString fileName = fi.fileName();
            if (fileName.endsWith(QStringLiteral(".jar.disabled"), Qt::CaseInsensitive)) {
                const QString normalized = fileName.chopped(QStringLiteral(".disabled").size());
                if (!knownSeen.contains(normalized)) {
                    knownSeen.insert(normalized);
                    knownMods.append(normalized);
                }
            } else if (fileName.endsWith(QStringLiteral(".jar"), Qt::CaseInsensitive)) {
                if (!knownSeen.contains(fileName)) {
                    knownSeen.insert(fileName);
                    knownMods.append(fileName);
                }
                if (!enabledSeen.contains(fileName)) {
                    enabledSeen.insert(fileName);
                    enabledMods.append(fileName);
                }
            }
        }
    }

    std::optional<QString> shaderPack;
    if (captureShaderSetting) {
        shaderPack = readShaderPackSetting(instance->gameRoot());
    }

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();

    QJsonObject obj;
    obj.insert(QStringLiteral("version"), 1);
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("name"), trimmedName);
    obj.insert(QStringLiteral("updatedAtUtc"), nowUtc.toString(Qt::ISODateWithMs));
    obj.insert(QStringLiteral("enabledMods"), QJsonArray::fromStringList(enabledMods));
    obj.insert(QStringLiteral("knownMods"), QJsonArray::fromStringList(knownMods));
    if (shaderPack.has_value()) {
        obj.insert(QStringLiteral("shaderPack"), *shaderPack);
    }

    if (!writeBytesAtomic(destPath, QJsonDocument(obj).toJson(QJsonDocument::Indented))) {
        setError(errorOut, QStringLiteral("Failed to write preset file."));
        return std::nullopt;
    }

    setActivePresetId(instance, id);

    Preset preset;
    preset.id = id;
    preset.name = trimmedName;
    preset.updatedAt = nowUtc.toLocalTime();
    preset.enabledMods = std::move(enabledMods);
    preset.knownMods = std::move(knownMods);
    preset.shaderPack = std::move(shaderPack);
    return preset;
}

bool applyPreset(BaseInstance* instance, const QString& presetId, QString* errorOut)
{
    if (!instance || instance->instanceRoot().isEmpty()) {
        setError(errorOut, QStringLiteral("Instance is null or has no root directory."));
        return false;
    }
    if (instance->isRunning()) {
        setError(errorOut, QStringLiteral("Cannot apply preset while the instance is running."));
        return false;
    }
    const QString bisectLockPath = FS::PathCombine(instance->instanceRoot(), QStringLiteral("halo-bisect.json"));
    if (QFileInfo::exists(bisectLockPath)) {
        setError(errorOut, QStringLiteral("Cannot apply preset while mod bisect is active."));
        return false;
    }

    const QString path = presetFilePath(instance, presetId);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        setError(errorOut, QStringLiteral("Preset does not exist."));
        return false;
    }
    const auto presetOpt = parsePresetFile(path);
    if (!presetOpt.has_value()) {
        setError(errorOut, QStringLiteral("Preset file is invalid or unreadable."));
        return false;
    }
    const Preset& preset = *presetOpt;

    QSet<QString> enabledSet;
    for (const QString& mod : preset.enabledMods) {
        enabledSet.insert(mod);
    }
    QSet<QString> knownSet = enabledSet;
    for (const QString& mod : preset.knownMods) {
        knownSet.insert(mod);
    }

    QList<RenameOp> plannedRenames;
    const QString modsPath = instance->modsRoot();
    if (!modsPath.isEmpty() && QDir(modsPath).exists()) {
        for (const QString& mod : preset.enabledMods) {
            const QString enabledPath = FS::PathCombine(modsPath, mod);
            const QString disabledPath = FS::PathCombine(modsPath, mod + QStringLiteral(".disabled"));
            if (!QFileInfo::exists(enabledPath) && QFileInfo::exists(disabledPath)) {
                plannedRenames.append({ disabledPath, enabledPath });
            }
        }
        for (const QString& mod : knownSet) {
            if (enabledSet.contains(mod)) {
                continue;
            }
            const QString enabledPath = FS::PathCombine(modsPath, mod);
            const QString disabledPath = FS::PathCombine(modsPath, mod + QStringLiteral(".disabled"));
            if (QFileInfo::exists(enabledPath)) {
                plannedRenames.append({ enabledPath, disabledPath });
            }
        }
    }

    for (const RenameOp& op : plannedRenames) {
        if (QFileInfo::exists(op.dstPath)) {
            setError(errorOut, QStringLiteral("Target file already exists: %1").arg(op.dstPath));
            return false;
        }
    }

    QList<RenameOp> completedRenames;
    completedRenames.reserve(plannedRenames.size());
    for (const RenameOp& op : plannedRenames) {
        if (QFileInfo::exists(op.dstPath) || !QFile::rename(op.srcPath, op.dstPath)) {
            rollbackRenames(completedRenames);
            setError(errorOut, QStringLiteral("Failed to rename mod file: %1").arg(op.srcPath));
            return false;
        }
        completedRenames.append(op);
    }

    if (preset.shaderPack.has_value() && !writeShaderPackSetting(instance->gameRoot(), *preset.shaderPack)) {
        rollbackRenames(completedRenames);
        setError(errorOut, QStringLiteral("Failed to update optionsshaders.txt."));
        return false;
    }

    setActivePresetId(instance, preset.id);
    return true;
}

bool deletePreset(BaseInstance* instance, const QString& presetId)
{
    const QString path = presetFilePath(instance, presetId);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return false;
    }
    if (!QFile::remove(path)) {
        return false;
    }
    if (activePresetId(instance) == presetId) {
        setActivePresetId(instance, QString());
    }
    return true;
}

}  // namespace InstanceProfilePreset
