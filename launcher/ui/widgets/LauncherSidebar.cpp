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
#include "LauncherSidebar.h"

#include <QAction>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

#include "Application.h"
#include "BuildConfig.h"
#include "translations/TranslationsModel.h"

namespace {
QString uiLabel(const QString& translated, const char* englishLiteral, const QString& ruFallback)
{
    return TranslationsModel::trFallback(APPLICATION ? APPLICATION->translations() : nullptr, translated, englishLiteral, ruFallback);
}
}  // namespace

LauncherSidebar::LauncherSidebar(const Actions& actions, QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("launcherSidebar"));
    setFixedWidth(204);
    setAttribute(Qt::WA_StyledBackground, true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(10, 14, 10, 12);
    root->setSpacing(4);

    // Brand header
    {
        auto* brandRow = new QHBoxLayout();
        brandRow->setContentsMargins(6, 2, 6, 10);
        brandRow->setSpacing(10);

        auto* logoLabel = new QLabel(this);
        logoLabel->setFixedSize(30, 30);
        logoLabel->setPixmap(APPLICATION->logo().pixmap(30, 30));
        logoLabel->setScaledContents(true);
        brandRow->addWidget(logoLabel);

        auto* titleCol = new QVBoxLayout();
        titleCol->setContentsMargins(0, 0, 0, 0);
        titleCol->setSpacing(1);

        m_titleLabel = new QLabel(BuildConfig.LAUNCHER_DISPLAYNAME, this);
        m_titleLabel->setObjectName(QStringLiteral("sidebarTitleLabel"));
        m_versionLabel = new QLabel(QStringLiteral("v%1").arg(BuildConfig.printableVersionString()), this);
        m_versionLabel->setObjectName(QStringLiteral("sidebarVersionLabel"));

        titleCol->addWidget(m_titleLabel);
        titleCol->addWidget(m_versionLabel);
        brandRow->addLayout(titleCol, 1);

        root->addLayout(brandRow);
    }

    m_librarySectionLabel = new QLabel(this);
    m_librarySectionLabel->setObjectName(QStringLiteral("sidebarSectionLabel"));
    root->addWidget(m_librarySectionLabel);

    m_instancesButton = makeNavButton(QStringLiteral("launcher"), QString(), this);
    m_instancesButton->setProperty("active", true);
    root->addWidget(m_instancesButton);

    m_addInstanceButton = makeNavButton(QStringLiteral("new"), QString(), this);
    if (actions.addInstance) {
        connect(m_addInstanceButton, &QToolButton::clicked, actions.addInstance, &QAction::trigger);
        bindActionVisibility(m_addInstanceButton, actions.addInstance);
    }
    root->addWidget(m_addInstanceButton);

    root->addSpacing(8);

    m_systemSectionLabel = new QLabel(this);
    m_systemSectionLabel->setObjectName(QStringLiteral("sidebarSectionLabel"));
    root->addWidget(m_systemSectionLabel);

    m_foldersButton = makeNavButton(QStringLiteral("viewfolder"), QString(), this);
    if (actions.foldersMenu) {
        m_foldersButton->setMenu(actions.foldersMenu);
        m_foldersButton->setPopupMode(QToolButton::InstantPopup);
    }
    root->addWidget(m_foldersButton);

    m_updateButton = makeNavButton(QStringLiteral("checkupdate"), QString(), this);
    if (actions.checkUpdate) {
        connect(m_updateButton, &QToolButton::clicked, actions.checkUpdate, &QAction::trigger);
        bindActionVisibility(m_updateButton, actions.checkUpdate);
    }
    root->addWidget(m_updateButton);

    m_settingsButton = makeNavButton(QStringLiteral("settings"), QString(), this);
    if (actions.settings) {
        connect(m_settingsButton, &QToolButton::clicked, actions.settings, &QAction::trigger);
        bindActionVisibility(m_settingsButton, actions.settings);
    }
    root->addWidget(m_settingsButton);

    m_helpButton = makeNavButton(QStringLiteral("help"), QString(), this);
    if (actions.helpMenu) {
        m_helpButton->setMenu(actions.helpMenu);
        m_helpButton->setPopupMode(QToolButton::InstantPopup);
    }
    if (actions.helpAction) {
        bindActionVisibility(m_helpButton, actions.helpAction);
    }
    root->addWidget(m_helpButton);

    root->addStretch(1);

    if (actions.closeWindow && qgetenv("XDG_CURRENT_DESKTOP") == "gamescope") {
        m_closeButton = makeNavButton(QStringLiteral("delete"), QString(), this);
        connect(m_closeButton, &QToolButton::clicked, actions.closeWindow, &QAction::trigger);
        bindActionVisibility(m_closeButton, actions.closeWindow);
        root->addWidget(m_closeButton);
    }

    m_catButton = makeNavButton(QStringLiteral("cat"), QString(), this);
    m_catButton->setCheckable(true);
    if (actions.catToggle) {
        m_catButton->setChecked(actions.catToggle->isChecked());
        connect(actions.catToggle, &QAction::toggled, m_catButton, &QToolButton::setChecked);
        connect(m_catButton, &QToolButton::clicked, actions.catToggle, &QAction::trigger);
        bindActionVisibility(m_catButton, actions.catToggle);
    }
    root->addWidget(m_catButton);

    root->addSpacing(4);

    m_accountButton = new QToolButton(this);
    m_accountButton->setObjectName(QStringLiteral("sidebarAccountButton"));
    m_accountButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_accountButton->setIconSize(QSize(22, 22));
    m_accountButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_accountButton->setCursor(Qt::PointingHandCursor);
    m_accountButton->setFocusPolicy(Qt::NoFocus);
    m_accountButton->setPopupMode(QToolButton::InstantPopup);
    if (actions.accountsMenu) {
        m_accountButton->setMenu(actions.accountsMenu);
    } else if (actions.manageAccounts) {
        connect(m_accountButton, &QToolButton::clicked, actions.manageAccounts, &QAction::trigger);
    }
    root->addWidget(m_accountButton);

    retranslateUi();
}

