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
#include "ModFolderHaloTools.h"

#include <QAction>
#include <QActionGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include "BaseInstance.h"
#include "minecraft/mod/ModBisectSession.h"
#include "minecraft/mod/ModFolderModel.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/pages/instance/InstanceProfilePreset.h"
#include "ui/widgets/WideBar.h"

namespace {

class ModBisectBanner : public QFrame {
    Q_OBJECT

   public:
    ModBisectBanner(BaseInstance* instance, ModFolderModel* model, QWidget* parent = nullptr)
        : QFrame(parent), m_instance(instance), m_model(model)
    {
        setFrameShape(QFrame::StyledPanel);
        setObjectName(QStringLiteral("modBisectBanner"));

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(10, 8, 10, 8);
        root->setSpacing(6);

        m_statusLabel = new QLabel(this);
        m_statusLabel->setWordWrap(true);
        root->addWidget(m_statusLabel);

        auto* btnRow = new QHBoxLayout();
        btnRow->setSpacing(6);

        m_presentBtn = new QPushButton(tr("Bug Still Happens"), this);
        m_absentBtn = new QPushButton(tr("Bug Fixed"), this);
        m_keepDisabledBtn = new QPushButton(tr("Keep Culprit Disabled"), this);
        m_restoreBtn = new QPushButton(tr("Cancel && Restore All"), this);

        connect(m_presentBtn, &QPushButton::clicked, this, [this]() { runStep(StepKind::BugPresent); });
        connect(m_absentBtn, &QPushButton::clicked, this, [this]() { runStep(StepKind::BugAbsent); });
        connect(m_keepDisabledBtn, &QPushButton::clicked, this, [this]() { runStep(StepKind::KeepDisabled); });
        connect(m_restoreBtn, &QPushButton::clicked, this, [this]() { runStep(StepKind::RestoreAll); });

        btnRow->addWidget(m_presentBtn);
        btnRow->addWidget(m_absentBtn);
        btnRow->addWidget(m_keepDisabledBtn);
        btnRow->addWidget(m_restoreBtn);
        btnRow->addStretch(1);
        root->addLayout(btnRow);

        refresh();
    }

