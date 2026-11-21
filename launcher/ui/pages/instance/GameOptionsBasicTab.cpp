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

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>

#include "minecraft/GameGraphicsAdvisor.h"

namespace {

int fovOptionToDegrees(double val)
{
    return qBound(30, static_cast<int>(std::lround(val * 40.0 + 70.0)), 110);
}

double degreesToFovOption(int deg)
{
    return (static_cast<double>(qBound(30, deg, 110)) - 70.0) / 40.0;
}

int soundToPercent(double val)
{
    return qBound(0, static_cast<int>(std::lround(val * 100.0)), 100);
}

double percentToSound(int pct)
{
    return static_cast<double>(qBound(0, pct, 100)) / 100.0;
}

int gammaToPercent(double val)
{
    return qBound(0, static_cast<int>(std::lround(val * 100.0)), 100);
}

double percentToGamma(int pct)
{
    return static_cast<double>(qBound(0, pct, 100)) / 100.0;
}

int entityDistToPercent(double val)
{
    return qBound(50, static_cast<int>(std::lround(val * 100.0)), 500);
}

double percentToEntityDist(int pct)
{
    return static_cast<double>(qBound(50, pct, 500)) / 100.0;
}

int cloudsValueToIndex(const QString& raw)
{
    const QString norm = raw.trimmed().toLower();
    if (norm == QLatin1String("false") || norm == QLatin1String("off")) {
        return 0;
    }
    if (norm == QLatin1String("fast")) {
        return 1;
    }
    return 2;
}

QString cloudsIndexToValue(int idx)
{
    if (idx == 0) {
        return QStringLiteral("false");
    }
    if (idx == 1) {
        return QStringLiteral("fast");
    }
    return QStringLiteral("true");
}

bool parseSmoothLighting(const QString& raw, bool fallback)
{
    const QString norm = raw.trimmed().toLower();
    if (norm.isEmpty()) {
        return fallback;
    }
    if (norm == QLatin1String("false") || norm == QLatin1String("0")) {
        return false;
    }
    return true;
}

class MaxFpsSpinBox : public QSpinBox {
   public:
    explicit MaxFpsSpinBox(QString unlimitedText, QWidget* parent = nullptr)
        : QSpinBox(parent), m_unlimitedText(std::move(unlimitedText))
    {
        setRange(10, 260);
        setSingleStep(10);
    }

   protected:
    QString textFromValue(int val) const override { return val >= 260 ? m_unlimitedText : QSpinBox::textFromValue(val); }

    int valueFromText(const QString& text) const override
    {
        if (text.compare(m_unlimitedText, Qt::CaseInsensitive) == 0) {
            return 260;
        }
        return QSpinBox::valueFromText(text);
    }

    QValidator::State validate(QString& input, int& pos) const override
    {
        if (input.compare(m_unlimitedText, Qt::CaseInsensitive) == 0) {
            return QValidator::Acceptable;
        }
        return QSpinBox::validate(input, pos);
    }

   private:
    QString m_unlimitedText;
};

}  // namespace

