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
#include "UpdaterArtifacts.h"

#include <QDebug>
#include <QRegularExpression>

#include "StringUtils.h"

namespace HaloUpdater {

QList<GitHubReleaseAsset> validReleaseArtifacts(const GitHubRelease& release, const ArtifactTarget& target)
{
    QList<GitHubReleaseAsset> valid;

    qDebug() << "Selecting best asset from" << release.tag_name << "for platform" << target.buildArtifact
             << "portable:" << target.isPortable;
    if (target.buildArtifact.isEmpty()) {
        qWarning() << "Build platform is not set!";
        return valid;
    }

    const auto [platform, platform_qt_ver] = StringUtils::splitFirst(target.buildArtifact.toLower(), QStringLiteral("-qt"));
    const bool system_is_arm = target.cpuArchitecture.toLower().contains(QStringLiteral("arm64"));
    static const QRegularExpression s_qtPattern(QStringLiteral("-qt(\\d+)"));

    for (const auto& asset : release.assets) {
        if (asset.name.endsWith(QStringLiteral(".zsync"), Qt::CaseInsensitive)) {
            qDebug() << "Rejecting zsync file" << asset.name;
            continue;
        }
        const QString asset_name = asset.name.toLower();
        if (!target.isAppimage && asset_name.endsWith(QStringLiteral("appimage"))) {
            qDebug() << "Rejecting" << asset.name << "because it is an AppImage";
            continue;
        }
        if (target.isAppimage && !asset_name.endsWith(QStringLiteral("appimage"))) {
            qDebug() << "Rejecting" << asset.name << "because it is not an AppImage";
            continue;
        }

        const bool asset_is_arm = asset_name.contains(QStringLiteral("arm64"));
        const bool asset_is_archive = asset_name.endsWith(QStringLiteral(".zip")) || asset_name.endsWith(QStringLiteral(".tar.gz"));

        bool for_platform = !platform.isEmpty() && asset_name.contains(platform);
        if (!for_platform) {
            qDebug() << "Rejecting" << asset.name << "because platforms do not match";
        }
        const bool for_portable = asset_name.contains(QStringLiteral("portable"));
        if (for_platform && asset_name.contains(QStringLiteral("legacy")) && !platform.contains(QStringLiteral("legacy"))) {
            qDebug() << "Rejecting" << asset.name << "because platforms do not match";
            for_platform = false;
        }
        if (for_platform && (asset_is_arm != system_is_arm)) {
            qDebug() << "Rejecting" << asset.name << "because architecture does not match";
            for_platform = false;
        }
        if (for_platform && platform.contains(QStringLiteral("windows")) && !target.isPortable && asset_is_archive) {
            qDebug() << "Rejecting" << asset.name << "because it is not an installer";
            for_platform = false;
        }

        const auto qt_match = s_qtPattern.match(asset_name);
        if (for_platform && qt_match.hasMatch()) {
            if (platform_qt_ver.isEmpty() || platform_qt_ver.toInt() != qt_match.captured(1).toInt()) {
                qDebug() << "Rejecting" << asset.name << "because it is not for the correct qt version" << platform_qt_ver.toInt()
                         << "vs" << qt_match.captured(1).toInt();
                for_platform = false;
            }
        }

        if (((target.isPortable && for_portable) || (!target.isPortable && !for_portable)) && for_platform) {
            qDebug() << "Accepting" << asset.name;
            valid.append(asset);
        }
    }
    return valid;
}

}  // namespace HaloUpdater
