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
#include "ModDiffDialog.h"

#include <QApplication>
#include <QBrush>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include "BaseInstance.h"
#include "diagnostics/InstanceStorageCleaner.h"

namespace {

enum KindFilter {
    FilterAll = -1,
    FilterAdded = static_cast<int>(PackSnapshot::DiffKind::Added),
    FilterRemoved = static_cast<int>(PackSnapshot::DiffKind::Removed),
    FilterEnabled = static_cast<int>(PackSnapshot::DiffKind::Enabled),
    FilterDisabled = static_cast<int>(PackSnapshot::DiffKind::Disabled),
};

QString kindLabel(PackSnapshot::DiffKind kind)
{
    switch (kind) {
        case PackSnapshot::DiffKind::Added:
            return ModDiffDialog::tr("+ Added");
        case PackSnapshot::DiffKind::Removed:
            return ModDiffDialog::tr("- Removed");
        case PackSnapshot::DiffKind::Enabled:
            return ModDiffDialog::tr("Enabled");
        case PackSnapshot::DiffKind::Disabled:
            return ModDiffDialog::tr("Disabled");
    }
    return {};
}

QColor kindColor(PackSnapshot::DiffKind kind)
{
    switch (kind) {
        case PackSnapshot::DiffKind::Added:
            return QColor(QStringLiteral("#3fb950"));
        case PackSnapshot::DiffKind::Removed:
            return QColor(QStringLiteral("#f85149"));
        case PackSnapshot::DiffKind::Enabled:
            return QColor(QStringLiteral("#58a6ff"));
        case PackSnapshot::DiffKind::Disabled:
            return QColor(QStringLiteral("#d29922"));
    }
    return {};
}

QString formatFileWithSize(const QString& name, qint64 bytes)
{
    if (name.isEmpty()) {
        return QStringLiteral("-");
    }
    return bytes > 0 ? QStringLiteral("%1 (%2)").arg(name, InstanceStorageCleaner::formatBytes(bytes)) : name;
}

}  // namespace

ModDiffDialog::ModDiffDialog(const PackSnapshot::SnapshotDiff& diff, QWidget* parent) : QDialog(parent)
{
    setupUi();
    setDiff(diff);
}

ModDiffDialog* ModDiffDialog::forSnapshotComparison(BaseInstance* instance,
                                                    const QList<PackSnapshot::Info>& snapshots,
                                                    QWidget* parent)
{
    const PackSnapshot::SnapshotDiff initial =
        snapshots.isEmpty() ? PackSnapshot::SnapshotDiff{} : PackSnapshot::diffSnapshotToCurrent(instance, snapshots.first());
    auto* dlg = new ModDiffDialog(initial, parent);
    dlg->m_leftInstance = instance;
    dlg->m_snapshots = snapshots;
    dlg->m_instanceCompareMode = false;

    if (!snapshots.isEmpty()) {
        dlg->m_selectorLabel->setText(tr("Snapshot:"));
        dlg->m_selectorLabel->show();
        dlg->m_selectorCombo->clear();
        for (const auto& snap : snapshots) {
            const QString when =
                snap.createdAt.isValid() ? QLocale().toString(snap.createdAt.toLocalTime(), QLocale::ShortFormat) : snap.id;
            const QString ver = snap.packVersion.isEmpty() ? QString() : QStringLiteral(" [%1]").arg(snap.packVersion);
            dlg->m_selectorCombo->addItem(QStringLiteral("%1%2 (%3 mods)").arg(when, ver).arg(snap.modCount));
        }
        dlg->m_selectorCombo->show();
        connect(dlg->m_selectorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), dlg, [dlg](int idx) {
            if (!dlg->m_leftInstance || idx < 0 || idx >= dlg->m_snapshots.size()) {
                return;
            }
            dlg->setDiff(PackSnapshot::diffSnapshotToCurrent(dlg->m_leftInstance, dlg->m_snapshots.at(idx)));
        });
    }
    return dlg;
}

ModDiffDialog* ModDiffDialog::forInstanceComparison(BaseInstance* leftInstance,
                                                    const QList<BaseInstance*>& candidates,
                                                    QWidget* parent)
{
    QList<BaseInstance*> filtered;
    for (BaseInstance* inst : candidates) {
        if (inst && inst != leftInstance) {
            filtered.append(inst);
        }
    }
    const PackSnapshot::SnapshotDiff initial =
        filtered.isEmpty() ? PackSnapshot::SnapshotDiff{} : PackSnapshot::diffInstances(leftInstance, filtered.first());
    auto* dlg = new ModDiffDialog(initial, parent);
    dlg->m_leftInstance = leftInstance;
    dlg->m_instanceCompareMode = true;

    dlg->m_selectorLabel->setText(tr("Compare against:"));
    dlg->m_selectorLabel->show();
    dlg->m_selectorCombo->clear();
    for (BaseInstance* inst : filtered) {
        dlg->m_instanceCandidates.append(QPointer<BaseInstance>(inst));
        dlg->m_selectorCombo->addItem(inst->name());
    }
    dlg->m_selectorCombo->show();
    connect(dlg->m_selectorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), dlg, [dlg](int idx) {
        if (!dlg->m_leftInstance || idx < 0 || idx >= dlg->m_instanceCandidates.size()) {
            return;
        }
        BaseInstance* right = dlg->m_instanceCandidates.at(idx);
        if (right) {
            dlg->setDiff(PackSnapshot::diffInstances(dlg->m_leftInstance, right));
        }
    });
    return dlg;
}