QWidget* GameOptionsPage::buildBasicTab()
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* container = new QWidget(scroll);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(12);

    auto* presetGroup = new QGroupBox(tr("Graphics Presets & Auto-Tune"), container);
    auto* presetLayout = new QVBoxLayout(presetGroup);
    presetLayout->setSpacing(8);

    auto* presetRow = new QHBoxLayout();
    presetRow->setSpacing(6);
    m_presetGroup = new QButtonGroup(presetGroup);
    m_presetGroup->setExclusive(true);

    const struct {
        int id;
        QString label;
    } kPresets[] = {
        { static_cast<int>(GameGraphicsAdvisor::QualityPreset::MaxFps), tr("Max FPS") },
        { static_cast<int>(GameGraphicsAdvisor::QualityPreset::Low), tr("Low") },
        { static_cast<int>(GameGraphicsAdvisor::QualityPreset::Medium), tr("Medium") },
        { static_cast<int>(GameGraphicsAdvisor::QualityPreset::High), tr("High") },
        { static_cast<int>(GameGraphicsAdvisor::QualityPreset::Ultra), tr("Ultra") },
    };

    for (const auto& item : kPresets) {
        auto* btn = new QPushButton(item.label, presetGroup);
        btn->setCheckable(true);
        m_presetGroup->addButton(btn, item.id);
        presetRow->addWidget(btn);
        const int presetId = item.id;
        connect(btn, &QPushButton::clicked, this, [this, presetId]() { applyPreset(presetId); });
    }

    auto* autoBtn = new QPushButton(tr("Auto-Tune to PC"), presetGroup);
    autoBtn->setDefault(true);
    presetRow->addStretch(1);
    presetRow->addWidget(autoBtn);
    connect(autoBtn, &QPushButton::clicked, this, &GameOptionsPage::applyAutoHardwarePreset);
    presetLayout->addLayout(presetRow);

    m_hardwareSummaryLabel = new QLabel(presetGroup);
    m_hardwareSummaryLabel->setWordWrap(true);
    const auto hw = GameGraphicsAdvisor::detectSystemHardware();
    m_hardwareSummaryLabel->setText(tr("Detected PC: %1").arg(GameGraphicsAdvisor::formatHardwareSummary(hw)));
    presetLayout->addWidget(m_hardwareSummaryLabel);
    layout->addWidget(presetGroup);

    auto* displayGroup = new QGroupBox(tr("Display & Performance"), container);
    auto* displayForm = new QFormLayout(displayGroup);
    m_renderDistSpin = new QSpinBox(displayGroup);
    m_renderDistSpin->setRange(2, 32);
    m_renderDistSpin->setSuffix(tr(" chunks"));

    m_simDistSpin = new QSpinBox(displayGroup);
    m_simDistSpin->setRange(5, 32);
    m_simDistSpin->setSuffix(tr(" chunks"));

    m_entityDistSpin = new QSpinBox(displayGroup);
    m_entityDistSpin->setRange(50, 500);
    m_entityDistSpin->setSingleStep(25);
    m_entityDistSpin->setSuffix(QStringLiteral("%"));

    m_maxFpsSpin = new MaxFpsSpinBox(tr("Unlimited (260)"), displayGroup);

    m_fovSpin = new QSpinBox(displayGroup);
    m_fovSpin->setRange(30, 110);

    m_gammaSpin = new QSpinBox(displayGroup);
    m_gammaSpin->setRange(0, 100);
    m_gammaSpin->setSingleStep(5);
    m_gammaSpin->setSuffix(QStringLiteral("%"));

    m_guiScaleCombo = new QComboBox(displayGroup);
    m_guiScaleCombo->addItems({ tr("Auto (0)"), tr("Small (1)"), tr("Normal (2)"), tr("Large (3)"), tr("Huge (4)") });

    m_fullscreenCheck = new QCheckBox(tr("Fullscreen"), displayGroup);
    m_vsyncCheck = new QCheckBox(tr("Use VSync"), displayGroup);

    displayForm->addRow(tr("Render Distance:"), m_renderDistSpin);
    displayForm->addRow(tr("Simulation Distance:"), m_simDistSpin);
    displayForm->addRow(tr("Entity Distance:"), m_entityDistSpin);
    displayForm->addRow(tr("Max Framerate (260 = Unlimited):"), m_maxFpsSpin);
    displayForm->addRow(tr("FOV (degrees):"), m_fovSpin);
    displayForm->addRow(tr("Brightness:"), m_gammaSpin);
    displayForm->addRow(tr("GUI Scale:"), m_guiScaleCombo);
    displayForm->addRow(QString(), m_fullscreenCheck);
    displayForm->addRow(QString(), m_vsyncCheck);
    layout->addWidget(displayGroup);

    auto* qualityGroup = new QGroupBox(tr("Visual Quality & Details"), container);
    auto* qualityForm = new QFormLayout(qualityGroup);

    m_graphicsCombo = new QComboBox(qualityGroup);
    m_graphicsCombo->addItems({ tr("Fast (0)"), tr("Fancy (1)"), tr("Fabulous! (2)") });

    m_cloudsCombo = new QComboBox(qualityGroup);
    m_cloudsCombo->addItems({ tr("Off"), tr("Fast"), tr("Fancy") });

    m_particlesCombo = new QComboBox(qualityGroup);
    m_particlesCombo->addItems({ tr("All (0)"), tr("Decreased (1)"), tr("Minimal (2)") });

    m_biomeBlendCombo = new QComboBox(qualityGroup);
    m_biomeBlendCombo->addItems({ tr("Off (0)"), tr("3x3 (1)"), tr("5x5 (2)"), tr("7x7 (3)"), tr("9x9 (4)"),
                                  tr("11x11 (5)"), tr("13x13 (6)"), tr("15x15 (7)") });

    m_mipmapSpin = new QSpinBox(qualityGroup);
    m_mipmapSpin->setRange(0, 4);

    m_smoothLightingCheck = new QCheckBox(tr("Smooth Lighting"), qualityGroup);
    m_entityShadowsCheck = new QCheckBox(tr("Entity Shadows"), qualityGroup);
    m_viewBobbingCheck = new QCheckBox(tr("View Bobbing"), qualityGroup);

    qualityForm->addRow(tr("Graphics Quality:"), m_graphicsCombo);
    qualityForm->addRow(tr("Clouds:"), m_cloudsCombo);
    qualityForm->addRow(tr("Particles:"), m_particlesCombo);
    qualityForm->addRow(tr("Biome Blend:"), m_biomeBlendCombo);
    qualityForm->addRow(tr("Mipmap Levels:"), m_mipmapSpin);
    qualityForm->addRow(QString(), m_smoothLightingCheck);
    qualityForm->addRow(QString(), m_entityShadowsCheck);
    qualityForm->addRow(QString(), m_viewBobbingCheck);
    layout->addWidget(qualityGroup);

    auto* audioGroup = new QGroupBox(tr("Audio (%)"), container);
    auto* audioForm = new QFormLayout(audioGroup);
    auto makeVolSpin = [&](const QString& label) {
        auto* spin = new QSpinBox(audioGroup);
        spin->setRange(0, 100);
        spin->setSuffix(QStringLiteral("%"));
        audioForm->addRow(label, spin);
        return spin;
    };
    m_masterVolSpin = makeVolSpin(tr("Master Volume:"));
    m_musicVolSpin = makeVolSpin(tr("Music:"));
    m_weatherVolSpin = makeVolSpin(tr("Weather:"));
    m_blocksVolSpin = makeVolSpin(tr("Blocks:"));
    m_hostileVolSpin = makeVolSpin(tr("Hostile Creatures:"));
    m_playerVolSpin = makeVolSpin(tr("Players:"));
    layout->addWidget(audioGroup);
    layout->addStretch(1);

    scroll->setWidget(container);
    return scroll;
}

