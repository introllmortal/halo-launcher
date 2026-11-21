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
#include "ResourcePackOrderPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "minecraft/GameOptionsFile.h"
#include "minecraft/mod/ResourcePackFolderModel.h"

ResourcePackOrderPanel::ResourcePackOrderPanel(BaseInstance* instance,
                                               QWidget* parent,
                                               ResourcePackFolderModel* folderModel)
    : QWidget(parent), m_instance(instance)
{
    setMinimumWidth(260);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(8);

    auto* headerLayout = new QHBoxLayout();
    auto* title = new QLabel(tr("In-Game Load Order (Top = Highest Priority)"), this);
    title->setWordWrap(true);
    QFont f = title->font();
    f.setBold(true);
    title->setFont(f);
    headerLayout->addWidget(title, 1);

    m_showSystemCheck = new QCheckBox(tr("Show system layers"), this);
    m_showSystemCheck->setChecked(false);
    headerLayout->addWidget(m_showSystemCheck);
    layout->addLayout(headerLayout);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_listWidget = new QListWidget(this);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_listWidget, 1);

    auto* controlsLayout = new QHBoxLayout();
    m_upBtn = new QPushButton(tr("Up"), this);
    m_downBtn = new QPushButton(tr("Down"), this);
    m_toggleBtn = new QPushButton(tr("Enable / Disable"), this);
    m_reloadBtn = new QPushButton(tr("Reload"), this);
    m_saveBtn = new QPushButton(tr("Save Order"), this);
    controlsLayout->addWidget(m_upBtn);
    controlsLayout->addWidget(m_downBtn);
    controlsLayout->addWidget(m_toggleBtn);
    controlsLayout->addStretch(1);
    controlsLayout->addWidget(m_reloadBtn);
    controlsLayout->addWidget(m_saveBtn);
    layout->addLayout(controlsLayout);

    connect(m_upBtn, &QPushButton::clicked, this, &ResourcePackOrderPanel::moveSelectedUp);
    connect(m_downBtn, &QPushButton::clicked, this, &ResourcePackOrderPanel::moveSelectedDown);
    connect(m_toggleBtn, &QPushButton::clicked, this, &ResourcePackOrderPanel::toggleSelectedEnabled);
    connect(m_saveBtn, &QPushButton::clicked, this, &ResourcePackOrderPanel::saveOrder);
    connect(m_reloadBtn, &QPushButton::clicked, this, &ResourcePackOrderPanel::reload);
    connect(m_showSystemCheck, &QCheckBox::toggled, this, [this](bool) {
        const QString currentId =
            m_listWidget->currentItem() ? m_listWidget->currentItem()->data(Qt::UserRole).toString() : QString();
        rebuildList(currentId);
    });
    connect(m_listWidget, &QListWidget::currentRowChanged, this, [this](int) { updateControlsState(); });
    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved, this, &ResourcePackOrderPanel::onRowsMoved);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { toggleSelectedEnabled(); });

    setFolderModel(folderModel);
}

void ResourcePackOrderPanel::setFolderModel(ResourcePackFolderModel* folderModel)
{
    if (m_folderModel) {
        disconnect(m_folderModel, nullptr, this, nullptr);
    }
    m_folderModel = folderModel;
    if (m_folderModel) {
        connect(m_folderModel, &QAbstractItemModel::modelReset, this, &ResourcePackOrderPanel::scheduleFolderRefresh);
        connect(m_folderModel, &QAbstractItemModel::rowsInserted, this, &ResourcePackOrderPanel::scheduleFolderRefresh);
        connect(m_folderModel, &QAbstractItemModel::rowsRemoved, this, &ResourcePackOrderPanel::scheduleFolderRefresh);
        connect(m_folderModel, &QAbstractItemModel::dataChanged, this, &ResourcePackOrderPanel::scheduleFolderRefresh);
    }
    reload();
}

void ResourcePackOrderPanel::refreshRunningState()
{
    updateControlsState();
}

void ResourcePackOrderPanel::scheduleFolderRefresh()
{
    if (m_dirty || m_refreshQueued) {
        return;
    }
    m_refreshQueued = true;
    QTimer::singleShot(120, this, [this]() {
        m_refreshQueued = false;
        if (!m_dirty) {
            reload();
        }
    });
}