void ModDiffDialog::setupUi()
{
    setWindowTitle(tr("Mod Changes Diff"));
    resize(780, 520);
    setMinimumSize(560, 380);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setWordWrap(true);
    layout->addWidget(m_summaryLabel);

    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);

    m_selectorLabel = new QLabel(this);
    m_selectorLabel->hide();
    filterRow->addWidget(m_selectorLabel);

    m_selectorCombo = new QComboBox(this);
    m_selectorCombo->setMinimumWidth(180);
    m_selectorCombo->hide();
    filterRow->addWidget(m_selectorCombo);

    m_kindFilterCombo = new QComboBox(this);
    m_kindFilterCombo->addItem(tr("All changes"), FilterAll);
    m_kindFilterCombo->addItem(tr("Added"), FilterAdded);
    m_kindFilterCombo->addItem(tr("Removed"), FilterRemoved);
    m_kindFilterCombo->addItem(tr("Enabled"), FilterEnabled);
    m_kindFilterCombo->addItem(tr("Disabled"), FilterDisabled);
    connect(m_kindFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ModDiffDialog::applyFilters);
    filterRow->addWidget(m_kindFilterCombo);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter by mod file name..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ModDiffDialog::applyFilters);
    filterRow->addWidget(m_searchEdit, 1);

    layout->addLayout(filterRow);

    m_tree = new QTreeWidget(this);
    m_tree->setRootIsDecorated(false);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({ tr("Status"), tr("Mod"), tr("Before"), tr("After") });
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    layout->addWidget(m_tree, 1);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(8);

    m_copyButton = new QPushButton(tr("Copy Markdown"), this);
    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        if (QClipboard* cb = QApplication::clipboard()) {
            cb->setText(m_diff.toMarkdown());
            m_copyButton->setText(tr("Copied!"));
            QTimer::singleShot(1500, this, [this]() { m_copyButton->setText(tr("Copy Markdown")); });
        }
    });
    bottomRow->addWidget(m_copyButton);
    bottomRow->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    bottomRow->addWidget(buttons);
    layout->addLayout(bottomRow);
}

void ModDiffDialog::setDiff(const PackSnapshot::SnapshotDiff& diff)
{
    m_diff = diff;
    refreshSummaryAndRows();
    applyFilters();
}

void ModDiffDialog::refreshSummaryAndRows()
{
    const QString left = m_diff.leftLabel.isEmpty() ? tr("Before") : m_diff.leftLabel;
    const QString right = m_diff.rightLabel.isEmpty() ? tr("After") : m_diff.rightLabel;
    m_titleLabel->setText(QStringLiteral("%1 -> %2").arg(left, right));

    QString summary = tr("+%1 added, -%2 removed, ~%3 toggled (%4 -> %5 mods)")
                          .arg(m_diff.addedCount())
                          .arg(m_diff.removedCount())
                          .arg(m_diff.toggledCount())
                          .arg(m_diff.totalLeftMods)
                          .arg(m_diff.totalRightMods);
    if (!m_diff.leftPackVersion.isEmpty() || !m_diff.rightPackVersion.isEmpty()) {
        const QString lv = m_diff.leftPackVersion.isEmpty() ? QStringLiteral("-") : m_diff.leftPackVersion;
        const QString rv = m_diff.rightPackVersion.isEmpty() ? QStringLiteral("-") : m_diff.rightPackVersion;
        summary = tr("Pack version: %1 -> %2  |  %3").arg(lv, rv, summary);
    }
    m_summaryLabel->setText(summary);

    m_tree->clear();
    m_tree->setSortingEnabled(false);
    for (const auto& entry : m_diff.modChanges) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, kindLabel(entry.kind));
        item->setForeground(0, QBrush(kindColor(entry.kind)));
        item->setData(0, Qt::UserRole, static_cast<int>(entry.kind));
        item->setText(1, entry.baseKey);
        item->setText(2, formatFileWithSize(entry.leftFileName, entry.leftSize));
        item->setText(3, formatFileWithSize(entry.rightFileName, entry.rightSize));
    }
    m_tree->setSortingEnabled(true);
}

void ModDiffDialog::applyFilters()
{
    const int filterKind = m_kindFilterCombo->currentData().toInt();
    const QString query = m_searchEdit->text().trimmed();

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        const int rowKind = item->data(0, Qt::UserRole).toInt();
        const bool kindMatches = (filterKind == FilterAll) || (rowKind == filterKind);
        const bool textMatches = query.isEmpty() || item->text(1).contains(query, Qt::CaseInsensitive)
                                 || item->text(2).contains(query, Qt::CaseInsensitive)
                                 || item->text(3).contains(query, Qt::CaseInsensitive);
        item->setHidden(!kindMatches || !textMatches);
    }
}
