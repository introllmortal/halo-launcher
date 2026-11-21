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
#include "GameOptionsPage.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
QVariant findEntryValue(const QList<GraphicsModConfigManager::Entry>& entries, const QString& keyPath, const QVariant& fallback)
{
    for (const auto& e : entries) {
        if (e.keyPath == keyPath) {
            return e.value;
        }
    }
    return fallback;
}

void setEntryValue(QList<GraphicsModConfigManager::Entry>& entries, const QString& keyPath, const QVariant& value)
{
    for (auto& e : entries) {
        if (e.keyPath == keyPath) {
            e.value = value;
            return;
        }
    }
    entries.append({ keyPath, value });
}
}  // namespace

QWidget* GameOptionsPage::buildSodiumTab()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* container = new QWidget(scroll);
    auto* layout = new QVBoxLayout(container);

    m_sodiumTitleLabel = new QLabel(container);
    m_sodiumTitleLabel->setWordWrap(true);
    layout->addWidget(m_sodiumTitleLabel);

    auto* form = new QFormLayout();
    m_leavesCombo = new QComboBox(container);
    m_leavesCombo->addItems({ QStringLiteral("DEFAULT"), QStringLiteral("FAST"), QStringLiteral("FANCY") });
    m_weatherCombo = new QComboBox(container);
    m_weatherCombo->addItems({ QStringLiteral("DEFAULT"), QStringLiteral("FAST"), QStringLiteral("FANCY") });
    m_vignetteCheck = new QCheckBox(tr("Enable Vignette"), container);

    m_chunkThreadsSpin = new QSpinBox(container);
    m_chunkThreadsSpin->setRange(0, 32);
    m_deferModeCombo = new QComboBox(container);
    m_deferModeCombo->addItems({ QStringLiteral("ALWAYS"), QStringLiteral("ONE_FRAME"), QStringLiteral("ZERO_FRAMES") });
    m_entityCullCheck = new QCheckBox(tr("Use Entity Culling"), container);
    m_fogOcclusionCheck = new QCheckBox(tr("Use Fog Occlusion"), container);
    m_blockCullCheck = new QCheckBox(tr("Use Block Face Culling"), container);
    m_animVisibleCheck = new QCheckBox(tr("Animate Only Visible Textures"), container);
    m_renderAheadSpin = new QSpinBox(container);
    m_renderAheadSpin->setRange(0, 9);

    form->addRow(tr("Leaves Quality:"), m_leavesCombo);
    form->addRow(tr("Weather Quality:"), m_weatherCombo);
    form->addRow(QString(), m_vignetteCheck);
    form->addRow(tr("Chunk Builder Threads (0 = Auto):"), m_chunkThreadsSpin);
    form->addRow(tr("Chunk Build Defer Mode:"), m_deferModeCombo);
    form->addRow(QString(), m_entityCullCheck);
    form->addRow(QString(), m_fogOcclusionCheck);
    form->addRow(QString(), m_blockCullCheck);
    form->addRow(QString(), m_animVisibleCheck);
    form->addRow(tr("CPU Render Ahead Limit:"), m_renderAheadSpin);
    layout->addLayout(form);
    layout->addStretch(1);

    scroll->setWidget(container);
    return scroll;
}

QWidget* GameOptionsPage::buildModConfigsTab()
{
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);

    auto* top = new QHBoxLayout();
    top->addWidget(new QLabel(tr("Config File:"), container));
    m_modConfigCombo = new QComboBox(container);
    m_modFilterEdit = new QLineEdit(container);
    m_modFilterEdit->setPlaceholderText(tr("Filter keys..."));
    top->addWidget(m_modConfigCombo, 1);
    top->addWidget(m_modFilterEdit, 1);
    layout->addLayout(top);

    m_modTable = new QTableWidget(0, 2, container);
    m_modTable->setHorizontalHeaderLabels({ tr("Setting"), tr("Value") });
    m_modTable->horizontalHeader()->setStretchLastSection(true);
    m_modTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_modTable->verticalHeader()->setVisible(false);
    layout->addWidget(m_modTable, 1);

    connect(m_modConfigCombo, &QComboBox::currentIndexChanged, this, &GameOptionsPage::onModConfigSelected);
    connect(m_modFilterEdit, &QLineEdit::textChanged, this, &GameOptionsPage::filterModConfigTable);
    return container;
}

