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

#include <QDialog>

#include "diagnostics/LaunchPreflightCheck.h"

class LaunchPreflightDialog final : public QDialog {
    Q_OBJECT

   public:
    enum class ResultAction {
        Cancel,
        LaunchAnyway,
        FixAndLaunch,
    };

    explicit LaunchPreflightDialog(const PreflightReport& report, QWidget* parent = nullptr);

    ResultAction resultAction() const { return m_action; }

   private:
    ResultAction m_action = ResultAction::Cancel;
};