QToolButton* LauncherSidebar::makeNavButton(const QString& iconName, const QString& text, QWidget* parent)
{
    auto* btn = new QToolButton(parent);
    btn->setObjectName(QStringLiteral("sidebarNavButton"));
    btn->setIcon(QIcon::fromTheme(iconName));
    btn->setIconSize(QSize(15, 15));
    btn->setText(text);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    return btn;
}

void LauncherSidebar::bindActionVisibility(QToolButton* button, QAction* action)
{
    auto sync = [button, action]() {
        button->setVisible(action->isVisible());
        button->setEnabled(action->isEnabled());
        if (!action->toolTip().isEmpty()) {
            button->setToolTip(action->toolTip());
        }
    };
    sync();
    connect(action, &QAction::changed, button, sync);
}

void LauncherSidebar::updateAccount(const MinecraftAccountPtr& account)
{
    m_currentAccount = account;
    if (account && !account->profileName().isEmpty()) {
        QString label = account->displayName();
        if (account->isInUse()) {
            label = uiLabel(tr("%1 (in use)"), "%1 (in use)", QStringLiteral("%1 (используется)")).arg(label);
        }
        m_accountButton->setText(label);
        m_accountButton->setToolTip(account->nameWithType());
        auto face = account->getFace(22, 22);
        m_accountButton->setIcon(face.isNull() ? QIcon::fromTheme(QStringLiteral("noaccount")) : QIcon(face));
    } else {
        m_accountButton->setText(uiLabel(tr("Accounts"), "Accounts", QStringLiteral("Аккаунты")));
        m_accountButton->setToolTip(uiLabel(tr("Manage Minecraft accounts and skins"),
                                            "Manage Minecraft accounts and skins",
                                            QStringLiteral("Управление аккаунтами Minecraft и скинами")));
        m_accountButton->setIcon(QIcon::fromTheme(QStringLiteral("noaccount")));
    }
}

void LauncherSidebar::retranslateUi()
{
    m_librarySectionLabel->setText(uiLabel(tr("LIBRARY"), "LIBRARY", QStringLiteral("БИБЛИОТЕКА")));
    m_systemSectionLabel->setText(uiLabel(tr("LAUNCHER"), "LAUNCHER", QStringLiteral("ЛАУНЧЕР")));
    m_instancesButton->setText(uiLabel(tr("Instances"), "Instances", QStringLiteral("Сборки")));
    m_addInstanceButton->setText(uiLabel(tr("Add Instance"), "Add Instance", QStringLiteral("Добавить сборку")));
    m_foldersButton->setText(uiLabel(tr("Folders"), "Folders", QStringLiteral("Папки")));
    m_updateButton->setText(uiLabel(tr("Updates"), "Updates", QStringLiteral("Обновления")));
    m_settingsButton->setText(uiLabel(tr("Settings"), "Settings", QStringLiteral("Настройки")));
    m_helpButton->setText(uiLabel(tr("Help"), "Help", QStringLiteral("Помощь")));
    if (m_closeButton) {
        m_closeButton->setText(uiLabel(tr("Close"), "Close", QStringLiteral("Закрыть")));
    }
    m_catButton->setText(uiLabel(tr("Mascot"), "Mascot", QStringLiteral("Маскот")));
    updateAccount(m_currentAccount);
}
