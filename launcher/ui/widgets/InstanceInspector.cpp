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
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "BaseInstance.h"
#include "MMCTime.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "translations/TranslationsModel.h"

namespace {
QString uiLabel(const QString& translated, const char* englishLiteral, const QString& ruFallback)
{
    return TranslationsModel::trFallback(APPLICATION ? APPLICATION->translations() : nullptr, translated, englishLiteral, ruFallback);
}

QString detectLoaderSummary(MinecraftInstance* mcInst)
{
    if (!mcInst || !mcInst->getPackProfile()) {
        return {};
    }
    auto* profile = mcInst->getPackProfile();
    QString mc = profile->getComponentVersion(QStringLiteral("net.minecraft"));
    QString loader;
    if (!profile->getComponentVersion(QStringLiteral("net.neoforge")).isEmpty()) {
        loader = QStringLiteral("NeoForge");
    } else if (!profile->getComponentVersion(QStringLiteral("net.minecraftforge")).isEmpty()) {
        loader = QStringLiteral("Forge");
    } else if (!profile->getComponentVersion(QStringLiteral("net.fabricmc.fabric-loader")).isEmpty()) {
        loader = QStringLiteral("Fabric");
    } else if (!profile->getComponentVersion(QStringLiteral("org.quiltmc.quilt-loader")).isEmpty()) {
        loader = QStringLiteral("Quilt");
    }

    if (!mc.isEmpty() && !loader.isEmpty()) {
        return QStringLiteral("Minecraft %1 • %2").arg(mc, loader);
    }
    if (!mc.isEmpty()) {
        return QStringLiteral("Minecraft %1").arg(mc);
    }
    return {};
}
}  // namespace

