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

class QAction;
class QGridLayout;
class QWidget;
class BaseInstance;
class ModFolderModel;
class WideBar;

namespace ModFolderHaloTools {

/// Attaches the Mod Bisect banner/action and Mod Profiles preset menu onto ModFolderPage.
void attach(QWidget* page,
            BaseInstance* instance,
            ModFolderModel* model,
            QGridLayout* gridLayout,
            WideBar* toolbar,
            QAction* insertAfterAction);

}  // namespace ModFolderHaloTools
