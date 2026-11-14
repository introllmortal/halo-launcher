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

#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <QVector>

class BaseInstance;
class QWidget;

struct HaloPackVersion {
    QString version;
    QString name;
    QString changelog;
    QUrl downloadUrl;
    QJsonObject deltaManifest;
    QUrl deltaManifestUrl;
    bool hasDeltaManifest = false;
};

struct ManagedPackUpdateCandidate {
    QString sourceLabel;
    QString versionId;
    QString versionName;
    QString displayName;
    QString changelogHtml;
    QUrl downloadUrl;
    bool isDelta = false;
    bool isHaloPack = false;
    HaloPackVersion haloVersion;
};

namespace ManagedPackUpdater {

/** Returns true if the instance is a managed modpack from a supported update provider (excludes FTB/ATLauncher). */
bool supportsOneStepUpdate(const BaseInstance* inst);

/** Parses a Halo / GitHub releases JSON manifest into normalized pack versions (newest first). */
QVector<HaloPackVersion> parseHaloPackVersions(const QByteArray& jsonData);

/** Fetches the latest release candidate for any managed pack provider (Halo, Modrinth, CurseForge). */
bool fetchLatestCandidate(BaseInstance* inst, QWidget* parent, ManagedPackUpdateCandidate& outCandidate, bool& outIsUpToDate);

/** Shows a scrollable changelog confirmation dialog for the pending modpack update. */
bool askConfirmUpdateDialog(BaseInstance* inst, const ManagedPackUpdateCandidate& candidate, QWidget* parent);

}  // namespace ManagedPackUpdater