InstanceInspector::InstanceInspector(const Actions& actions, QWidget* parent) : QWidget(parent), m_actions(actions)
{
    setObjectName(QStringLiteral("instanceInspector"));
    setFixedWidth(274);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    m_stack = new QStackedWidget(this);
    outer->addWidget(m_stack);

    // Page 0: empty state
    {
        auto* emptyPage = new QWidget(this);
        auto* layout = new QVBoxLayout(emptyPage);
        layout->setContentsMargins(20, 32, 20, 24);
        layout->setSpacing(10);
        layout->addStretch(1);

        auto* iconLabel = new QLabel(emptyPage);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("launcher")).pixmap(48, 48));
        layout->addWidget(iconLabel);

        m_emptyTitleLabel = new QLabel(emptyPage);
        m_emptyTitleLabel->setObjectName(QStringLiteral("inspectorTitleLabel"));
        m_emptyTitleLabel->setAlignment(Qt::AlignCenter);
        m_emptyTitleLabel->setWordWrap(true);
        layout->addWidget(m_emptyTitleLabel);

        m_emptyHintLabel = new QLabel(emptyPage);
        m_emptyHintLabel->setObjectName(QStringLiteral("inspectorMetaLabel"));
        m_emptyHintLabel->setAlignment(Qt::AlignCenter);
        m_emptyHintLabel->setWordWrap(true);
        layout->addWidget(m_emptyHintLabel);

        layout->addSpacing(8);

        m_emptyAddButton = new QPushButton(QIcon::fromTheme(QStringLiteral("new")), QString(), emptyPage);
        m_emptyAddButton->setCursor(Qt::PointingHandCursor);
        if (m_actions.addInstance) {
            connect(m_emptyAddButton, &QPushButton::clicked, m_actions.addInstance, &QAction::trigger);
        }
        layout->addWidget(m_emptyAddButton);

        layout->addStretch(2);
        m_stack->addWidget(emptyPage);
    }

    // Page 1: selected instance inspector
    {
        auto* detailsPage = new QWidget(this);
        auto* layout = new QVBoxLayout(detailsPage);
        layout->setContentsMargins(14, 14, 14, 14);
        layout->setSpacing(9);

        // Header: icon + title + rename
        {
            auto* headerRow = new QHBoxLayout();
            headerRow->setSpacing(10);

            m_iconButton = new QToolButton(detailsPage);
            m_iconButton->setObjectName(QStringLiteral("inspectorIconButton"));
            m_iconButton->setFixedSize(56, 56);
            m_iconButton->setIconSize(QSize(42, 42));
            m_iconButton->setCursor(Qt::PointingHandCursor);
            m_iconButton->setFocusPolicy(Qt::NoFocus);
            if (m_actions.changeInstIcon) {
                connect(m_iconButton, &QToolButton::clicked, m_actions.changeInstIcon, &QAction::trigger);
            }
            headerRow->addWidget(m_iconButton, 0, Qt::AlignTop);

            auto* titleCol = new QVBoxLayout();
            titleCol->setSpacing(4);
            titleCol->setContentsMargins(0, 2, 0, 0);

            auto* titleTop = new QHBoxLayout();
            titleTop->setSpacing(4);

            m_titleLabel = new QLabel(detailsPage);
            m_titleLabel->setObjectName(QStringLiteral("inspectorTitleLabel"));
            m_titleLabel->setWordWrap(true);
            titleTop->addWidget(m_titleLabel, 1);

            m_renameButton = new QToolButton(detailsPage);
            m_renameButton->setObjectName(QStringLiteral("inspectorActionButton"));
            m_renameButton->setIcon(QIcon::fromTheme(QStringLiteral("rename")));
            m_renameButton->setIconSize(QSize(14, 14));
            m_renameButton->setFixedSize(28, 28);
            m_renameButton->setCursor(Qt::PointingHandCursor);
            m_renameButton->setFocusPolicy(Qt::NoFocus);
            if (m_actions.renameInstance) {
                connect(m_renameButton, &QToolButton::clicked, m_actions.renameInstance, &QAction::trigger);
            }
            titleTop->addWidget(m_renameButton, 0, Qt::AlignTop);
            titleCol->addLayout(titleTop);

            // Status badges
            auto* badgesRow = new QHBoxLayout();
            badgesRow->setSpacing(6);
            m_runningBadge = new QLabel(detailsPage);
            m_runningBadge->setObjectName(QStringLiteral("inspectorBadgeRunning"));
            m_updateBadge = new QLabel(detailsPage);
            m_updateBadge->setObjectName(QStringLiteral("inspectorBadgeUpdate"));
            m_crashedBadge = new QLabel(detailsPage);
            m_crashedBadge->setObjectName(QStringLiteral("inspectorBadgeError"));
            badgesRow->addWidget(m_runningBadge);
            badgesRow->addWidget(m_updateBadge);
            badgesRow->addWidget(m_crashedBadge);
            badgesRow->addStretch(1);
            titleCol->addLayout(badgesRow);

            headerRow->addLayout(titleCol, 1);
            layout->addLayout(headerRow);
        }

        // Metadata card
        {
            m_metaCard = new QFrame(detailsPage);
            m_metaCard->setObjectName(QStringLiteral("inspectorMetaCard"));
            auto* metaLayout = new QVBoxLayout(m_metaCard);
            metaLayout->setContentsMargins(12, 10, 12, 10);
            metaLayout->setSpacing(4);

            m_versionLabel = new QLabel(m_metaCard);
            m_versionLabel->setObjectName(QStringLiteral("inspectorMetaLabel"));
            m_versionLabel->setWordWrap(true);

            m_packLabel = new QLabel(m_metaCard);
            m_packLabel->setObjectName(QStringLiteral("inspectorMetaLabel"));
            m_packLabel->setWordWrap(true);

            m_playtimeLabel = new QLabel(m_metaCard);
            m_playtimeLabel->setObjectName(QStringLiteral("inspectorMetaLabel"));
            m_playtimeLabel->setWordWrap(true);

            metaLayout->addWidget(m_versionLabel);
            metaLayout->addWidget(m_packLabel);
            metaLayout->addWidget(m_playtimeLabel);
            layout->addWidget(m_metaCard);
        }

        // Play / Stop primary button
        {
            m_playButton = new QToolButton(detailsPage);
            m_playButton->setObjectName(QStringLiteral("playPrimaryButton"));
            m_playButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            m_playButton->setIconSize(QSize(15, 15));
            m_playButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            m_playButton->setCursor(Qt::PointingHandCursor);
            m_playButton->setFocusPolicy(Qt::NoFocus);
            connect(m_playButton, &QToolButton::clicked, this, [this]() {
                if (m_instance && m_instance->isRunning()) {
                    if (m_actions.killInstance) {
                        m_actions.killInstance->trigger();
                    }
                } else if (m_actions.launchInstance) {
                    m_actions.launchInstance->trigger();
                }
            });
            layout->addWidget(m_playButton);
        }

        // Primary management button (Edit instance / Mods & settings)
        {
            m_editButton = new QPushButton(QIcon::fromTheme(QStringLiteral("settings")), QString(), detailsPage);
            m_editButton->setObjectName(QStringLiteral("inspectorActionButton"));
            m_editButton->setIconSize(QSize(14, 14));
            m_editButton->setCursor(Qt::PointingHandCursor);
            m_editButton->setFocusPolicy(Qt::NoFocus);
            if (m_actions.editInstance) {
                connect(m_editButton, &QPushButton::clicked, m_actions.editInstance, &QAction::trigger);
            }
            layout->addWidget(m_editButton);
        }

        // 2-column compact action grid
        {
            auto* grid = new QGridLayout();
            grid->setSpacing(6);
            grid->setContentsMargins(0, 2, 0, 0);

            m_folderButton = makeGridButton(QStringLiteral("viewfolder"), m_actions.viewFolder, detailsPage);
            m_groupButton = makeGridButton(QStringLiteral("tag"), m_actions.changeInstGroup, detailsPage);
            m_copyButton = makeGridButton(QStringLiteral("copy"), m_actions.copyInstance, detailsPage);

            m_exportButton = makeGridButton(QStringLiteral("export"), nullptr, detailsPage);
            if (m_actions.exportInstance && m_actions.exportInstance->menu()) {
                m_exportButton->setMenu(m_actions.exportInstance->menu());
                m_exportButton->setPopupMode(QToolButton::InstantPopup);
            } else if (m_actions.exportInstance) {
                connect(m_exportButton, &QToolButton::clicked, m_actions.exportInstance, &QAction::trigger);
            }

            m_shortcutButton = makeGridButton(QStringLiteral("shortcut"), m_actions.createShortcut, detailsPage);

            m_shareLogButton = makeGridButton(QStringLiteral("log"), nullptr, detailsPage);
            connect(m_shareLogButton, &QToolButton::clicked, this, &InstanceInspector::shareLogRequested);

            grid->addWidget(m_folderButton, 0, 0);
            grid->addWidget(m_groupButton, 0, 1);
            grid->addWidget(m_copyButton, 1, 0);
            grid->addWidget(m_exportButton, 1, 1);
            grid->addWidget(m_shortcutButton, 2, 0);
            grid->addWidget(m_shareLogButton, 2, 1);

            layout->addLayout(grid);
        }

        layout->addStretch(1);

        // Destructive action
        {
            m_deleteButton = new QPushButton(QIcon::fromTheme(QStringLiteral("delete")), QString(), detailsPage);
            m_deleteButton->setObjectName(QStringLiteral("inspectorDangerButton"));
            m_deleteButton->setIconSize(QSize(14, 14));
            m_deleteButton->setCursor(Qt::PointingHandCursor);
            m_deleteButton->setFocusPolicy(Qt::NoFocus);
            if (m_actions.deleteInstance) {
                connect(m_deleteButton, &QPushButton::clicked, m_actions.deleteInstance, &QAction::trigger);
            }
            layout->addWidget(m_deleteButton);
        }

        m_stack->addWidget(detailsPage);
    }

    retranslateUi();
    setInstance(nullptr, QIcon());
}

