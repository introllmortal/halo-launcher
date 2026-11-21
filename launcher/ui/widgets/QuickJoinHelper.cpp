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
#include "QuickJoinHelper.h"

#include <io/stream_reader.h>
#include <tag_compound.h>
#include <tag_list.h>
#include <tag_string.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <memory>
#include <sstream>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/World.h"

namespace QuickJoinHelper {

namespace {

QString pinConfigPath(const BaseInstance* instance)
{
    return (instance && !instance->instanceRoot().isEmpty())
               ? FS::PathCombine(instance->instanceRoot(), QStringLiteral("halo-quickjoin.json"))
               : QString();
}

QList<Target> readSavedServers(const BaseInstance* instance)
{
    QList<Target> result;
    if (!instance || instance->gameRoot().isEmpty()) {
        return result;
    }
    const QString serversPath = FS::PathCombine(instance->gameRoot(), QStringLiteral("servers.dat"));
    if (!QFileInfo::exists(serversPath)) {
        return result;
    }
    try {
        const QByteArray bytes = FS::read(serversPath);
        std::istringstream stream(std::string(bytes.constData(), static_cast<size_t>(bytes.size())));
        auto pair = nbt::io::read_compound(stream);
        if (pair.first != "" || !pair.second || !pair.second->has_key("servers", nbt::tag_type::List)) {
            return result;
        }
        auto& list = pair.second->at("servers").as<nbt::tag_list>();
        for (const auto& elem : list) {
            if (elem.get_type() != nbt::tag_type::Compound) {
                continue;
            }
            const auto& compound = elem.as<nbt::tag_compound>();
            if (!compound.has_key("ip", nbt::tag_type::String)) {
                continue;
            }
            const std::string ip = compound.at("ip").as<nbt::tag_string>().get();
            const std::string name =
                compound.has_key("name", nbt::tag_type::String) ? compound.at("name").as<nbt::tag_string>().get() : ip;
            const QString address = QString::fromUtf8(ip.c_str()).trimmed();
            if (address.isEmpty()) {
                continue;
            }
            QString label = QString::fromUtf8(name.c_str()).trimmed();
            if (label.isEmpty()) {
                label = address;
            }
            result.append({ Target::Kind::Server, label, address });
        }
    } catch (...) {
        return {};
    }
    return result;
}

QList<Target> readWorlds(const BaseInstance* instance)
{
    QList<Target> result;
    auto* mcInst = dynamic_cast<const MinecraftInstance*>(instance);
    if (!mcInst || mcInst->worldDir().isEmpty() || !QDir(mcInst->worldDir()).exists()) {
        return result;
    }

    QList<World> worlds;
    for (const QFileInfo& entry : QDir(mcInst->worldDir()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        World w(entry);
        if (w.isValid() && !w.folderName().isEmpty()) {
            worlds.append(w);
        }
    }
    std::sort(worlds.begin(), worlds.end(), [](const World& a, const World& b) { return a.lastPlayed() > b.lastPlayed(); });
    for (const World& w : worlds) {
        const QString title = w.name().isEmpty() ? w.folderName() : w.name();
        result.append({ Target::Kind::World, title, w.folderName() });
    }
    return result;
}

}  // namespace

MinecraftTarget::Ptr Target::toMinecraftTarget() const
{
    return std::make_shared<MinecraftTarget>(MinecraftTarget::parse(targetValue, kind == Kind::World));
}

QList<Target> discoverTargets(const BaseInstance* instance)
{
    QList<Target> targets = readSavedServers(instance);
    if (instance && instance->traits().contains(QStringLiteral("feature:is_quick_play_singleplayer"))) {
        targets.append(readWorlds(instance));
    }
    return targets;
}

std::optional<Target> resolveActiveTarget(const BaseInstance* instance, const QList<Target>& available)
{
    if (!instance) {
        return std::nullopt;
    }
    const QString cfgPath = pinConfigPath(instance);
    if (!cfgPath.isEmpty() && QFileInfo::exists(cfgPath)) {
        QFile f(cfgPath);
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject()) {
                const QJsonObject obj = doc.object();
                const QString kindStr = obj.value(QStringLiteral("kind")).toString();
                const QString val = obj.value(QStringLiteral("value")).toString();
                const QString lbl = obj.value(QStringLiteral("label")).toString();
                const Target::Kind kind = (kindStr == QStringLiteral("world")) ? Target::Kind::World : Target::Kind::Server;
                if (!val.isEmpty()) {
                    for (const auto& t : available) {
                        if (t.kind == kind && t.targetValue == val) {
                            return t;
                        }
                    }
                    if (kind == Target::Kind::Server) {
                        return Target{ kind, lbl.isEmpty() ? val : lbl, val };
                    }
                }
            }
        }
    }
    // Prefer first saved server or latest played world if any exist
    if (!available.isEmpty()) {
        return available.first();
    }
    return std::nullopt;
}

std::optional<Target> resolveActiveTarget(const BaseInstance* instance)
{
    return instance ? resolveActiveTarget(instance, discoverTargets(instance)) : std::nullopt;
}

void savePinnedTarget(BaseInstance* instance, const std::optional<Target>& target)
{
    const QString cfgPath = pinConfigPath(instance);
    if (cfgPath.isEmpty()) {
        return;
    }
    if (!target.has_value()) {
        QFile::remove(cfgPath);
        return;
    }
    QJsonObject obj;
    obj.insert(QStringLiteral("kind"), target->kind == Target::Kind::World ? QStringLiteral("world") : QStringLiteral("server"));
    obj.insert(QStringLiteral("value"), target->targetValue);
    obj.insert(QStringLiteral("label"), target->label);
    QSaveFile out(cfgPath);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        out.commit();
    }
}

}  // namespace QuickJoinHelper