void GameOptionsPage::populateBasicControls()
{
    m_renderDistSpin->setValue(m_options.intValue(QStringLiteral("renderDistance")).value_or(12));
    m_simDistSpin->setValue(m_options.intValue(QStringLiteral("simulationDistance")).value_or(8));
    m_entityDistSpin->setValue(entityDistToPercent(m_options.doubleValue(QStringLiteral("entityDistanceScaling")).value_or(1.0)));
    m_maxFpsSpin->setValue(m_options.intValue(QStringLiteral("maxFps")).value_or(120));
    m_fovSpin->setValue(fovOptionToDegrees(m_options.doubleValue(QStringLiteral("fov")).value_or(0.0)));
    m_gammaSpin->setValue(gammaToPercent(m_options.doubleValue(QStringLiteral("gamma")).value_or(0.5)));
    m_guiScaleCombo->setCurrentIndex(qBound(0, m_options.intValue(QStringLiteral("guiScale")).value_or(0), 4));
    m_graphicsCombo->setCurrentIndex(qBound(0, m_options.intValue(QStringLiteral("graphicsMode")).value_or(1), 2));
    m_cloudsCombo->setCurrentIndex(cloudsValueToIndex(m_options.rawValue(QStringLiteral("renderClouds"), QStringLiteral("true"))));
    m_particlesCombo->setCurrentIndex(qBound(0, m_options.intValue(QStringLiteral("particles")).value_or(0), 2));
    m_biomeBlendCombo->setCurrentIndex(qBound(0, m_options.intValue(QStringLiteral("biomeBlendRadius")).value_or(2), 7));
    m_mipmapSpin->setValue(qBound(0, m_options.intValue(QStringLiteral("mipmapLevels")).value_or(4), 4));
    m_smoothLightingCheck->setChecked(parseSmoothLighting(m_options.rawValue(QStringLiteral("ao")), true));
    m_entityShadowsCheck->setChecked(m_options.boolValue(QStringLiteral("entityShadows")).value_or(true));
    m_viewBobbingCheck->setChecked(m_options.boolValue(QStringLiteral("bobView")).value_or(true));
    m_fullscreenCheck->setChecked(m_options.boolValue(QStringLiteral("fullscreen")).value_or(false));
    m_vsyncCheck->setChecked(m_options.boolValue(QStringLiteral("enableVsync")).value_or(true));

    m_masterVolSpin->setValue(soundToPercent(m_options.doubleValue(QStringLiteral("soundCategory_master")).value_or(1.0)));
    m_musicVolSpin->setValue(soundToPercent(m_options.doubleValue(QStringLiteral("soundCategory_music")).value_or(1.0)));
    m_weatherVolSpin->setValue(soundToPercent(m_options.doubleValue(QStringLiteral("soundCategory_weather")).value_or(1.0)));
    m_blocksVolSpin->setValue(soundToPercent(m_options.doubleValue(QStringLiteral("soundCategory_block")).value_or(1.0)));
    m_hostileVolSpin->setValue(soundToPercent(m_options.doubleValue(QStringLiteral("soundCategory_hostile")).value_or(1.0)));
    m_playerVolSpin->setValue(soundToPercent(m_options.doubleValue(QStringLiteral("soundCategory_player")).value_or(1.0)));
}

