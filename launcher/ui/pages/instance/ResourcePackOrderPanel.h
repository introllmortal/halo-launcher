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

#include "minecraft/mod/ResourcePackOrderModel.h"

class BaseInstance;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class ResourcePackFolderModel;

/**
 * Panel on ResourcePackPage for editing the active resource pack load order
 * in options.txt (`resourcePacks`) without launching Minecraft.
 */
class ResourcePackOrderPanel : public QWidget {
    Q_OBJECT

   public:
    explicit ResourcePackOrderPanel(BaseInstance* instance,
                                    QWidget* parent = nullptr,
                                    ResourcePackFolderModel* folderModel = nullptr);

    void setFolderModel(ResourcePackFolderModel* folderModel);
    bool hasUnsavedChanges() const { return m_dirty; }
    void refreshRunningState();
    void reload();

   private slots:
    void moveSelectedUp();
    void moveSelectedDown();
    void toggleSelectedEnabled();
    void onRowsMoved();
    void saveOrder();
    void scheduleFolderRefresh();

   private:
    int modelIndexForId(const QString& id) const;
    int selectedModelIndex() const;
    QHash<QString, QString> buildDisplayNames() const;
    void rebuildList(const QString& selectId = {});
    void updateControlsState();

    BaseInstance* m_instance = nullptr;
    QPointer<ResourcePackFolderModel> m_folderModel;
    ResourcePackOrderModel m_orderModel;
    QString m_loadError;
    bool m_dirty = false;
    bool m_rebuilding = false;
    bool m_refreshQueued = false;

    QLabel* m_statusLabel = nullptr;
    QCheckBox* m_showSystemCheck = nullptr;
    QListWidget* m_listWidget = nullptr;
    QPushButton* m_upBtn = nullptr;
    QPushButton* m_downBtn = nullptr;
    QPushButton* m_toggleBtn = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_reloadBtn = nullptr;
};
