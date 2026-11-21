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
#include <QFutureWatcher>
#include <QList>
#include <QPointer>

#include "diagnostics/InstanceStorageCleaner.h"

class BaseInstance;
class QCheckBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

class StorageCleanerDialog : public QDialog {
    Q_OBJECT

   public:
    explicit StorageCleanerDialog(BaseInstance* instance, QWidget* parent = nullptr);
    ~StorageCleanerDialog() override;

   private:
    void startScan();
    void onScanFinished();
    void cleanSelected();
    void updateTotalLabel();

    QPointer<BaseInstance> m_instance;
    QLabel* m_headerLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QVBoxLayout* m_listLayout = nullptr;
    QPushButton* m_cleanButton = nullptr;
    QPushButton* m_refreshButton = nullptr;

    QFutureWatcher<InstanceStorageCleaner::ScanReport> m_watcher;
    InstanceStorageCleaner::ScanReport m_report;
    QList<QCheckBox*> m_checkBoxes;
};
