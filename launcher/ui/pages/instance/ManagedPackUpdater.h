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

#include "ManagedPackManifest.h"

struct ZipDeltaPlan;

namespace ManagedPackUpdater {

/** Executes a full archive/URL update via InstanceImportTask for a managed instance. */
bool runImportUpdate(BaseInstance* inst,
                     const QUrl& url,
                     const QString& versionID,
                     const QString& versionName,
                     QWidget* parent,
                     bool showResultDialog = true);

/** Executes a delta sync update for a Halo/GitHub pack version, offering full fallback on failure. */
bool executeHaloDeltaSync(BaseInstance* inst, const HaloPackVersion& version, QWidget* parent);

/** Executes a Zip Range delta sync update from a precomputed plan, offering full fallback on failure. */
bool executeZipDeltaSync(BaseInstance* inst,
                         const ZipDeltaPlan& plan,
                         const QUrl& fallbackUrl,
                         const QString& versionName,
                         QWidget* parent,
                         bool showResultDialog = true);

/** Attempts a Zip Range delta sync update for a .zip URL, falling back to full import if Range is unavailable. */
bool runSmartZipUpdate(BaseInstance* inst,
                       const QUrl& url,
                       const QString& versionID,
                       const QString& versionName,
                       QWidget* parent,
                       bool showResultDialog = true);

/** Checks for the latest release, previews its changelog, and updates the pack in one step. */
bool runOneStepUpdate(BaseInstance* inst, QWidget* parent);

}  // namespace ManagedPackUpdater