    void startBisect()
    {
        if (!m_instance) {
            return;
        }
        if (m_instance->isRunning()) {
            CustomMessageBox::selectable(this, tr("Instance Running"),
                                         tr("Stop Minecraft before starting mod bisect."), QMessageBox::Warning)
                ->show();
            return;
        }
        if (ModBisectSession::hasActiveSession(m_instance)) {
            refresh();
            return;
        }
        const auto confirm =
            CustomMessageBox::selectable(
                this, tr("Bisect Mods"),
                tr("Start binary search across enabled mods?\n\n"
                   "Half of your enabled mods will be temporarily disabled each step until you isolate the culprit."),
                QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                ->exec();
        if (confirm != QMessageBox::Yes || !m_instance) {
            return;
        }
        QString err;
        if (!ModBisectSession::start(m_instance, &err)) {
            CustomMessageBox::selectable(this, tr("Bisect Unavailable"), err, QMessageBox::Warning)->show();
            return;
        }
        if (m_model) {
            m_model->update();
        }
        refresh();
    }

    void refresh()
    {
        const ModBisectSession::Status status = ModBisectSession::loadStatus(m_instance);
        setVisible(status.active);
        if (!status.active) {
            return;
        }
        const bool culpritFound = !status.culpritMod.isEmpty();
        if (culpritFound) {
            m_statusLabel->setText(tr("Mod Bisect finished — suspected culprit: %1").arg(status.culpritMod));
            m_restoreBtn->setText(tr("Restore All Mods"));
        } else {
            const QString depsHint = status.dependencySupportCount > 0
                                         ? tr(" (including %1 required dependencies)").arg(status.dependencySupportCount)
                                         : QString();
            m_statusLabel->setText(
                tr("Mod Bisect (Step %1 of ~%2) — Testing %3 mods%5 (%4 candidates remaining). Launch the game and report whether the bug occurs:")
                    .arg(status.stepNumber)
                    .arg(status.estimatedTotalSteps)
                    .arg(status.activeTestingCount)
                    .arg(status.remainingCount)
                    .arg(depsHint));
            m_restoreBtn->setText(tr("Cancel && Restore All"));
        }
        m_presentBtn->setVisible(!culpritFound);
        m_absentBtn->setVisible(!culpritFound);
        m_keepDisabledBtn->setVisible(culpritFound);
    }

   private:
    enum class StepKind { BugPresent, BugAbsent, KeepDisabled, RestoreAll };

    void runStep(StepKind kind)
    {
        if (!m_instance) {
            return;
        }
        if (m_instance->isRunning()) {
            CustomMessageBox::selectable(this, tr("Instance Running"),
                                         tr("Close Minecraft before changing mod states during bisect."), QMessageBox::Warning)
                ->show();
            return;
        }
        QString err;
        bool ok = false;
        const ModBisectSession::Status before = ModBisectSession::loadStatus(m_instance);
        if (kind == StepKind::BugPresent) {
            ok = ModBisectSession::recordBugPresent(m_instance, &err);
        } else if (kind == StepKind::BugAbsent) {
            ok = ModBisectSession::recordBugAbsent(m_instance, &err);
        } else if (kind == StepKind::KeepDisabled) {
            ok = ModBisectSession::keepCulpritDisabledAndClear(m_instance, &err);
        } else {
            ok = !before.culpritMod.isEmpty() ? ModBisectSession::restoreAllAndClear(m_instance, &err)
                                              : ModBisectSession::cancelAndRestore(m_instance, &err);
        }
        if (!ok) {
            CustomMessageBox::selectable(this, tr("Bisect Error"), err, QMessageBox::Warning)->show();
        }
        if (m_model) {
            m_model->update();
        }
        refresh();
    }

    QPointer<BaseInstance> m_instance;
    QPointer<ModFolderModel> m_model;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_presentBtn = nullptr;
    QPushButton* m_absentBtn = nullptr;
    QPushButton* m_keepDisabledBtn = nullptr;
    QPushButton* m_restoreBtn = nullptr;
};

void populatePresetsMenu(QMenu* menu, BaseInstance* instance, ModFolderModel* model, QWidget* parent)
{
    menu->clear();
    if (!instance) {
        return;
    }

    const bool running = instance->isRunning();
    const bool bisecting = ModBisectSession::hasActiveSession(instance);
    if (running || bisecting) {
        QAction* locked = menu->addAction(
            running ? QObject::tr("Stop Minecraft before switching profiles") : QObject::tr("Finish Mod Bisect before switching profiles"));
        locked->setEnabled(false);
        menu->addSeparator();
    }

    const QList<InstanceProfilePreset::Preset> presets = InstanceProfilePreset::listPresets(instance);
    const QString activeId = InstanceProfilePreset::activePresetId(instance);
    auto* group = new QActionGroup(menu);
    group->setExclusive(true);

    if (presets.isEmpty()) {
        QAction* emptyAct = menu->addAction(QObject::tr("No saved presets yet"));
        emptyAct->setEnabled(false);
    } else {
        for (const auto& preset : presets) {
            QAction* act = menu->addAction(QStringLiteral("%1 (%2)").arg(preset.name).arg(preset.enabledMods.size()));
            act->setCheckable(true);
            act->setChecked(preset.id == activeId);
            act->setEnabled(!running && !bisecting);
            group->addAction(act);
            const QString presetId = preset.id;
            QPointer<BaseInstance> instPtr(instance);
            QPointer<ModFolderModel> modelPtr(model);
            QObject::connect(act, &QAction::triggered, menu, [instPtr, modelPtr, parent, presetId]() {
                if (!instPtr) {
                    return;
                }
                QString err;
                if (!InstanceProfilePreset::applyPreset(instPtr, presetId, &err)) {
                    CustomMessageBox::selectable(parent, QObject::tr("Preset Switch Failed"), err, QMessageBox::Warning)->show();
                    return;
                }
                if (modelPtr) {
                    modelPtr->update();
                }
            });
        }
    }

    menu->addSeparator();
    QAction* saveAct = menu->addAction(QObject::tr("Save Current as Preset..."));
    saveAct->setEnabled(!running && !bisecting);
    QPointer<BaseInstance> instPtr(instance);
    QObject::connect(saveAct, &QAction::triggered, menu, [instPtr, parent]() {
        if (!instPtr) {
            return;
        }
        bool ok = false;
        const QString name =
            QInputDialog::getText(parent, QObject::tr("Save Mod Profile Preset"), QObject::tr("Preset name:"), QLineEdit::Normal, {}, &ok);
        if (!ok || name.trimmed().isEmpty()) {
            return;
        }
        QString err;
        if (!InstanceProfilePreset::saveCurrentAsPreset(instPtr, name.trimmed(), true, &err)) {
            CustomMessageBox::selectable(parent, QObject::tr("Save Preset Failed"), err, QMessageBox::Warning)->show();
        }
    });

    if (!presets.isEmpty()) {
        QMenu* delMenu = menu->addMenu(QObject::tr("Delete Preset"));
        for (const auto& preset : presets) {
            QAction* delAct = delMenu->addAction(preset.name);
            const QString presetId = preset.id;
            QObject::connect(delAct, &QAction::triggered, menu, [instPtr, presetId]() {
                if (instPtr) {
                    InstanceProfilePreset::deletePreset(instPtr, presetId);
                }
            });
        }
    }
}

}  // namespace

namespace ModFolderHaloTools {

void attach(QWidget* page,
            BaseInstance* instance,
            ModFolderModel* model,
            QGridLayout* gridLayout,
            WideBar* toolbar,
            QAction* insertAfterAction)
{
    if (!page || !instance || !gridLayout || !toolbar) {
        return;
    }

    auto* banner = new ModBisectBanner(instance, model, page);
    gridLayout->addWidget(banner, 0, 1, 1, 2);

    auto* bisectAction = new QAction(QIcon::fromTheme(QStringLiteral("bug")), QObject::tr("Bisect Mods"), page);
    bisectAction->setToolTip(QObject::tr("Binary-search enabled mods to find a crashing or broken mod"));
    QObject::connect(bisectAction, &QAction::triggered, banner, &ModBisectBanner::startBisect);

    auto* profilesMenu = new QMenu(page);
    QPointer<BaseInstance> instPtr(instance);
    QPointer<ModFolderModel> modelPtr(model);
    QObject::connect(profilesMenu, &QMenu::aboutToShow, profilesMenu, [profilesMenu, instPtr, modelPtr, page]() {
        populatePresetsMenu(profilesMenu, instPtr, modelPtr, page);
    });

    auto* profilesAction = new QAction(QIcon::fromTheme(QStringLiteral("packages")), QObject::tr("Mod Profiles"), page);
    profilesAction->setToolTip(QObject::tr("Save and switch between mod & shader profile presets"));
    profilesAction->setMenu(profilesMenu);

    if (insertAfterAction) {
        toolbar->insertActionAfter(insertAfterAction, profilesAction);
        toolbar->insertActionAfter(insertAfterAction, bisectAction);
    } else {
        toolbar->addAction(bisectAction);
        toolbar->addAction(profilesAction);
    }

    if (auto* btn = qobject_cast<QToolButton*>(toolbar->widgetForAction(profilesAction))) {
        btn->setPopupMode(QToolButton::InstantPopup);
    }
}

}  // namespace ModFolderHaloTools

#include "ModFolderHaloTools.moc"
