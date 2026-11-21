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
#include <QPointer>

#include "ui/pages/instance/PackSnapshot.h"

class BaseInstance;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

class ModDiffDialog : public QDialog {
    Q_OBJECT

   public:
    explicit ModDiffDialog(const PackSnapshot::SnapshotDiff& diff, QWidget* parent = nullptr);

    static ModDiffDialog* forSnapshotComparison(BaseInstance* instance,
                                                const QList<PackSnapshot::Info>& snapshots,
                                                QWidget* parent = nullptr);
    static ModDiffDialog* forInstanceComparison(BaseInstance* leftInstance,
                                                const QList<BaseInstance*>& candidates,
                                                QWidget* parent = nullptr);

    void setDiff(const PackSnapshot::SnapshotDiff& diff);

   private:
    void setupUi();
    void refreshSummaryAndRows();
    void applyFilters();

    PackSnapshot::SnapshotDiff m_diff;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_selectorLabel = nullptr;
    QComboBox* m_selectorCombo = nullptr;
    QComboBox* m_kindFilterCombo = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QTreeWidget* m_tree = nullptr;
    QPushButton* m_copyButton = nullptr;

    QPointer<BaseInstance> m_leftInstance;
    QList<PackSnapshot::Info> m_snapshots;
    QList<QPointer<BaseInstance>> m_instanceCandidates;
    bool m_instanceCompareMode = false;
};
