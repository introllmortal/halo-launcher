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

#include <QPointer>
#include <QWidget>

class BaseInstance;
class QAction;
class QFrame;
class QIcon;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QToolButton;

class InstanceInspector : public QWidget {
    Q_OBJECT

   public:
    struct Actions {
        QAction* addInstance = nullptr;
        QAction* launchInstance = nullptr;
        QAction* killInstance = nullptr;
        QAction* editInstance = nullptr;
        QAction* renameInstance = nullptr;
        QAction* changeInstIcon = nullptr;
        QAction* changeInstGroup = nullptr;
        QAction* viewFolder = nullptr;
        QAction* exportInstance = nullptr;
        QAction* copyInstance = nullptr;
        QAction* createShortcut = nullptr;
        QAction* deleteInstance = nullptr;
    };

    explicit InstanceInspector(const Actions& actions, QWidget* parent = nullptr);
    ~InstanceInspector() override = default;

    void setInstance(BaseInstance* instance, const QIcon& icon);
    void retranslateUi();

   signals:
    void shareLogRequested();
    void openLogRequested();
    void updatePackRequested();
    void rollbackSnapshotRequested();
    void checkModUpdatesRequested();

   protected:
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;

   private:
    void buildEmptyPage();
    void buildDetailsPage();
    QToolButton* makeGridButton(const QString& iconName, QAction* action, QWidget* parent);
    void updatePlayButtonState();
    void updatePinButtonState();
    void updateMetadata(BaseInstance* instance, bool animateVisibility);
    void updateSessionMetrics();
    void syncSessionCardVisibility(bool animate);

    Actions m_actions;
    QPointer<BaseInstance> m_instance;
    bool m_initialized = false;

    QStackedWidget* m_stack = nullptr;

    // Empty state
    QLabel* m_emptyTitleLabel = nullptr;
    QLabel* m_emptyHintLabel = nullptr;
    QPushButton* m_emptyAddButton = nullptr;

    // Instance details state
    QToolButton* m_iconButton = nullptr;
    QLabel* m_titleLabel = nullptr;
    QToolButton* m_pinButton = nullptr;
    QToolButton* m_renameButton = nullptr;

    QLabel* m_runningBadge = nullptr;
    QLabel* m_updateBadge = nullptr;
    QLabel* m_crashedBadge = nullptr;

    QFrame* m_metaCard = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_packLabel = nullptr;
    QLabel* m_playtimeLabel = nullptr;

    QFrame* m_sessionCard = nullptr;
    QLabel* m_sessionDurationLabel = nullptr;
    QLabel* m_sessionRamLabel = nullptr;
    QPushButton* m_sessionLogButton = nullptr;
    QTimer* m_sessionTimer = nullptr;

    QToolButton* m_playButton = nullptr;
    QPushButton* m_updatePackButton = nullptr;
    QPushButton* m_rollbackButton = nullptr;
    QPushButton* m_checkModsButton = nullptr;
    QPushButton* m_editButton = nullptr;
    QToolButton* m_folderButton = nullptr;
    QToolButton* m_groupButton = nullptr;
    QToolButton* m_copyButton = nullptr;
    QToolButton* m_exportButton = nullptr;
    QToolButton* m_shortcutButton = nullptr;
    QToolButton* m_shareLogButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
};
