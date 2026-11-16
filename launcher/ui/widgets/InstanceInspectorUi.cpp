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
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "BaseInstance.h"
#include "ui/widgets/UiAnimations.h"

namespace {
QPushButton* makePanelPushButton(const QString& iconName, const QString& objectName, QAction* action, QWidget* parent)
{
    auto* btn = new QPushButton(QIcon::fromTheme(iconName), QString(), parent);
    btn->setObjectName(objectName);
    btn->setIconSize(QSize(14, 14));
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    if (action) {
        QObject::connect(btn, &QPushButton::clicked, action, &QAction::trigger);
    }
    return btn;
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

    m_sessionTimer = new QTimer(this);
    m_sessionTimer->setInterval(1000);
    connect(m_sessionTimer, &QTimer::timeout, this, &InstanceInspector::updateSessionMetrics);

    buildEmptyPage();
    buildDetailsPage();

    retranslateUi();
    setInstance(nullptr, QIcon());
    m_initialized = true;
}

void InstanceInspector::buildEmptyPage()
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

void InstanceInspector::buildDetailsPage()
{
    auto* detailsPage = new QWidget(this);
    auto* layout = new QVBoxLayout(detailsPage);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(9);

    // Header: icon + title + rename + status badges
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

        m_pinButton = new QToolButton(detailsPage);
        m_pinButton->setObjectName(QStringLiteral("inspectorPinButton"));
        m_pinButton->setIcon(QIcon::fromTheme(QStringLiteral("star")));
        m_pinButton->setIconSize(QSize(14, 14));
        m_pinButton->setFixedSize(28, 28);
        m_pinButton->setCheckable(true);
        m_pinButton->setCursor(Qt::PointingHandCursor);
        m_pinButton->setFocusPolicy(Qt::NoFocus);
        connect(m_pinButton, &QToolButton::clicked, this, [this](bool checked) {
            if (m_instance) {
                m_instance->setPinned(checked);
            }
            updatePinButtonState();
        });
        titleTop->addWidget(m_pinButton, 0, Qt::AlignTop);

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

    // Active session card (live timer, RAM readout, and quick log open)
    {
        m_sessionCard = new QFrame(detailsPage);
        m_sessionCard->setObjectName(QStringLiteral("inspectorSessionCard"));
        auto* sessionLayout = new QVBoxLayout(m_sessionCard);
        sessionLayout->setContentsMargins(12, 9, 12, 9);
        sessionLayout->setSpacing(6);

        auto* statsRow = new QHBoxLayout();
        statsRow->setSpacing(8);
        m_sessionDurationLabel = new QLabel(m_sessionCard);
        m_sessionDurationLabel->setObjectName(QStringLiteral("inspectorSessionLabel"));
        m_sessionRamLabel = new QLabel(m_sessionCard);
        m_sessionRamLabel->setObjectName(QStringLiteral("inspectorSessionLabel"));
        m_sessionRamLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statsRow->addWidget(m_sessionDurationLabel, 1);
        statsRow->addWidget(m_sessionRamLabel, 0);
        sessionLayout->addLayout(statsRow);

        m_sessionLogButton = makePanelPushButton(QStringLiteral("log"), QStringLiteral("inspectorSessionLogButton"), nullptr, m_sessionCard);
        connect(m_sessionLogButton, &QPushButton::clicked, this, &InstanceInspector::openLogRequested);
        sessionLayout->addWidget(m_sessionLogButton);

        m_sessionCard->hide();
        layout->addWidget(m_sessionCard);
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
        UiAnimations::installButtonHoverGlow(m_playButton, 8.0, 140);
        layout->addWidget(m_playButton);
    }

    // One-step Modpack Update & management buttons
    m_updatePackButton =
        makePanelPushButton(QStringLiteral("checkupdate"), QStringLiteral("inspectorUpdateButton"), nullptr, detailsPage);
    connect(m_updatePackButton, &QPushButton::clicked, this, &InstanceInspector::updatePackRequested);
    UiAnimations::installButtonHoverGlow(m_updatePackButton, 7.0, 140);
    layout->addWidget(m_updatePackButton);

    m_rollbackButton =
        makePanelPushButton(QStringLiteral("refresh"), QStringLiteral("inspectorActionButton"), nullptr, detailsPage);
    connect(m_rollbackButton, &QPushButton::clicked, this, &InstanceInspector::rollbackSnapshotRequested);
    layout->addWidget(m_rollbackButton);

    m_checkModsButton =
        makePanelPushButton(QStringLiteral("loadermods"), QStringLiteral("inspectorActionButton"), nullptr, detailsPage);
    connect(m_checkModsButton, &QPushButton::clicked, this, &InstanceInspector::checkModUpdatesRequested);
    layout->addWidget(m_checkModsButton);

    m_editButton = makePanelPushButton(QStringLiteral("settings"), QStringLiteral("inspectorActionButton"), m_actions.editInstance,
                                       detailsPage);
    layout->addWidget(m_editButton);

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

    m_deleteButton =
        makePanelPushButton(QStringLiteral("delete"), QStringLiteral("inspectorDangerButton"), m_actions.deleteInstance, detailsPage);
    layout->addWidget(m_deleteButton);

    m_stack->addWidget(detailsPage);
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
