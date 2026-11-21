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
#include "InstanceInspector.h"

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QHideEvent>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QShowEvent>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <optional>

#include "Application.h"
#include "BaseInstance.h"
#include "MMCTime.h"
#include "diagnostics/ProcessMemory.h"
#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "ui/pages/instance/ManagedPackUpdater.h"
#include "ui/pages/instance/PackSnapshot.h"
#include "ui/pages/instance/ServerPingTask.h"
#include "ui/widgets/UiAnimations.h"
#include "ui/widgets/UiMotionEffects.h"

namespace {
QString detectLoaderSummary(MinecraftInstance* mcInst)
{
    if (!mcInst || !mcInst->getPackProfile()) {
        return {};
    }
    auto* profile = mcInst->getPackProfile();
    const QString mc = profile->getComponentVersion(QStringLiteral("net.minecraft"));
    if (mc.isEmpty()) {
        return {};
    }

    struct LoaderEntry {
        const char* uid;
        const char* name;
    };
    static constexpr LoaderEntry kLoaders[] = {
        { "net.neoforged", "NeoForge" },
        { "net.neoforge", "NeoForge" },
        { "net.minecraftforge", "Forge" },
        { "net.fabricmc.fabric-loader", "Fabric" },
        { "org.quiltmc.quilt-loader", "Quilt" },
    };
    for (const auto& entry : kLoaders) {
        if (!profile->getComponentVersion(QLatin1String(entry.uid)).isEmpty()) {
            return QStringLiteral("Minecraft %1 • %2").arg(mc, QLatin1String(entry.name));
        }
    }
    return QStringLiteral("Minecraft %1").arg(mc);
}

QString formatSessionElapsed(qint64 seconds)
{
    const qint64 clamped = qMax<qint64>(0, seconds);
    const qint64 hours = clamped / 3600;
    const qint64 minutes = (clamped % 3600) / 60;
    const qint64 secs = clamped % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(minutes, 2, 10, QLatin1Char('0')).arg(secs, 2, 10, QLatin1Char('0'));
}
}  // namespace

void InstanceInspector::setInstance(BaseInstance* instance, const QIcon& icon)
{
    const BaseInstance* prevInstance = m_instance.data();
    const int prevPage = m_stack->currentIndex();
    const int nextPage = instance ? 1 : 0;
    const bool switchingContent = m_initialized && isVisible() && (prevInstance != instance || prevPage != nextPage);
    const QPixmap snapshot =
        (switchingContent && UiAnimations::animationsEnabled() && m_stack->size().isValid()) ? m_stack->grab() : QPixmap();

    m_instance = instance;
    if (!instance) {
        m_pingInstanceId.clear();
        m_pingAddressKey.clear();
        m_lastPingMs = 0;
        m_lastPingPlayers.reset();
        m_quickJoinPingTask = nullptr;
        if (m_sessionTimer) {
            m_sessionTimer->stop();
        }
        if (m_sessionCard) {
            m_sessionCard->hide();
        }
        m_stack->setCurrentIndex(0);
        if (!snapshot.isNull()) {
            UiAnimations::TransitionOverlay::start(m_stack, snapshot, QPoint(0, 6), 120);
        }
        return;
    }

    m_stack->setCurrentIndex(1);
    m_iconButton->setIcon(icon);
    m_titleLabel->setText(instance->name());

    m_runningBadge->setVisible(instance->isRunning());
    m_updateBadge->setVisible(instance->hasUpdateAvailable());
    m_crashedBadge->setVisible(!instance->isRunning() && (instance->hasCrashed() || instance->hasVersionBroken()));

    const bool animateVisibility = m_initialized && isVisible() && prevPage == 1;
    updatePinButtonState();
    updateMetadata(instance, animateVisibility);
    updatePlayButtonState();
    updateQuickJoinState(animateVisibility);
    syncSessionCardVisibility(animateVisibility);

    if (!snapshot.isNull()) {
        UiAnimations::TransitionOverlay::start(m_stack, snapshot, QPoint(0, 6), 120);
    }
}