QToolButton* InstanceInspector::makeGridButton(const QString& iconName, QAction* action, QWidget* parent)
{
    auto* btn = new QToolButton(parent);
    btn->setObjectName(QStringLiteral("inspectorActionButton"));
    btn->setIcon(QIcon::fromTheme(iconName));
    btn->setIconSize(QSize(14, 14));
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    if (action) {
        connect(btn, &QToolButton::clicked, action, &QAction::trigger);
        auto sync = [btn, action]() { btn->setEnabled(action->isEnabled()); };
        sync();
        connect(action, &QAction::changed, btn, sync);
    }
    return btn;
}

void InstanceInspector::setInstance(BaseInstance* instance, const QIcon& icon)
{
    m_instance = instance;
    if (!instance) {
        m_stack->setCurrentIndex(0);
        return;
    }

    m_stack->setCurrentIndex(1);
    m_iconButton->setIcon(icon);
    m_titleLabel->setText(instance->name());

    m_runningBadge->setVisible(instance->isRunning());
    m_updateBadge->setVisible(instance->hasUpdateAvailable());
    m_crashedBadge->setVisible(!instance->isRunning() && (instance->hasCrashed() || instance->hasVersionBroken()));

    updateMetadata(instance);
    updatePlayButtonState();
}

void InstanceInspector::updateMetadata(BaseInstance* instance)
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
        m_packLabel->setText(uiLabel(tr("Modpack: %1"), "Modpack: %1", QStringLiteral("Сборка: %1")).arg(packText));
        m_packLabel->setVisible(!packText.isEmpty());
    } else {
        m_packLabel->setVisible(false);
    }

    QStringList timeParts;
    if (instance->totalTimePlayed() > 0) {
        timeParts.append(uiLabel(tr("Played %1"), "Played %1", QStringLiteral("Наиграно %1"))
                             .arg(Time::prettifyDuration(instance->totalTimePlayed(),
                                                         APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())));
    }
    if (instance->lastLaunch() > 0) {
        QDateTime last = QDateTime::fromMSecsSinceEpoch(instance->lastLaunch());
        timeParts.append(uiLabel(tr("Last launch: %1"), "Last launch: %1", QStringLiteral("Запуск: %1"))
                             .arg(QLocale().toString(last.date(), QLocale::ShortFormat)));
    }
    if (timeParts.isEmpty()) {
        m_playtimeLabel->setText(uiLabel(tr("Ready to launch"), "Ready to launch", QStringLiteral("Готово к запуску")));
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
        m_playButton->setText(uiLabel(tr("Stop"), "Stop", QStringLiteral("Остановить")));
        m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("status-bad")));
        m_playButton->setMenu(nullptr);
        m_playButton->setPopupMode(QToolButton::DelayedPopup);
        m_playButton->setEnabled(true);
    } else {
        m_playButton->setText(uiLabel(tr("Play"), "Play", QStringLiteral("Играть")));
        m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("launch")));
        QMenu* launchMenu = m_actions.launchInstance ? m_actions.launchInstance->menu() : nullptr;
        m_playButton->setMenu(launchMenu && !launchMenu->isEmpty() ? launchMenu : nullptr);
        m_playButton->setPopupMode((launchMenu && !launchMenu->isEmpty()) ? QToolButton::MenuButtonPopup : QToolButton::DelayedPopup);
        m_playButton->setEnabled(m_instance && m_instance->canLaunch());
    }
}

