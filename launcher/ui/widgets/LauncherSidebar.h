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

#include <QWidget>

#include "minecraft/auth/MinecraftAccount.h"

class QAction;
class QLabel;
class QMenu;
class QToolButton;

class LauncherSidebar : public QWidget {
    Q_OBJECT

   public:
    struct Actions {
        QAction* addInstance = nullptr;
        QAction* quickLaunch = nullptr;
        QAction* settings = nullptr;
        QAction* checkUpdate = nullptr;
        QAction* helpAction = nullptr;
        QAction* closeWindow = nullptr;
        QAction* manageAccounts = nullptr;
        QAction* catToggle = nullptr;
        QMenu* foldersMenu = nullptr;
        QMenu* helpMenu = nullptr;
        QMenu* accountsMenu = nullptr;
    };

    explicit LauncherSidebar(const Actions& actions, QWidget* parent = nullptr);
    ~LauncherSidebar() override = default;

    void updateAccount(const MinecraftAccountPtr& account);
    void updateQuickLaunch(const QString& instanceName, bool canLaunch);
    void retranslateUi();

    QToolButton* helpButton() const { return m_helpButton; }
    QToolButton* accountButton() const { return m_accountButton; }

   private:
    QToolButton* makeNavButton(const QString& iconName, const QString& text, QWidget* parent = nullptr);
    void bindActionVisibility(QToolButton* button, QAction* action);

    QLabel* m_titleLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_librarySectionLabel = nullptr;
    QLabel* m_systemSectionLabel = nullptr;

    QToolButton* m_instancesButton = nullptr;
    QToolButton* m_addInstanceButton = nullptr;
    QToolButton* m_quickLaunchButton = nullptr;
    QString m_quickLaunchName;
    bool m_canQuickLaunch = false;
    QToolButton* m_foldersButton = nullptr;
    QToolButton* m_updateButton = nullptr;
    QToolButton* m_settingsButton = nullptr;
    QToolButton* m_helpButton = nullptr;
    QToolButton* m_catButton = nullptr;
    QToolButton* m_closeButton = nullptr;
    QToolButton* m_accountButton = nullptr;

    MinecraftAccountPtr m_currentAccount;
};