void InstanceInspector::updateMetadata(BaseInstance* instance, bool animateVisibility)
{
    if (!instance) {
        return;
    }

    QString summary = detectLoaderSummary(dynamic_cast<MinecraftInstance*>(instance));
    if (summary.isEmpty()) {
        summary = instance->typeName();
    }
    m_versionLabel->setText(summary);

    if (instance->isManagedPack()) {
        QString packText = instance->getManagedPackName();
        if (!instance->getManagedPackVersionName().isEmpty()) {
            packText = packText.isEmpty() ? instance->getManagedPackVersionName()
                                          : QStringLiteral("%1 (%2)").arg(packText, instance->getManagedPackVersionName());
        }
        m_packLabel->setText(tr("Modpack: %1").arg(packText));
        m_packLabel->setVisible(!packText.isEmpty());
    } else {
        m_packLabel->setVisible(false);
    }

    if (m_updatePackButton) {
        const bool canUpdatePack = ManagedPackUpdater::supportsOneStepUpdate(instance);
        const bool hasUpdate = canUpdatePack && instance->hasUpdateAvailable();
        m_updatePackButton->setProperty("updateAvailable", hasUpdate);
        m_updatePackButton->style()->unpolish(m_updatePackButton);
        m_updatePackButton->style()->polish(m_updatePackButton);
        UiAnimations::animateVisibleHeight(m_updatePackButton, canUpdatePack, animateVisibility ? 160 : 0);
    }

    if (m_rollbackButton) {
        const QList<PackSnapshot::Info> snapshots = instance->isRunning() ? QList<PackSnapshot::Info>{} : PackSnapshot::listSnapshots(instance);
        const bool showRollback = !snapshots.isEmpty();
        if (showRollback) {
            const PackSnapshot::Info& latest = snapshots.constFirst();
            const QString when = QLocale().toString(latest.createdAt, QLocale::ShortFormat);
            const QString ver = !latest.packVersion.isEmpty() ? latest.packVersion : QStringLiteral("-");
            m_rollbackButton->setToolTip(tr("Restore snapshot from %1 (version %2)").arg(when, ver));
        }
        UiAnimations::animateVisibleHeight(m_rollbackButton, showRollback, animateVisibility ? 160 : 0);
    }

    QStringList timeParts;
    if (instance->totalTimePlayed() > 0) {
        const bool showWithoutDays = APPLICATION_DYN ? APPLICATION_DYN->settings()->get("ShowGameTimeWithoutDays").toBool() : false;
        timeParts.append(tr("Played %1").arg(Time::prettifyDuration(instance->totalTimePlayed(), showWithoutDays)));
    }
    if (instance->lastLaunch() > 0) {
        QDateTime last = QDateTime::fromMSecsSinceEpoch(instance->lastLaunch());
        timeParts.append(tr("Last launch: %1").arg(QLocale().toString(last.date(), QLocale::ShortFormat)));
    }
    if (timeParts.isEmpty()) {
        m_playtimeLabel->setText(tr("Ready to launch"));
    } else {
        m_playtimeLabel->setText(timeParts.join(QStringLiteral(" • ")));
    }

    m_editButton->setEnabled(instance->canEdit());
    m_exportButton->setEnabled(instance->canExport());
    if (m_actions.exportInstance) {
        m_exportButton->setMenu(m_actions.exportInstance->menu());
        m_exportButton->setPopupMode(m_actions.exportInstance->menu() ? QToolButton::InstantPopup : QToolButton::DelayedPopup);
    }
}

void InstanceInspector::updatePlayButtonState()
{
    if (!m_playButton) {
        return;
    }
    const bool running = m_instance && m_instance->isRunning();
    m_playButton->setProperty("running", running);
    m_playButton->style()->unpolish(m_playButton);
    m_playButton->style()->polish(m_playButton);
    UiAnimations::setButtonBusyShimmer(m_playButton, running);

    if (running) {
        m_playButton->setText(tr("Stop"));
        m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("status-bad")));
        m_playButton->setMenu(nullptr);
        m_playButton->setPopupMode(QToolButton::DelayedPopup);
        m_playButton->setEnabled(true);
    } else {
        m_playButton->setText(tr("Play"));
        m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("launch")));
        QMenu* launchMenu = m_actions.launchInstance ? m_actions.launchInstance->menu() : nullptr;
        m_playButton->setMenu(launchMenu && !launchMenu->isEmpty() ? launchMenu : nullptr);
        m_playButton->setPopupMode((launchMenu && !launchMenu->isEmpty()) ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
        m_playButton->setEnabled(m_instance && m_instance->canLaunch());
    }
}

