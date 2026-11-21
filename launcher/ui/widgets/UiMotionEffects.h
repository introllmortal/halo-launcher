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

class QAbstractButton;
class QAbstractItemView;

namespace UiAnimations {

/// Installs an animated sliding accent pill on the left edge of the active row in a list/sidebar view.
void installListActiveIndicator(QAbstractItemView* view);

/// Installs smooth cubic-eased mouse wheel scrolling on an item view when animations are enabled.
void installSmoothScroll(QAbstractItemView* view);

/// Enables or disables a subtle diagonal shimmer sweep on a button (e.g. Play button while launching/running).
void setButtonBusyShimmer(QAbstractButton* button, bool active);

}  // namespace UiAnimations