void GameOptionsPage::collectBasicControls()
{
    m_options.setIntValue(QStringLiteral("renderDistance"), m_renderDistSpin->value());
    m_options.setIntValue(QStringLiteral("simulationDistance"), m_simDistSpin->value());
    m_options.setDoubleValue(QStringLiteral("entityDistanceScaling"), percentToEntityDist(m_entityDistSpin->value()));
    m_options.setIntValue(QStringLiteral("maxFps"), m_maxFpsSpin->value());
    m_options.setDoubleValue(QStringLiteral("fov"), degreesToFovOption(m_fovSpin->value()));
    m_options.setDoubleValue(QStringLiteral("gamma"), percentToGamma(m_gammaSpin->value()));
    m_options.setIntValue(QStringLiteral("guiScale"), m_guiScaleCombo->currentIndex());
    m_options.setIntValue(QStringLiteral("graphicsMode"), m_graphicsCombo->currentIndex());
    m_options.setRawValue(QStringLiteral("renderClouds"), cloudsIndexToValue(m_cloudsCombo->currentIndex()));
    m_options.setIntValue(QStringLiteral("particles"), m_particlesCombo->currentIndex());
    m_options.setIntValue(QStringLiteral("biomeBlendRadius"), m_biomeBlendCombo->currentIndex());
    m_options.setIntValue(QStringLiteral("mipmapLevels"), m_mipmapSpin->value());
    m_options.setBoolValue(QStringLiteral("ao"), m_smoothLightingCheck->isChecked());
    m_options.setBoolValue(QStringLiteral("entityShadows"), m_entityShadowsCheck->isChecked());
    m_options.setBoolValue(QStringLiteral("bobView"), m_viewBobbingCheck->isChecked());
    m_options.setBoolValue(QStringLiteral("fullscreen"), m_fullscreenCheck->isChecked());
    m_options.setBoolValue(QStringLiteral("enableVsync"), m_vsyncCheck->isChecked());

    m_options.setDoubleValue(QStringLiteral("soundCategory_master"), percentToSound(m_masterVolSpin->value()));
    m_options.setDoubleValue(QStringLiteral("soundCategory_music"), percentToSound(m_musicVolSpin->value()));
    m_options.setDoubleValue(QStringLiteral("soundCategory_weather"), percentToSound(m_weatherVolSpin->value()));
    m_options.setDoubleValue(QStringLiteral("soundCategory_block"), percentToSound(m_blocksVolSpin->value()));
    m_options.setDoubleValue(QStringLiteral("soundCategory_hostile"), percentToSound(m_hostileVolSpin->value()));
    m_options.setDoubleValue(QStringLiteral("soundCategory_player"), percentToSound(m_playerVolSpin->value()));
}

