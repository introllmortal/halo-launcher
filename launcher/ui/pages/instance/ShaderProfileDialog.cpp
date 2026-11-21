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
#include "ShaderProfileDialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

ShaderProfileDialog::ShaderProfileDialog(QString shaderPacksDir, QString packFileName, bool readOnly, QWidget* parent)
    : QDialog(parent), m_shaderPacksDir(std::move(shaderPacksDir)), m_packFileName(std::move(packFileName)), m_readOnly(readOnly)
{
    setWindowTitle(tr("Shader Settings: %1").arg(m_packFileName));
    resize(560, 440);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* info = new QLabel(
        m_readOnly
            ? tr("Minecraft is running. Close the game to save shader option changes.")
            : tr("Editing overrides in %1.txt (values already changed from shader defaults).").arg(m_packFileName),
        this);
    info->setWordWrap(true);
    layout->addWidget(info);

    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Search shader option..."));
    layout->addWidget(m_filterEdit);

    m_table = new QTableWidget(0, 2, this);
    m_table->setHorizontalHeaderLabels({ tr("Option"), tr("Value") });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table, 1);

    auto* bottom = new QHBoxLayout();
    m_addBtn = new QPushButton(tr("Add Option"), this);
    m_resetBtn = new QPushButton(tr("Reset to Defaults"), this);
    m_addBtn->setEnabled(!m_readOnly);
    m_resetBtn->setEnabled(!m_readOnly);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    m_saveBtn = box->button(QDialogButtonBox::Save);
    m_saveBtn->setEnabled(!m_readOnly);

    bottom->addWidget(m_addBtn);
    bottom->addWidget(m_resetBtn);
    bottom->addStretch(1);
    bottom->addWidget(box);
    layout->addLayout(bottom);

    connect(m_filterEdit, &QLineEdit::textChanged, this, &ShaderProfileDialog::applyFilter);
    connect(m_addBtn, &QPushButton::clicked, this, &ShaderProfileDialog::addOption);
    connect(m_resetBtn, &QPushButton::clicked, this, &ShaderProfileDialog::resetToDefaults);
    connect(box, &QDialogButtonBox::accepted, this, &ShaderProfileDialog::saveAndClose);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_entries = ShaderConfigManager::loadShaderProfile(m_shaderPacksDir, m_packFileName);
    populateTable();
}

void ShaderProfileDialog::populateTable()
{
    m_table->setRowCount(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) {
        auto* keyItem = new QTableWidgetItem(m_entries.at(i).key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        auto* valItem = new QTableWidgetItem(m_entries.at(i).value);
        if (m_readOnly) {
            valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
        }
        m_table->setItem(i, 0, keyItem);
        m_table->setItem(i, 1, valItem);
    }
    applyFilter(m_filterEdit->text());
}

void ShaderProfileDialog::applyFilter(const QString& text)
{
    const QString needle = text.trimmed();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const QString key = m_table->item(row, 0)->text();
        const QString val = m_table->item(row, 1)->text();
        const bool match = needle.isEmpty() || key.contains(needle, Qt::CaseInsensitive) || val.contains(needle, Qt::CaseInsensitive);
        m_table->setRowHidden(row, !match);
    }
}

void ShaderProfileDialog::addOption()
{
    bool ok = false;
    const QString key = QInputDialog::getText(this, tr("Add Shader Option"), tr("Option key (e.g. SHADOW_QUALITY):"),
                                              QLineEdit::Normal, QString(), &ok)
                            .trimmed();
    if (!ok || key.isEmpty()) {
        return;
    }
    const QString val = QInputDialog::getText(this, tr("Add Shader Option"), tr("Value for %1:").arg(key),
                                              QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        return;
    }
    for (int i = 0; i < m_table->rowCount(); ++i) {
        if (m_table->item(i, 0)->text() == key) {
            m_table->item(i, 1)->setText(val);
            return;
        }
    }
    m_entries.append({ key, val });
    populateTable();
}

void ShaderProfileDialog::resetToDefaults()
{
    const auto answer = QMessageBox::question(
        this, tr("Reset Shader Settings"),
        tr("Remove all saved overrides for %1 and restore shader defaults?").arg(m_packFileName));
    if (answer != QMessageBox::Yes) {
        return;
    }
    QString err;
    if (!ShaderConfigManager::resetShaderProfile(m_shaderPacksDir, m_packFileName, &err)) {
        QMessageBox::warning(this, tr("Shader Settings"), err);
        return;
    }
    m_entries.clear();
    populateTable();
}

void ShaderProfileDialog::saveAndClose()
{
    QList<ShaderConfigManager::ProfileEntry> updated;
    updated.reserve(m_table->rowCount());
    for (int row = 0; row < m_table->rowCount(); ++row) {
        updated.append({ m_table->item(row, 0)->text(), m_table->item(row, 1)->text() });
    }
    QString err;
    if (!ShaderConfigManager::saveShaderProfile(m_shaderPacksDir, m_packFileName, updated, &err)) {
        QMessageBox::warning(this, tr("Shader Settings"), err);
        return;
    }
    accept();
}