void InstanceInspector::retranslateUi()
{
    m_emptyTitleLabel->setText(uiLabel(tr("No instance selected"), "No instance selected", QStringLiteral("Сборка не выбрана")));
    m_emptyHintLabel->setText(uiLabel(tr("Select an instance from the grid or create a new one to play."),
                                      "Select an instance from the grid or create a new one to play.",
                                      QStringLiteral("Выберите сборку в списке или создайте новую для запуска.")));
    m_emptyAddButton->setText(uiLabel(tr("Add Instance"), "Add Instance", QStringLiteral("Добавить сборку")));

    m_runningBadge->setText(uiLabel(tr("Running"), "Running", QStringLiteral("Запущена")));
    m_updateBadge->setText(uiLabel(tr("Update"), "Update", QStringLiteral("Обновление")));
    m_crashedBadge->setText(uiLabel(tr("Issue"), "Issue", QStringLiteral("Ошибка")));

    m_iconButton->setToolTip(uiLabel(tr("Change Icon"), "Change Icon", QStringLiteral("Сменить иконку")));
    m_renameButton->setToolTip(uiLabel(tr("Rename Instance"), "Rename Instance", QStringLiteral("Переименовать")));

    m_editButton->setText(uiLabel(tr("Edit & Mods"), "Edit & Mods", QStringLiteral("Моды и настройки")));
    m_folderButton->setText(uiLabel(tr("Folder"), "Folder", QStringLiteral("Папка")));
    m_groupButton->setText(uiLabel(tr("Group"), "Group", QStringLiteral("Группа")));
    m_copyButton->setText(uiLabel(tr("Copy"), "Copy", QStringLiteral("Копировать")));
    m_exportButton->setText(uiLabel(tr("Export"), "Export", QStringLiteral("Экспорт")));
    m_shortcutButton->setText(uiLabel(tr("Shortcut"), "Shortcut", QStringLiteral("Ярлык")));
    m_shareLogButton->setText(uiLabel(tr("Share Log"), "Share Log", QStringLiteral("Отправить лог")));
    m_shareLogButton->setToolTip(
        uiLabel(tr("Upload latest log to mclo.gs"), "Upload latest log to mclo.gs", QStringLiteral("Загрузить последний лог на mclo.gs")));
    m_deleteButton->setText(uiLabel(tr("Delete Instance"), "Delete Instance", QStringLiteral("Удалить сборку")));

    if (m_instance) {
        updateMetadata(m_instance);
    }
    updatePlayButtonState();
}