void GameOptionsPage::applyPresetValues(const GameGraphicsAdvisor::PresetValues& values)
{
    m_renderDistSpin->setValue(values.renderDistance);
    m_simDistSpin->setValue(values.simulationDistance);
    m_entityDistSpin->setValue(values.entityDistancePercent);
    m_graphicsCombo->setCurrentIndex(values.graphicsMode);
    m_cloudsCombo->setCurrentIndex(cloudsValueToIndex(values.renderClouds));
    m_particlesCombo->setCurrentIndex(values.particles);
    m_smoothLightingCheck->setChecked(values.smoothLighting);
    m_entityShadowsCheck->setChecked(values.entityShadows);
    m_biomeBlendCombo->setCurrentIndex(values.biomeBlendRadius);
    m_mipmapSpin->setValue(values.mipmapLevels);
    m_maxFpsSpin->setValue(values.maxFps);
    m_vsyncCheck->setChecked(values.vsync);

    if (m_sodiumConfigIndex >= 0) {
        m_leavesCombo->setCurrentText(values.leavesQuality);
        m_weatherCombo->setCurrentText(values.weatherQuality);
        m_vignetteCheck->setChecked(values.enableVignette);
        if (values.chunkBuilderThreads > 0) {
            m_chunkThreadsSpin->setValue(values.chunkBuilderThreads);
        }
        m_deferModeCombo->setCurrentText(values.chunkDeferMode);
        m_entityCullCheck->setChecked(values.useEntityCulling);
        m_fogOcclusionCheck->setChecked(values.useFogOcclusion);
        m_blockCullCheck->setChecked(values.useBlockFaceCulling);
        m_animVisibleCheck->setChecked(values.animateOnlyVisibleTextures);
        m_renderAheadSpin->setValue(values.cpuRenderAheadLimit);
    }
}

void GameOptionsPage::applyPreset(int presetIndex)
{
    const auto preset = static_cast<GameGraphicsAdvisor::QualityPreset>(qBound(0, presetIndex, 4));
    if (m_presetGroup) {
        if (auto* btn = m_presetGroup->button(static_cast<int>(preset))) {
            btn->setChecked(true);
        }
    }
    const auto values = GameGraphicsAdvisor::presetValues(preset, 0);
    applyPresetValues(values);
    m_statusLabel->setText(tr("Preset \"%1\" applied. Click \"Save Changes\" to write config files.")
                               .arg(GameGraphicsAdvisor::presetDisplayName(preset)));
}

void GameOptionsPage::applyAutoHardwarePreset()
{
    const auto hw = GameGraphicsAdvisor::detectSystemHardware();
    const auto preset = GameGraphicsAdvisor::recommendPreset(hw);
    if (m_presetGroup) {
        if (auto* btn = m_presetGroup->button(static_cast<int>(preset))) {
            btn->setChecked(true);
        }
    }
    const auto values = GameGraphicsAdvisor::presetValues(preset, hw.cpuThreads);
    applyPresetValues(values);

    const QString presetName = GameGraphicsAdvisor::presetDisplayName(preset);
    if (m_hardwareSummaryLabel) {
        m_hardwareSummaryLabel->setText(
            tr("Detected PC: %1 -> Recommended profile: %2").arg(GameGraphicsAdvisor::formatHardwareSummary(hw), presetName));
    }
    m_statusLabel->setText(
        tr("Auto-tuned to \"%1\" based on your hardware. Click \"Save Changes\" to apply.").arg(presetName));
}
