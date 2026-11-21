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
#include <QList>

#include "minecraft/mod/ShaderConfigManager.h"

class QLineEdit;
class QPushButton;
class QTableWidget;

/**
 * Edits per-shader overrides stored in shaderpacks/<packName>.txt.
 */
class ShaderProfileDialog : public QDialog {
    Q_OBJECT

   public:
    explicit ShaderProfileDialog(QString shaderPacksDir, QString packFileName, bool readOnly, QWidget* parent = nullptr);

   private slots:
    void applyFilter(const QString& text);
    void addOption();
    void resetToDefaults();
    void saveAndClose();

   private:
    void populateTable();

    QString m_shaderPacksDir;
    QString m_packFileName;
    bool m_readOnly = false;
    QList<ShaderConfigManager::ProfileEntry> m_entries;

    QLineEdit* m_filterEdit = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_addBtn = nullptr;
    QPushButton* m_resetBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
};