void InstanceInspector::retranslateUi()
{
    m_emptyTitleLabel->setText(tr("No instance selected"));
    m_emptyHintLabel->setText(tr("Select an instance from the grid or create a new one to play."));
    m_emptyAddButton->setText(tr("Add Instance"));

    m_runningBadge->setText(tr("Running"));
    m_updateBadge->setText(tr("Update"));
    m_crashedBadge->setText(tr("Issue"));

    m_iconButton->setToolTip(tr("Change Icon"));
    m_renameButton->setToolTip(tr("Rename Instance"));
    if (m_sessionLogButton) {
        m_sessionLogButton->setText(tr("Open Log"));
    }

    m_updatePackButton->setText(tr("Update Modpack"));
    m_updatePackButton->setToolTip(tr("Check and install the latest modpack version"));
    if (m_rollbackButton) {
        m_rollbackButton->setText(tr("Rollback Update"));
    }
    m_editButton->setText(tr("Edit & Mods"));
    m_folderButton->setText(tr("Folder"));
    m_groupButton->setText(tr("Group"));
    m_copyButton->setText(tr("Copy"));
    m_exportButton->setText(tr("Export"));
    m_shortcutButton->setText(tr("Shortcut"));
    m_shareLogButton->setText(tr("Share Log"));
    m_shareLogButton->setToolTip(tr("Upload latest log to mclo.gs"));
    if (m_diffModsButton) {
        m_diffModsButton->setText(tr("Mod Diff"));
        m_diffModsButton->setToolTip(tr("Compare mods with a snapshot or another instance"));
    }
    if (m_cleanStorageButton) {
        m_cleanStorageButton->setText(tr("Cleanup"));
        m_cleanStorageButton->setToolTip(tr("Scan and clean logs, caches, and old snapshots"));
    }
    m_deleteButton->setText(tr("Delete Instance"));

    if (m_instance) {
        updatePinButtonState();
        updateMetadata(m_instance, false);
        if (m_quickJoinButton && m_activeQuickJoin.has_value()) {
            const auto& active = *m_activeQuickJoin;
            if (active.kind == QuickJoinHelper::Target::Kind::Server && m_lastPingPlayers.has_value() && *m_lastPingPlayers >= 0) {
                m_quickJoinButton->setText(tr("Join: %1 (%2 online)").arg(active.label).arg(*m_lastPingPlayers));
            } else {
                m_quickJoinButton->setText(tr("Join: %1").arg(active.label));
            }
            m_quickJoinButton->setToolTip(tr("Quick-join \"%1\" (%2)").arg(active.label, active.targetValue));
        }
        updateSessionMetrics();
    }
    updatePlayButtonState();
}

void InstanceInspector::updateQuickJoinState(bool animateVisibility)
{
    if (!m_quickJoinButton) {
        return;
    }

    const QList<QuickJoinHelper::Target> allTargets = m_instance ? QuickJoinHelper::discoverTargets(m_instance) : QList<QuickJoinHelper::Target>{};
    m_activeQuickJoin = m_instance ? QuickJoinHelper::resolveActiveTarget(m_instance, allTargets) : std::nullopt;
    if (!m_instance || !m_activeQuickJoin.has_value()) {
        m_pingInstanceId.clear();
        m_pingAddressKey.clear();
        m_lastPingMs = 0;
        m_lastPingPlayers.reset();
        m_quickJoinPingTask = nullptr;
        UiAnimations::animateVisibleHeight(m_quickJoinButton, false, animateVisibility ? 140 : 0);
        return;
    }

    const QuickJoinHelper::Target active = *m_activeQuickJoin;
    const QString instId = m_instance->id();
    const bool sameTarget = (m_pingInstanceId == instId && m_pingAddressKey == active.targetValue);
    if (!sameTarget) {
        m_lastPingMs = 0;
        m_lastPingPlayers.reset();
        m_quickJoinPingTask = nullptr;
    }

    m_quickJoinButton->setIcon(
        QIcon::fromTheme(active.kind == QuickJoinHelper::Target::Kind::World ? QStringLiteral("worlds") : QStringLiteral("server")));
    if (active.kind == QuickJoinHelper::Target::Kind::Server && sameTarget && m_lastPingPlayers.has_value() && *m_lastPingPlayers >= 0) {
        m_quickJoinButton->setText(tr("Join: %1 (%2 online)").arg(active.label).arg(*m_lastPingPlayers));
    } else {
        m_quickJoinButton->setText(tr("Join: %1").arg(active.label));
    }
    m_quickJoinButton->setToolTip(tr("Quick-join \"%1\" (%2)").arg(active.label, active.targetValue));
    m_quickJoinButton->setEnabled(!m_instance->isRunning() && m_instance->canLaunch());
    UiAnimations::animateVisibleHeight(m_quickJoinButton, true, animateVisibility ? 140 : 0);

    if (m_quickJoinMenu) {
        m_quickJoinMenu->clear();
        for (const auto& t : allTargets) {
            const QString icon = (t.kind == QuickJoinHelper::Target::Kind::World) ? QStringLiteral("worlds") : QStringLiteral("server");
            QAction* act = m_quickJoinMenu->addAction(QIcon::fromTheme(icon), t.label);
            act->setCheckable(true);
            act->setChecked(t.kind == active.kind && t.targetValue == active.targetValue);
            QPointer<BaseInstance> instPtr(m_instance);
            connect(act, &QAction::triggered, this, [this, instPtr, t]() {
                if (instPtr) {
                    QuickJoinHelper::savePinnedTarget(instPtr, t);
                    updateQuickJoinState(false);
                }
            });
        }
        m_quickJoinMenu->addSeparator();
        QAction* resetAct = m_quickJoinMenu->addAction(tr("Auto (Reset Pin)"));
        QPointer<BaseInstance> instPtr(m_instance);
        connect(resetAct, &QAction::triggered, this, [this, instPtr]() {
            if (instPtr) {
                QuickJoinHelper::savePinnedTarget(instPtr, std::nullopt);
                updateQuickJoinState(false);
            }
        });
    }

    if (active.kind == QuickJoinHelper::Target::Kind::Server && !m_instance->isRunning()) {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        constexpr qint64 kPingCacheMs = 30000;
        if (sameTarget && (m_quickJoinPingTask != nullptr || (m_lastPingMs > 0 && (nowMs - m_lastPingMs) < kPingCacheMs))) {
            return;
        }
        const auto parsed = MinecraftTarget::parse(active.targetValue, false);
        m_pingInstanceId = instId;
        m_pingAddressKey = active.targetValue;
        m_lastPingMs = nowMs;
        auto* ping = new ServerPingTask(parsed.address, parsed.port);
        ping->setParent(this);
        connect(ping, &Task::finished, ping, &QObject::deleteLater);
        m_quickJoinPingTask = ping;
        const QString pingKey = m_pingAddressKey;
        const QString label = active.label;
        connect(ping, &Task::finished, this, [this, ping, instId, pingKey, label]() {
            if (m_pingInstanceId == instId && m_pingAddressKey == pingKey) {
                m_lastPingPlayers = ping->m_outputOnlinePlayers;
                m_lastPingMs = QDateTime::currentMSecsSinceEpoch();
                if (m_quickJoinButton && ping->m_outputOnlinePlayers >= 0) {
                    m_quickJoinButton->setText(tr("Join: %1 (%2 online)").arg(label).arg(ping->m_outputOnlinePlayers));
                }
            }
        });
        ping->start();
    } else {
        m_pingInstanceId.clear();
        m_pingAddressKey.clear();
        m_lastPingMs = 0;
        m_lastPingPlayers.reset();
    }
}