void GameOptionsPage::populateSodiumControls()
{
    const bool hasSodium = m_sodiumConfigIndex >= 0;
    m_sodiumTab->setEnabled(hasSodium);
    if (!hasSodium) {
        m_sodiumEntries.clear();
        m_sodiumTitleLabel->setText(tr("No Sodium / Embeddium / Rubidium config found in config/."));
        return;
    }

    const auto& cfg = m_modConfigs.at(m_sodiumConfigIndex);
    m_sodiumTitleLabel->setText(tr("Editing %1 (%2)").arg(cfg.displayName, cfg.filePath));
    m_sodiumEntries = GraphicsModConfigManager::loadEntries(cfg.filePath, cfg.format);

    m_leavesCombo->setCurrentText(findEntryValue(m_sodiumEntries, QStringLiteral("quality.leaves_quality"), QStringLiteral("DEFAULT")).toString());
    m_weatherCombo->setCurrentText(findEntryValue(m_sodiumEntries, QStringLiteral("quality.weather_quality"), QStringLiteral("DEFAULT")).toString());
    m_vignetteCheck->setChecked(findEntryValue(m_sodiumEntries, QStringLiteral("quality.enable_vignette"), true).toBool());
    m_chunkThreadsSpin->setValue(findEntryValue(m_sodiumEntries, QStringLiteral("performance.chunk_builder_threads"), 0).toInt());
    m_deferModeCombo->setCurrentText(findEntryValue(m_sodiumEntries, QStringLiteral("performance.chunk_build_defer_mode"), QStringLiteral("ALWAYS")).toString());
    m_entityCullCheck->setChecked(findEntryValue(m_sodiumEntries, QStringLiteral("performance.use_entity_culling"), true).toBool());
    m_fogOcclusionCheck->setChecked(findEntryValue(m_sodiumEntries, QStringLiteral("performance.use_fog_occlusion"), true).toBool());
    m_blockCullCheck->setChecked(findEntryValue(m_sodiumEntries, QStringLiteral("performance.use_block_face_culling"), true).toBool());
    m_animVisibleCheck->setChecked(findEntryValue(m_sodiumEntries, QStringLiteral("performance.animate_only_visible_textures"), true).toBool());
    m_renderAheadSpin->setValue(findEntryValue(m_sodiumEntries, QStringLiteral("advanced.cpu_render_ahead_limit"), 3).toInt());
}

void GameOptionsPage::collectSodiumControls()
{
    if (m_sodiumConfigIndex < 0) {
        return;
    }
    setEntryValue(m_sodiumEntries, QStringLiteral("quality.leaves_quality"), m_leavesCombo->currentText());
    setEntryValue(m_sodiumEntries, QStringLiteral("quality.weather_quality"), m_weatherCombo->currentText());
    setEntryValue(m_sodiumEntries, QStringLiteral("quality.enable_vignette"), m_vignetteCheck->isChecked());
    setEntryValue(m_sodiumEntries, QStringLiteral("performance.chunk_builder_threads"), m_chunkThreadsSpin->value());
    setEntryValue(m_sodiumEntries, QStringLiteral("performance.chunk_build_defer_mode"), m_deferModeCombo->currentText());
    setEntryValue(m_sodiumEntries, QStringLiteral("performance.use_entity_culling"), m_entityCullCheck->isChecked());
    setEntryValue(m_sodiumEntries, QStringLiteral("performance.use_fog_occlusion"), m_fogOcclusionCheck->isChecked());
    setEntryValue(m_sodiumEntries, QStringLiteral("performance.use_block_face_culling"), m_blockCullCheck->isChecked());
    setEntryValue(m_sodiumEntries, QStringLiteral("performance.animate_only_visible_textures"), m_animVisibleCheck->isChecked());
    setEntryValue(m_sodiumEntries, QStringLiteral("advanced.cpu_render_ahead_limit"), m_renderAheadSpin->value());
}

void GameOptionsPage::onModConfigSelected(int index)
{
    if (m_activeModConfigIndex >= 0 && m_activeModConfigIndex < m_modConfigs.size()) {
        collectModConfigTable();
    }
    m_activeModConfigIndex = index;
    if (index < 0 || index >= m_modConfigs.size()) {
        m_activeModEntries.clear();
        m_modTable->setRowCount(0);
        return;
    }
    const auto& cfg = m_modConfigs.at(index);
    m_activeModEntries = GraphicsModConfigManager::loadEntries(cfg.filePath, cfg.format);
    populateModConfigTable();
}

void GameOptionsPage::populateModConfigTable()
{
    m_modTable->setRowCount(m_activeModEntries.size());
    for (int i = 0; i < m_activeModEntries.size(); ++i) {
        const auto& e = m_activeModEntries.at(i);
        auto* keyItem = new QTableWidgetItem(e.keyPath);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_modTable->setItem(i, 0, keyItem);
        m_modTable->setItem(i, 1, new QTableWidgetItem(e.value.toString()));
    }
    filterModConfigTable(m_modFilterEdit->text());
}

void GameOptionsPage::collectModConfigTable()
{
    for (int i = 0; i < m_activeModEntries.size() && i < m_modTable->rowCount(); ++i) {
        const QString text = m_modTable->item(i, 1)->text().trimmed();
        const int typeId = m_activeModEntries.at(i).value.typeId();
        if (typeId == QMetaType::Bool) {
            m_activeModEntries[i].value = (text.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0 || text == QStringLiteral("1"));
        } else if (typeId == QMetaType::Int || typeId == QMetaType::LongLong) {
            bool ok = false;
            const int v = text.toInt(&ok);
            if (ok) {
                m_activeModEntries[i].value = v;
            }
        } else if (typeId == QMetaType::Double) {
            bool ok = false;
            const double v = text.toDouble(&ok);
            if (ok) {
                m_activeModEntries[i].value = v;
            }
        } else {
            m_activeModEntries[i].value = text;
        }
    }
}

void GameOptionsPage::filterModConfigTable(const QString& text)
{
    const QString needle = text.trimmed();
    for (int row = 0; row < m_modTable->rowCount(); ++row) {
        const bool match = needle.isEmpty() || m_modTable->item(row, 0)->text().contains(needle, Qt::CaseInsensitive);
        m_modTable->setRowHidden(row, !match);
    }
}
