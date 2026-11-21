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

#include <QList>
#include <QWidget>

#include "minecraft/GameOptionsFile.h"
#include "minecraft/mod/GraphicsModConfigManager.h"
#include "ui/pages/BasePage.h"

class MinecraftInstance;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTabWidget;

namespace GameGraphicsAdvisor {
struct PresetValues;
}

/**
 * Instance page for editing Minecraft options.txt, Sodium/Embeddium/Rubidium settings,
 * and discovered graphics/performance mod configs without launching the game.
 */
class GameOptionsPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit GameOptionsPage(MinecraftInstance* inst, QWidget* parent = nullptr);
    ~GameOptionsPage() override = default;

    void openedImpl() override;

    QString displayName() const override { return tr("Game Options"); }
    QIcon icon() const override { return QIcon::fromTheme("settings"); }
    QString id() const override { return QStringLiteral("gameoptions"); }
    QString helpPage() const override { return QStringLiteral("Game-Options-management"); }
    void retranslate() override {}

   private slots:
    void reloadAll();
    void saveAll();
    void applyPreset(int presetIndex);
    void applyAutoHardwarePreset();
    void onModConfigSelected(int index);
    void filterModConfigTable(const QString& text);
    void filterRawOptionsTable(const QString& text);

   private:
    void buildUi();
    QWidget* buildBasicTab();
    QWidget* buildSodiumTab();
    QWidget* buildModConfigsTab();
    QWidget* buildRawOptionsTab();

    void populateBasicControls();
    void collectBasicControls();
    void applyPresetValues(const GameGraphicsAdvisor::PresetValues& values);
    void populateSodiumControls();
    void collectSodiumControls();
    void populateModConfigTable();
    void collectModConfigTable();
    void populateRawOptionsTable();
    void collectRawOptionsTable();
    void updateRunningState();

    MinecraftInstance* m_instance = nullptr;
    GameOptionsFile m_options;
    QString m_loadError;
    QList<GraphicsModConfigManager::ConfigFileInfo> m_modConfigs;
    int m_sodiumConfigIndex = -1;
    QList<GraphicsModConfigManager::Entry> m_sodiumEntries;
    int m_activeModConfigIndex = -1;
    QList<GraphicsModConfigManager::Entry> m_activeModEntries;

    QLabel* m_statusLabel = nullptr;
    QPushButton* m_reloadBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QTabWidget* m_tabs = nullptr;

    // Basic options.txt controls
    QButtonGroup* m_presetGroup = nullptr;
    QLabel* m_hardwareSummaryLabel = nullptr;
    QSpinBox* m_renderDistSpin = nullptr;
    QSpinBox* m_simDistSpin = nullptr;
    QSpinBox* m_entityDistSpin = nullptr;
    QSpinBox* m_maxFpsSpin = nullptr;
    QSpinBox* m_fovSpin = nullptr;
    QSpinBox* m_gammaSpin = nullptr;
    QComboBox* m_guiScaleCombo = nullptr;
    QComboBox* m_graphicsCombo = nullptr;
    QComboBox* m_cloudsCombo = nullptr;
    QComboBox* m_particlesCombo = nullptr;
    QComboBox* m_biomeBlendCombo = nullptr;
    QSpinBox* m_mipmapSpin = nullptr;
    QCheckBox* m_smoothLightingCheck = nullptr;
    QCheckBox* m_entityShadowsCheck = nullptr;
    QCheckBox* m_viewBobbingCheck = nullptr;
    QCheckBox* m_fullscreenCheck = nullptr;
    QCheckBox* m_vsyncCheck = nullptr;
    QSpinBox* m_masterVolSpin = nullptr;
    QSpinBox* m_musicVolSpin = nullptr;
    QSpinBox* m_weatherVolSpin = nullptr;
    QSpinBox* m_blocksVolSpin = nullptr;
    QSpinBox* m_hostileVolSpin = nullptr;
    QSpinBox* m_playerVolSpin = nullptr;

    // Sodium / Embeddium / Rubidium controls
    QWidget* m_sodiumTab = nullptr;
    QLabel* m_sodiumTitleLabel = nullptr;
    QComboBox* m_leavesCombo = nullptr;
    QComboBox* m_weatherCombo = nullptr;
    QCheckBox* m_vignetteCheck = nullptr;
    QSpinBox* m_chunkThreadsSpin = nullptr;
    QComboBox* m_deferModeCombo = nullptr;
    QCheckBox* m_entityCullCheck = nullptr;
    QCheckBox* m_fogOcclusionCheck = nullptr;
    QCheckBox* m_blockCullCheck = nullptr;
    QCheckBox* m_animVisibleCheck = nullptr;
    QSpinBox* m_renderAheadSpin = nullptr;

    // Generic graphics mod configs tab
    QComboBox* m_modConfigCombo = nullptr;
    QLineEdit* m_modFilterEdit = nullptr;
    QTableWidget* m_modTable = nullptr;

    // Raw options.txt tab
    QLineEdit* m_rawFilterEdit = nullptr;
    QTableWidget* m_rawTable = nullptr;
};