void InstanceInspector::updatePinButtonState()
{
    if (!m_pinButton) {
        return;
    }
    const bool pinned = m_instance && m_instance->isPinned();
    m_pinButton->setChecked(pinned);
    m_pinButton->setEnabled(m_instance != nullptr);
    m_pinButton->setToolTip(pinned ? tr("Unpin from Top") : tr("Pin to Top"));
}

void InstanceInspector::syncSessionCardVisibility(bool animate)
{
    const bool running = m_instance && m_instance->isRunning();
    if (running) {
        updateSessionMetrics();
        if (m_sessionTimer && isVisible() && !m_sessionTimer->isActive()) {
            m_sessionTimer->start();
        }
    } else if (m_sessionTimer) {
        m_sessionTimer->stop();
    }
    if (m_sessionCard) {
        UiAnimations::animateVisibleHeight(m_sessionCard, running, animate ? 150 : 0);
    }
}

void InstanceInspector::updateSessionMetrics()
{
    if (!m_instance || !m_instance->isRunning()) {
        if (m_sessionTimer) {
            m_sessionTimer->stop();
        }
        return;
    }

    const QString elapsed = formatSessionElapsed(m_instance->lastTimePlayed());
    m_sessionDurationLabel->setText(tr("Session: %1").arg(elapsed));

    std::optional<qint64> ramBytes;
    if (auto* launchTask = m_instance->getLaunchTask()) {
        ramBytes = ProcessMemory::queryProcessWorkingSetBytes(launchTask->pid());
    }
    if (ramBytes.has_value()) {
        m_sessionRamLabel->setText(QStringLiteral("RAM: %1").arg(ProcessMemory::formatMemorySize(*ramBytes)));
        m_sessionRamLabel->setVisible(true);
    } else {
        m_sessionRamLabel->clear();
        m_sessionRamLabel->setVisible(false);
    }
}

void InstanceInspector::hideEvent(QHideEvent* event)
{
    if (m_sessionTimer) {
        m_sessionTimer->stop();
    }
    QWidget::hideEvent(event);
}

void InstanceInspector::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_instance && m_instance->isRunning()) {
        updateSessionMetrics();
        if (m_sessionTimer && !m_sessionTimer->isActive()) {
            m_sessionTimer->start();
        }
    }
}