QHash<QString, QString> ResourcePackOrderPanel::buildDisplayNames() const
{
    QHash<QString, QString> names;
    if (!m_folderModel) {
        return names;
    }
    const int total = m_folderModel->rowCount();
    for (int i = 0; i < total; ++i) {
        const auto& pack = m_folderModel->at(i);
        const QString rawName = pack.fileinfo().fileName();
        if (rawName.isEmpty()) {
            continue;
        }
        const QString activeName =
            rawName.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive) ? rawName.left(rawName.size() - 9) : rawName;
        const QString friendly = pack.name().trimmed();
        if (!friendly.isEmpty()) {
            names.insert(activeName, friendly);
        }
    }
    return names;
}

int ResourcePackOrderPanel::modelIndexForId(const QString& id) const
{
    if (id.isEmpty()) {
        return -1;
    }
    const auto& entries = m_orderModel.entries();
    for (int i = 0; i < entries.size(); ++i) {
        if (entries.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

int ResourcePackOrderPanel::selectedModelIndex() const
{
    const QListWidgetItem* item = m_listWidget->currentItem();
    return item ? modelIndexForId(item->data(Qt::UserRole).toString()) : -1;
}

void ResourcePackOrderPanel::reload()
{
    const QString prevSelectedId =
        m_listWidget->currentItem() ? m_listWidget->currentItem()->data(Qt::UserRole).toString() : QString();
    m_dirty = false;
    m_loadError.clear();
    if (!m_instance || m_instance->gameRoot().isEmpty()) {
        m_listWidget->clear();
        updateControlsState();
        return;
    }

    const QString optPath = GameOptionsFile::optionsFilePath(m_instance);
    const QString rpDir = FS::PathCombine(m_instance->gameRoot(), QStringLiteral("resourcepacks"));
    QString err;
    if (!m_orderModel.load(optPath, rpDir, &err, buildDisplayNames())) {
        m_loadError = err;
    }
    rebuildList(prevSelectedId);
}

void ResourcePackOrderPanel::rebuildList(const QString& selectId)
{
    m_rebuilding = true;
    m_listWidget->clear();

    const bool includeSystem = m_showSystemCheck && m_showSystemCheck->isChecked();
    const auto& entries = m_orderModel.entries();
    int selectRow = -1;

    for (int i = 0; i < entries.size(); ++i) {
        const auto& e = entries.at(i);
        if (e.isSystem && !includeSystem) {
            continue;
        }

        QString label = e.displayName;
        if (e.isSystem) {
            label = tr("[System] %1").arg(label);
        } else if (!e.enabled) {
            label = tr("[Off] %1").arg(label);
        } else if (!e.existsOnDisk) {
            label = tr("[Missing] %1").arg(label);
        }

        auto* item = new QListWidgetItem(label, m_listWidget);
        item->setData(Qt::UserRole, e.id);
        item->setToolTip(e.isSystem ? tr("Built-in game or mod resource layer") : e.id.mid(5));
        if (!e.enabled) {
            item->setForeground(palette().placeholderText());
            item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
        }
        if (!selectId.isEmpty() && e.id == selectId) {
            selectRow = m_listWidget->count() - 1;
        }
    }

    if (selectRow >= 0 && selectRow < m_listWidget->count()) {
        m_listWidget->setCurrentRow(selectRow);
    }
    m_rebuilding = false;
    updateControlsState();
}

void ResourcePackOrderPanel::updateControlsState()
{
    const bool running = m_instance && m_instance->isRunning();
    const int row = m_listWidget->currentRow();
    const int totalVisible = m_listWidget->count();
    const int modelIdx = selectedModelIndex();
    const bool hasModelRow = modelIdx >= 0 && modelIdx < m_orderModel.count();
    const bool isSystem = hasModelRow && m_orderModel.entries().at(modelIdx).isSystem;
    const bool curEnabled = hasModelRow && m_orderModel.entries().at(modelIdx).enabled;
    const bool canMoveUp = !running && curEnabled && row > 0;
    bool hasNextMovable = false;
    if (!running && curEnabled && row >= 0 && row + 1 < totalVisible) {
        const QString nextId = m_listWidget->item(row + 1)->data(Qt::UserRole).toString();
        const int nextModelIdx = modelIndexForId(nextId);
        hasNextMovable = nextModelIdx >= 0 && m_orderModel.entries().at(nextModelIdx).enabled;
    }

    m_listWidget->setEnabled(!running);
    m_upBtn->setEnabled(canMoveUp);
    m_downBtn->setEnabled(hasNextMovable);
    m_toggleBtn->setEnabled(!running && hasModelRow && !isSystem);
    m_saveBtn->setEnabled(!running && m_dirty);

    if (running) {
        m_statusLabel->setText(tr("Game is running. Close Minecraft to edit resource pack order."));
    } else if (!m_loadError.isEmpty()) {
        m_statusLabel->setText(tr("Load error: %1").arg(m_loadError));
    } else if (m_dirty) {
        m_statusLabel->setText(tr("Unsaved order changes."));
    } else {
        m_statusLabel->setText(tr("Drag packs or use Up/Down, then click Save Order."));
    }
}

void ResourcePackOrderPanel::moveSelectedUp()
{
    const int modelIdx = selectedModelIndex();
    if (modelIdx < 0) {
        return;
    }
    const QString selectedId = m_orderModel.entries().at(modelIdx).id;
    const bool includeSystem = m_showSystemCheck && m_showSystemCheck->isChecked();
    if (m_orderModel.moveVisibleUp(modelIdx, includeSystem)) {
        m_dirty = true;
        rebuildList(selectedId);
    }
}

void ResourcePackOrderPanel::moveSelectedDown()
{
    const int modelIdx = selectedModelIndex();
    if (modelIdx < 0) {
        return;
    }
    const QString selectedId = m_orderModel.entries().at(modelIdx).id;
    const bool includeSystem = m_showSystemCheck && m_showSystemCheck->isChecked();
    if (m_orderModel.moveVisibleDown(modelIdx, includeSystem)) {
        m_dirty = true;
        rebuildList(selectedId);
    }
}

void ResourcePackOrderPanel::toggleSelectedEnabled()
{
    const int modelIdx = selectedModelIndex();
    if (modelIdx < 0 || modelIdx >= m_orderModel.count()) {
        return;
    }
    const QString selectedId = m_orderModel.entries().at(modelIdx).id;
    const bool next = !m_orderModel.entries().at(modelIdx).enabled;
    if (m_orderModel.setEnabled(modelIdx, next)) {
        m_dirty = true;
        rebuildList(selectedId);
    }
}

void ResourcePackOrderPanel::onRowsMoved()
{
    if (m_rebuilding) {
        return;
    }
    QStringList newIds;
    newIds.reserve(m_listWidget->count());
    for (int i = 0; i < m_listWidget->count(); ++i) {
        newIds.append(m_listWidget->item(i)->data(Qt::UserRole).toString());
    }
    const bool includeSystem = m_showSystemCheck && m_showSystemCheck->isChecked();
    const QStringList beforeIds = m_orderModel.visibleIds(includeSystem);
    if (!m_orderModel.reorderVisibleIds(newIds, includeSystem)) {
        const QString selectedId =
            m_listWidget->currentItem() ? m_listWidget->currentItem()->data(Qt::UserRole).toString() : QString();
        QTimer::singleShot(0, this, [this, selectedId]() {
            if (!m_rebuilding) {
                rebuildList(selectedId);
            }
        });
        return;
    }
    const QStringList afterIds = m_orderModel.visibleIds(includeSystem);
    if (afterIds != beforeIds) {
        m_dirty = true;
    }
    if (afterIds != newIds) {
        const QString selectedId =
            m_listWidget->currentItem() ? m_listWidget->currentItem()->data(Qt::UserRole).toString() : QString();
        QTimer::singleShot(0, this, [this, selectedId]() {
            if (!m_rebuilding) {
                rebuildList(selectedId);
            }
        });
        return;
    }
    updateControlsState();
}

void ResourcePackOrderPanel::saveOrder()
{
    if (!m_instance || m_instance->isRunning()) {
        updateControlsState();
        return;
    }
    QString err;
    if (!m_orderModel.save(GameOptionsFile::optionsFilePath(m_instance), &err)) {
        QMessageBox::warning(this, tr("Resource Pack Order"), err);
        return;
    }
    m_dirty = false;
    updateControlsState();
}
