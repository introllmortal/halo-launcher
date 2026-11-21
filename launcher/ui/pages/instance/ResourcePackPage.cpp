// SPDX-FileCopyrightText: 2023 flowln <flowlnlnln@gmail.com>
//
// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 *      Copyright 2013-2021 MultiMC Contributors
 *
 *      Licensed under the Apache License, Version 2.0 (the "License");
 *      you may not use this file except in compliance with the License.
 *      You may obtain a copy of the License at
 *
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 *      Unless required by applicable law or agreed to in writing, software
 *      distributed under the License is distributed on an "AS IS" BASIS,
 *      WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *      See the License for the specific language governing permissions and
 *      limitations under the License.
 */

#include "ResourcePackPage.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>

#include "ResourcePackOrderPanel.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/dialogs/ResourceUpdateDialog.h"

ResourcePackPage::ResourcePackPage(MinecraftInstance* instance, ResourcePackFolderModel* model, QWidget* parent)
    : ExternalResourcesPage(instance, model, parent), m_model(model)
{
    ui->actionDownloadItem->setText(tr("Download Packs"));
    ui->actionDownloadItem->setToolTip(tr("Download resource packs from online mod platforms"));
    ui->actionDownloadItem->setEnabled(true);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionDownloadItem);

    connect(ui->actionDownloadItem, &QAction::triggered, this, &ResourcePackPage::downloadResourcePacks);

    ui->actionUpdateItem->setToolTip(tr("Try to check or update all selected resource packs (all resource packs if none are selected)"));
    connect(ui->actionUpdateItem, &QAction::triggered, this, &ResourcePackPage::updateResourcePacks);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionUpdateItem);

    auto* updateMenu = new QMenu(this);

    auto* update = updateMenu->addAction(ui->actionUpdateItem->text());
    connect(update, &QAction::triggered, this, &ResourcePackPage::updateResourcePacks);

    updateMenu->addAction(ui->actionResetItemMetadata);
    connect(ui->actionResetItemMetadata, &QAction::triggered, this, &ResourcePackPage::deleteResourcePackMetadata);

    ui->actionUpdateItem->setMenu(updateMenu);

    ui->actionChangeVersion->setToolTip(tr("Change a mod's version."));
    connect(ui->actionChangeVersion, &QAction::triggered, this, &ResourcePackPage::changeResourcePackVersion);
    ui->actionsToolbar->insertActionAfter(ui->actionUpdateItem, ui->actionChangeVersion);

    m_modeBar = new QWidget(this);
    m_modeBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* modeLayout = new QHBoxLayout(m_modeBar);
    modeLayout->setContentsMargins(0, 2, 0, 4);
    modeLayout->setSpacing(6);

    m_filesModeBtn = new QPushButton(tr("Files in Folder"), m_modeBar);
    m_filesModeBtn->setCheckable(true);
    m_filesModeBtn->setChecked(true);
    m_orderModeBtn = new QPushButton(tr("In-Game Order"), m_modeBar);
    m_orderModeBtn->setCheckable(true);

    auto* modeGroup = new QButtonGroup(m_modeBar);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_filesModeBtn, 0);
    modeGroup->addButton(m_orderModeBtn, 1);

    modeLayout->addWidget(m_filesModeBtn);
    modeLayout->addWidget(m_orderModeBtn);
    modeLayout->addStretch(1);
    ui->gridLayout->addWidget(m_modeBar, 0, 1, 1, 2);

    m_orderPanel = new ResourcePackOrderPanel(m_instance, this, m_model);
    m_orderPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->gridLayout->addWidget(m_orderPanel, 1, 1, 3, 2);
    ui->gridLayout->setRowStretch(0, 0);
    ui->gridLayout->setRowStretch(1, 1);
    setOrderViewActive(false);

    connect(m_filesModeBtn, &QPushButton::clicked, this, [this]() { setOrderViewActive(false); });
    connect(m_orderModeBtn, &QPushButton::clicked, this, [this]() { setOrderViewActive(true); });

    if (m_instance) {
        connect(m_instance, &BaseInstance::runningStatusChanged, m_orderPanel, [this](bool) {
            if (!m_orderPanel) {
                return;
            }
            if (!m_orderPanel->hasUnsavedChanges()) {
                m_orderPanel->reload();
            } else {
                m_orderPanel->refreshRunningState();
            }
        });
    }
}

void ResourcePackPage::setOrderViewActive(bool active)
{
    m_orderViewActive = active;
    ui->treeView->setVisible(!active);
    ui->filterEdit->setVisible(!active);
    ui->actionsToolbar->setVisible(!active);
    if (active) {
        ui->frame->setVisible(false);
    } else {
        const QModelIndex current = ui->treeView->currentIndex();
        if (current.isValid()) {
            updateFrame(current, {});
        } else {
            ui->frame->clear();
        }
    }
    if (m_orderPanel) {
        m_orderPanel->setVisible(active);
        if (active && !m_orderPanel->hasUnsavedChanges()) {
            m_orderPanel->reload();
        }
    }
}

void ResourcePackPage::openedImpl()
{
    ExternalResourcesPage::openedImpl();
    if (m_orderViewActive) {
        ui->actionsToolbar->setVisible(false);
    }
    if (m_orderPanel && !m_orderPanel->hasUnsavedChanges()) {
        m_orderPanel->reload();
    }
}

