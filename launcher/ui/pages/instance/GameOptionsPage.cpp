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
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "minecraft/MinecraftInstance.h"

GameOptionsPage::GameOptionsPage(MinecraftInstance* inst, QWidget* parent) : QWidget(parent), m_instance(inst)
{
    buildUi();
    if (m_instance) {
        connect(m_instance, &BaseInstance::runningStatusChanged, this, [this](bool) { updateRunningState(); });
    }
    reloadAll();
}

void GameOptionsPage::openedImpl()
{
    reloadAll();
}

void GameOptionsPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(10);

    auto* topBar = new QHBoxLayout();
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_reloadBtn = new QPushButton(tr("Reload"), this);
    m_saveBtn = new QPushButton(tr("Save Changes"), this);
    topBar->addWidget(m_statusLabel, 1);
    topBar->addWidget(m_reloadBtn);
    topBar->addWidget(m_saveBtn);
    root->addLayout(topBar);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildBasicTab(), tr("Basic"));
    m_sodiumTab = buildSodiumTab();
    m_tabs->addTab(m_sodiumTab, tr("Sodium / Embeddium"));
    m_tabs->addTab(buildModConfigsTab(), tr("Graphics Mods"));
    m_tabs->addTab(buildRawOptionsTab(), tr("All Options (options.txt)"));
    root->addWidget(m_tabs, 1);

    connect(m_reloadBtn, &QPushButton::clicked, this, &GameOptionsPage::reloadAll);
    connect(m_saveBtn, &QPushButton::clicked, this, &GameOptionsPage::saveAll);
}

QWidget* GameOptionsPage::buildRawOptionsTab()
{
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);

    m_rawFilterEdit = new QLineEdit(container);
    m_rawFilterEdit->setPlaceholderText(tr("Search options.txt key or value..."));
    layout->addWidget(m_rawFilterEdit);

    m_rawTable = new QTableWidget(0, 2, container);
    m_rawTable->setHorizontalHeaderLabels({ tr("Key"), tr("Value") });
    m_rawTable->horizontalHeader()->setStretchLastSection(true);
    m_rawTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_rawTable->verticalHeader()->setVisible(false);
    layout->addWidget(m_rawTable, 1);

    connect(m_rawFilterEdit, &QLineEdit::textChanged, this, &GameOptionsPage::filterRawOptionsTable);
    return container;
}

void GameOptionsPage::reloadAll()
{
    if (!m_instance) {
        return;
    }

    m_loadError.clear();
    QString err;
    const QString optPath = GameOptionsFile::optionsFilePath(m_instance);
    if (!m_options.loadFromFile(optPath, &err)) {
        m_loadError = err;
    }

    m_modConfigs = GraphicsModConfigManager::discoverConfigs(m_instance->gameRoot());
    m_sodiumConfigIndex = -1;
    for (int i = 0; i < m_modConfigs.size(); ++i) {
        if (m_modConfigs.at(i).isSodiumFamily) {
            m_sodiumConfigIndex = i;
            break;
        }
    }

    populateBasicControls();
    populateSodiumControls();

    m_activeModConfigIndex = -1;
    m_activeModEntries.clear();
    m_modConfigCombo->blockSignals(true);
    m_modConfigCombo->clear();
    for (const auto& cfg : m_modConfigs) {
        m_modConfigCombo->addItem(cfg.displayName);
    }
    m_modConfigCombo->blockSignals(false);
    if (!m_modConfigs.isEmpty()) {
        onModConfigSelected(0);
    } else {
        m_modTable->setRowCount(0);
    }

    populateRawOptionsTable();
    updateRunningState();
}

void GameOptionsPage::populateRawOptionsTable()
{
    const auto entries = m_options.entries();
    int count = 0;
    for (const auto& e : entries) {
        if (!e.isRawLine) {
            ++count;
        }
    }
    m_rawTable->setRowCount(count);
    int row = 0;
    for (const auto& e : entries) {
        if (e.isRawLine) {
            continue;
        }
        auto* keyItem = new QTableWidgetItem(e.key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        m_rawTable->setItem(row, 0, keyItem);
        m_rawTable->setItem(row, 1, new QTableWidgetItem(e.rawValue));
        ++row;
    }
    filterRawOptionsTable(m_rawFilterEdit->text());
}

void GameOptionsPage::collectRawOptionsTable()
{
    for (int row = 0; row < m_rawTable->rowCount(); ++row) {
        const QString key = m_rawTable->item(row, 0)->text();
        const QString val = m_rawTable->item(row, 1)->text();
        m_options.setRawValue(key, val);
    }
}

void GameOptionsPage::filterRawOptionsTable(const QString& text)
{
    const QString needle = text.trimmed();
    for (int row = 0; row < m_rawTable->rowCount(); ++row) {
        const bool match = needle.isEmpty() || m_rawTable->item(row, 0)->text().contains(needle, Qt::CaseInsensitive) ||
                           m_rawTable->item(row, 1)->text().contains(needle, Qt::CaseInsensitive);
        m_rawTable->setRowHidden(row, !match);
    }
}

void GameOptionsPage::updateRunningState()
{
    const bool running = m_instance && m_instance->isRunning();
    const bool canEdit = !running && m_loadError.isEmpty();
    m_saveBtn->setEnabled(canEdit);
    m_tabs->setEnabled(canEdit);
    if (running) {
        m_statusLabel->setText(tr("Minecraft is running. Close the game to edit game options."));
    } else if (!m_loadError.isEmpty()) {
        m_statusLabel->setText(tr("Failed to load options.txt: %1").arg(m_loadError));
    } else {
        m_statusLabel->setText(tr("Edit game options, Sodium/Embeddium settings, and graphics mod configs."));
    }
}

void GameOptionsPage::saveAll()
{
    if (!m_instance || m_instance->isRunning() || !m_loadError.isEmpty()) {
        updateRunningState();
        return;
    }

    // Apply raw table first if user edited it, then overlay Basic tab controls if Basic tab is active.
    if (m_tabs->currentIndex() == 3) {
        collectRawOptionsTable();
        populateBasicControls();
    } else {
        collectBasicControls();
    }

    QString err;
    if (!m_options.saveForInstance(m_instance, GameOptionsFile::optionsFilePath(m_instance), &err)) {
        QMessageBox::warning(this, tr("Game Options"), err);
        return;
    }

    const bool modTabActive = (m_tabs->currentIndex() == 2);
    const bool hasModCfg = (m_activeModConfigIndex >= 0 && m_activeModConfigIndex < m_modConfigs.size());
    const bool sameAsSodium = hasModCfg && (m_activeModConfigIndex == m_sodiumConfigIndex);

    if (m_sodiumConfigIndex >= 0 && !(modTabActive && sameAsSodium)) {
        collectSodiumControls();
        const auto& sodiumCfg = m_modConfigs.at(m_sodiumConfigIndex);
        if (!GraphicsModConfigManager::saveEntries(sodiumCfg.filePath, sodiumCfg.format, m_sodiumEntries, &err)) {
            QMessageBox::warning(this, tr("Game Options"), err);
            return;
        }
    }

    if (hasModCfg && (modTabActive || !sameAsSodium)) {
        collectModConfigTable();
        const auto& modCfg = m_modConfigs.at(m_activeModConfigIndex);
        if (!GraphicsModConfigManager::saveEntries(modCfg.filePath, modCfg.format, m_activeModEntries, &err)) {
            QMessageBox::warning(this, tr("Game Options"), err);
            return;
        }
    }

    reloadAll();
    m_statusLabel->setText(tr("Saved successfully."));
}
