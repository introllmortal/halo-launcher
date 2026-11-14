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
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>

#include "Application.h"
#include "BaseInstance.h"
#include "MMCTime.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "translations/TranslationsModel.h"
#include "ui/pages/instance/ManagedPackUpdater.h"
#include "ui/widgets/UiAnimations.h"

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
}  // namespace

QString InstanceInspector::trLabel(const QString& translated, const char* englishLiteral, const QString& ruFallback)
{
    return TranslationsModel::trFallback(APPLICATION_DYN ? APPLICATION_DYN->translations() : nullptr, translated, englishLiteral,
                                         ruFallback);
}

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
    updateMetadata(instance, animateVisibility);
    updatePlayButtonState();

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
        m_packLabel->setText(trLabel(tr("Modpack: %1"), "Modpack: %1", QStringLiteral("Сборка: %1")).arg(packText));
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

    QStringList timeParts;
    if (instance->totalTimePlayed() > 0) {
        const bool showWithoutDays = APPLICATION_DYN ? APPLICATION_DYN->settings()->get("ShowGameTimeWithoutDays").toBool() : false;
        timeParts.append(trLabel(tr("Played %1"), "Played %1", QStringLiteral("Наиграно %1"))
                             .arg(Time::prettifyDuration(instance->totalTimePlayed(), showWithoutDays)));
    }
    if (instance->lastLaunch() > 0) {
        QDateTime last = QDateTime::fromMSecsSinceEpoch(instance->lastLaunch());
        timeParts.append(trLabel(tr("Last launch: %1"), "Last launch: %1", QStringLiteral("Запуск: %1"))
                             .arg(QLocale().toString(last.date(), QLocale::ShortFormat)));
    }
    if (timeParts.isEmpty()) {
        m_playtimeLabel->setText(trLabel(tr("Ready to launch"), "Ready to launch", QStringLiteral("Готово к запуску")));
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

    if (running) {
        m_playButton->setText(trLabel(tr("Stop"), "Stop", QStringLiteral("Остановить")));
        m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("status-bad")));
        m_playButton->setMenu(nullptr);
        m_playButton->setPopupMode(QToolButton::DelayedPopup);
        m_playButton->setEnabled(true);
    } else {
        m_playButton->setText(trLabel(tr("Play"), "Play", QStringLiteral("Играть")));
        m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("launch")));
        QMenu* launchMenu = m_actions.launchInstance ? m_actions.launchInstance->menu() : nullptr;
        m_playButton->setMenu(launchMenu && !launchMenu->isEmpty() ? launchMenu : nullptr);
        m_playButton->setPopupMode((launchMenu && !launchMenu->isEmpty()) ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
        m_playButton->setEnabled(m_instance && m_instance->canLaunch());
    }
}

void InstanceInspector::retranslateUi()
{
    m_emptyTitleLabel->setText(trLabel(tr("No instance selected"), "No instance selected", QStringLiteral("Сборка не выбрана")));
    m_emptyHintLabel->setText(trLabel(tr("Select an instance from the grid or create a new one to play."),
                                      "Select an instance from the grid or create a new one to play.",
                                      QStringLiteral("Выберите сборку в списке или создайте новую для запуска.")));
    m_emptyAddButton->setText(trLabel(tr("Add Instance"), "Add Instance", QStringLiteral("Добавить сборку")));

    m_runningBadge->setText(trLabel(tr("Running"), "Running", QStringLiteral("Запущена")));
    m_updateBadge->setText(trLabel(tr("Update"), "Update", QStringLiteral("Обновление")));
    m_crashedBadge->setText(trLabel(tr("Issue"), "Issue", QStringLiteral("Ошибка")));

    m_iconButton->setToolTip(trLabel(tr("Change Icon"), "Change Icon", QStringLiteral("Сменить иконку")));
    m_renameButton->setToolTip(trLabel(tr("Rename Instance"), "Rename Instance", QStringLiteral("Переименовать")));

    m_updatePackButton->setText(trLabel(tr("Update Modpack"), "Update Modpack", QStringLiteral("Обновить модпак")));
    m_updatePackButton->setToolTip(
        trLabel(tr("Check and install the latest modpack version"), "Check and install the latest modpack version",
                QStringLiteral("Проверить и установить последнюю версию модпака")));
    m_editButton->setText(trLabel(tr("Edit & Mods"), "Edit & Mods", QStringLiteral("Моды и настройки")));
    m_folderButton->setText(trLabel(tr("Folder"), "Folder", QStringLiteral("Папка")));
    m_groupButton->setText(trLabel(tr("Group"), "Group", QStringLiteral("Группа")));
    m_copyButton->setText(trLabel(tr("Copy"), "Copy", QStringLiteral("Копировать")));
    m_exportButton->setText(trLabel(tr("Export"), "Export", QStringLiteral("Экспорт")));
    m_shortcutButton->setText(trLabel(tr("Shortcut"), "Shortcut", QStringLiteral("Ярлык")));
    m_shareLogButton->setText(trLabel(tr("Share Log"), "Share Log", QStringLiteral("Отправить лог")));
    m_shareLogButton->setToolTip(
        trLabel(tr("Upload latest log to mclo.gs"), "Upload latest log to mclo.gs", QStringLiteral("Загрузить последний лог на mclo.gs")));
    m_deleteButton->setText(trLabel(tr("Delete Instance"), "Delete Instance", QStringLiteral("Удалить сборку")));

    if (m_instance) {
        updateMetadata(m_instance, false);
    }
    updatePlayButtonState();
}