void ResourcePackPage::retranslate()
{
    ExternalResourcesPage::retranslate();
    if (m_filesModeBtn) {
        m_filesModeBtn->setText(tr("Files in Folder"));
    }
    if (m_orderModeBtn) {
        m_orderModeBtn->setText(tr("In-Game Order"));
    }
}

void ResourcePackPage::updateFrame(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (m_orderViewActive) {
        ui->frame->setVisible(false);
        return;
    }
    auto sourceCurrent = m_filterModel->mapToSource(current);
    if (!sourceCurrent.isValid()) {
        ui->frame->clear();
        return;
    }
    int row = sourceCurrent.row();
    auto& rp = m_model->at(row);
    ui->frame->updateWithResourcePack(rp);
}

void ResourcePackPage::downloadResourcePacks()
{
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    m_downloadDialog = new ResourceDownload::ResourcePackDownloadDialog(this, m_model, m_instance);
    connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
    connect(m_downloadDialog, &QDialog::finished, this, &ResourcePackPage::downloadDialogFinished);

    m_downloadDialog->open();
}

void ResourcePackPage::downloadDialogFinished(int result)
{
    if (result != 0) {
        auto* tasks = new ConcurrentTask("Download Resource Pack", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, [this, tasks](const QString& reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, [this, tasks]() {
            CustomMessageBox::selectable(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count()) {
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
            }

            tasks->deleteLater();
        });

        if (m_downloadDialog) {
            for (auto& task : m_downloadDialog->getTasks()) {
                tasks->addTask(task);
            }
        } else {
            qWarning() << "ResourceDownloadDialog vanished before we could collect tasks!";
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
    if (m_downloadDialog) {
        m_downloadDialog->deleteLater();
    }
}

void ResourcePackPage::updateResourcePacks()
{
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Resource pack updates are unavailable when metadata is disabled!"));
        return;
    }
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response = CustomMessageBox::selectable(
                            this, tr("Confirm Update"),
                            tr("Updating resource packs while the game is running may cause pack duplication and game crashes.\n"
                               "The old files may not be deleted as they are in use.\n"
                               "Are you sure you want to do this?"),
                            QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();

    auto modsList = m_model->selectedResources(selection);
    bool useAll = modsList.empty();
    if (useAll) {
        modsList = m_model->allResources();
    }

    ResourceUpdateDialog updateDialog(this, m_instance, m_model, modsList, false);
    updateDialog.checkCandidates();

    if (updateDialog.aborted()) {
        CustomMessageBox::selectable(this, tr("Aborted"), tr("The resource pack updater was aborted!"), QMessageBox::Warning)->show();
        return;
    }
    if (updateDialog.noUpdates()) {
        QString message{ tr("'%1' is up-to-date! :)").arg(modsList.front()->name()) };
        if (modsList.size() > 1) {
            if (useAll) {
                message = tr("All resource packs are up-to-date! :)");
            } else {
                message = tr("All selected resource packs are up-to-date! :)");
            }
        }
        CustomMessageBox::selectable(this, tr("Update checker"), message)->exec();
        return;
    }

    if (updateDialog.exec() != 0) {
        auto* tasks = new ConcurrentTask("Download Resource Packs", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
        connect(tasks, &Task::failed, [this, tasks](const QString& reason) {
            CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::aborted, [this, tasks]() {
            CustomMessageBox::selectable(this, tr("Aborted"), tr("Download stopped by user."), QMessageBox::Information)->show();
            tasks->deleteLater();
        });
        connect(tasks, &Task::succeeded, [this, tasks]() {
            QStringList warnings = tasks->warnings();
            if (warnings.count()) {
                CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
            }
            tasks->deleteLater();
        });

        for (const auto& task : updateDialog.getTasks()) {
            tasks->addTask(task);
        }

        ProgressDialog loadDialog(this);
        loadDialog.setSkipButton(true, tr("Abort"));
        loadDialog.execWithTask(tasks);

        m_model->update();
    }
}

void ResourcePackPage::deleteResourcePackMetadata()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
    auto selectionCount = m_model->selectedResourcePacks(selection).length();
    if (selectionCount == 0) {
        return;
    }
    if (selectionCount > 1) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"),
                                                     tr("You are about to remove the metadata for %1 resource packs.\n"
                                                        "Are you sure?")
                                                         .arg(selectionCount),
                                                     QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                            ->exec();

        if (response != QMessageBox::Yes) {
            return;
        }
    }

    m_model->deleteMetadata(selection);
}

void ResourcePackPage::changeResourcePackVersion()
{
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Resource pack updates are unavailable when metadata is disabled!"));
        return;
    }

    const QModelIndexList rows = ui->treeView->selectionModel()->selectedRows();

    if (rows.count() != 1) {
        return;
    }

    Resource& resource = m_model->at(m_filterModel->mapToSource(rows[0]).row());

    if (resource.metadata() == nullptr) {
        return;
    }

    m_downloadDialog = new ResourceDownload::ResourcePackDownloadDialog(this, m_model, m_instance, true);
    connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
    connect(m_downloadDialog, &QDialog::finished, this, &ResourcePackPage::downloadDialogFinished);

    m_downloadDialog->setResourceMetadata(resource.metadata());
    m_downloadDialog->open();
}
