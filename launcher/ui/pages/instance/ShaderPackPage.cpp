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

#include "ShaderPackPage.h"
#include "ShaderProfileDialog.h"
#include "ui_ExternalResourcesPage.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>

#include "ResourceDownloadTask.h"

#include "minecraft/mod/ShaderPackFolderModel.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/ResourceDownloadDialog.h"
#include "ui/dialogs/ResourceUpdateDialog.h"

ShaderPackPage::ShaderPackPage(MinecraftInstance* instance, ShaderPackFolderModel* model, QWidget* parent)
    : ExternalResourcesPage(instance, model, parent), m_model(model)
{
    ui->actionDownloadItem->setText(tr("Download Packs"));
    ui->actionDownloadItem->setToolTip(tr("Download shader packs from online mod platforms"));
    ui->actionDownloadItem->setEnabled(true);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionDownloadItem);

    connect(ui->actionDownloadItem, &QAction::triggered, this, &ShaderPackPage::downloadShaderPack);

    ui->actionUpdateItem->setToolTip(tr("Try to check or update all selected shader packs (all shader packs if none are selected)"));
    connect(ui->actionUpdateItem, &QAction::triggered, this, &ShaderPackPage::updateShaderPacks);
    ui->actionsToolbar->insertActionBefore(ui->actionAddItem, ui->actionUpdateItem);

    auto* updateMenu = new QMenu(this);

    auto* update = updateMenu->addAction(ui->actionUpdateItem->text());
    connect(update, &QAction::triggered, this, &ShaderPackPage::updateShaderPacks);

    updateMenu->addAction(ui->actionResetItemMetadata);
    connect(ui->actionResetItemMetadata, &QAction::triggered, this, &ShaderPackPage::deleteShaderPackMetadata);

    ui->actionUpdateItem->setMenu(updateMenu);

    ui->actionChangeVersion->setToolTip(tr("Change a shader pack's version."));
    connect(ui->actionChangeVersion, &QAction::triggered, this, &ShaderPackPage::changeShaderPackVersion);
    ui->actionsToolbar->insertActionAfter(ui->actionUpdateItem, ui->actionChangeVersion);

    m_shaderBar = new QWidget(this);
    auto* barLayout = new QHBoxLayout(m_shaderBar);
    barLayout->setContentsMargins(0, 2, 0, 4);
    barLayout->setSpacing(10);

    m_enableShadersCheck = new QCheckBox(tr("Enable Shaders"), m_shaderBar);
    m_shaderStatusLabel = new QLabel(m_shaderBar);
    m_shaderStatusLabel->setWordWrap(false);
    m_shaderStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_setActiveBtn = new QPushButton(tr("Set Active"), m_shaderBar);
    m_configureBtn = new QPushButton(tr("Configure Shader..."), m_shaderBar);

    barLayout->addWidget(m_enableShadersCheck);
    barLayout->addWidget(m_shaderStatusLabel, 1);
    barLayout->addWidget(m_setActiveBtn);
    barLayout->addWidget(m_configureBtn);
    ui->gridLayout->addWidget(m_shaderBar, 0, 1, 1, 2);

    connect(m_enableShadersCheck, &QCheckBox::toggled, this, &ShaderPackPage::toggleShadersEnabled);
    connect(m_setActiveBtn, &QPushButton::clicked, this, &ShaderPackPage::setSelectedShaderActive);
    connect(m_configureBtn, &QPushButton::clicked, this, &ShaderPackPage::configureSelectedShader);
    connect(ui->treeView->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection&, const QItemSelection&) { refreshShaderState(); });
    if (m_model) {
        connect(m_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex&, const QModelIndex&) { refreshShaderState(); });
        connect(m_model, &QAbstractItemModel::modelReset, this, [this]() { refreshShaderState(); });
        connect(m_model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int, int) { refreshShaderState(); });
        connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex&, int, int) { refreshShaderState(); });
    }
    if (m_instance) {
        connect(m_instance, &BaseInstance::runningStatusChanged, this, [this](bool) { refreshShaderState(); });
    }
    refreshShaderState();
}

void ShaderPackPage::downloadShaderPack()
{
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    m_downloadDialog = new ResourceDownload::ShaderPackDownloadDialog(this, m_model, m_instance);
    connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
    connect(m_downloadDialog, &QDialog::finished, this, &ShaderPackPage::downloadDialogFinished);

    m_downloadDialog->open();
}

void ShaderPackPage::downloadDialogFinished(int result)
{
    if (result != 0) {
        auto* tasks = new ConcurrentTask("Download Shader Packs", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
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

void ShaderPackPage::updateShaderPacks()
{
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Shader pack updates are unavailable when metadata is disabled!"));
        return;
    }
    if (m_instance != nullptr && m_instance->isRunning()) {
        auto response =
            CustomMessageBox::selectable(this, tr("Confirm Update"),
                                         tr("Updating shader packs while the game is running may pack duplication and game crashes.\n"
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
        CustomMessageBox::selectable(this, tr("Aborted"), tr("The shader pack updater was aborted!"), QMessageBox::Warning)->show();
        return;
    }
    if (updateDialog.noUpdates()) {
        QString message{ tr("'%1' is up-to-date! :)").arg(modsList.front()->name()) };
        if (modsList.size() > 1) {
            if (useAll) {
                message = tr("All shader packs are up-to-date! :)");
            } else {
                message = tr("All selected shader packs are up-to-date! :)");
            }
        }
        CustomMessageBox::selectable(this, tr("Update checker"), message)->exec();
        return;
    }

    if (updateDialog.exec() != 0) {
        auto* tasks = new ConcurrentTask("Download Shader Packs", APPLICATION->settings()->get("NumberOfConcurrentDownloads").toInt());
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

void ShaderPackPage::deleteShaderPackMetadata()
{
    auto selection = m_filterModel->mapSelectionToSource(ui->treeView->selectionModel()->selection()).indexes();
    auto selectionCount = m_model->selectedShaderPacks(selection).length();
    if (selectionCount == 0) {
        return;
    }
    if (selectionCount > 1) {
        auto response = CustomMessageBox::selectable(this, tr("Confirm Removal"),
                                                     tr("You are about to remove the metadata for %1 shader packs.\n"
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

void ShaderPackPage::changeShaderPackVersion()
{
    if (m_instance->typeName() != "Minecraft") {
        return;  // this is a null instance or a legacy instance
    }

    if (APPLICATION->settings()->get("ModMetadataDisabled").toBool()) {
        QMessageBox::critical(this, tr("Error"), tr("Shader pack updates are unavailable when metadata is disabled!"));
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

    m_downloadDialog = new ResourceDownload::ShaderPackDownloadDialog(this, m_model, m_instance, true);
    connect(this, &QObject::destroyed, m_downloadDialog, &QDialog::close);
    connect(m_downloadDialog, &QDialog::finished, this, &ShaderPackPage::downloadDialogFinished);

    m_downloadDialog->setResourceMetadata(resource.metadata());
    m_downloadDialog->open();
}

void ShaderPackPage::openedImpl()
{
    ExternalResourcesPage::openedImpl();
    refreshShaderState();
}

QString ShaderPackPage::selectedShaderFileName() const
{
    if (!ui->treeView->selectionModel() || !m_filterModel || !m_model) {
        return {};
    }
    const QModelIndexList rows = ui->treeView->selectionModel()->selectedRows();
    if (rows.count() != 1) {
        return {};
    }
    const int srcRow = m_filterModel->mapToSource(rows.first()).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) {
        return {};
    }
    const QString rawName = m_model->at(srcRow).fileinfo().fileName();
    return rawName.endsWith(QStringLiteral(".disabled"), Qt::CaseInsensitive) ? rawName.left(rawName.size() - 9) : rawName;
}

void ShaderPackPage::refreshShaderState()
{
    if (!m_shaderBar || !m_instance) {
        return;
    }
    auto* mcInst = dynamic_cast<MinecraftInstance*>(m_instance);
    const QString modsRoot = mcInst ? mcInst->modsRoot() : QString();
    m_shaderState = ShaderConfigManager::loadState(m_instance->gameRoot(), modsRoot);

    const bool running = m_instance->isRunning();
    const bool hasBackend = m_shaderState.backend != ShaderConfigManager::Backend::Unknown;
    const QString selected = selectedShaderFileName();
    bool selectedEnabled = false;
    if (ui->treeView->selectionModel() && m_filterModel && m_model) {
        const QModelIndexList rows = ui->treeView->selectionModel()->selectedRows();
        if (rows.count() == 1) {
            const int srcRow = m_filterModel->mapToSource(rows.first()).row();
            if (srcRow >= 0 && srcRow < m_model->rowCount()) {
                selectedEnabled = m_model->at(srcRow).enabled();
            }
        }
    }
    const QString configTarget = !selected.isEmpty() ? selected : m_shaderState.activePack;

    const QSignalBlocker blocker(m_enableShadersCheck);
    m_enableShadersCheck->setChecked(m_shaderState.shadersEnabled);
    m_enableShadersCheck->setEnabled(hasBackend && !running);
    m_setActiveBtn->setEnabled(hasBackend && !running && !selected.isEmpty() && selectedEnabled);
    m_configureBtn->setEnabled(!configTarget.isEmpty());

    if (!hasBackend) {
        m_shaderStatusLabel->setText(tr("No shader mod (Iris / Oculus / OptiFine) detected."));
    } else if (running) {
        m_shaderStatusLabel->setText(tr("Game running — close Minecraft to switch shaders."));
    } else {
        const QString engine = ShaderConfigManager::backendDisplayName(m_shaderState.backend);
        const QString active = m_shaderState.activePack.isEmpty() ? tr("None") : m_shaderState.activePack;
        m_shaderStatusLabel->setText(tr("%1 | Active: %2").arg(engine, active));
    }
}

void ShaderPackPage::toggleShadersEnabled(bool enabled)
{
    if (!m_instance || m_instance->isRunning() || m_shaderState.backend == ShaderConfigManager::Backend::Unknown) {
        refreshShaderState();
        return;
    }
    m_shaderState.shadersEnabled = enabled;
    QString err;
    if (!ShaderConfigManager::saveState(m_shaderState, &err)) {
        QMessageBox::warning(this, tr("Shader Settings"), err);
    }
    refreshShaderState();
}

void ShaderPackPage::setSelectedShaderActive()
{
    if (!m_instance || m_instance->isRunning() || m_shaderState.backend == ShaderConfigManager::Backend::Unknown ||
        !ui->treeView->selectionModel() || !m_filterModel || !m_model) {
        refreshShaderState();
        return;
    }
    const QModelIndexList rows = ui->treeView->selectionModel()->selectedRows();
    if (rows.count() != 1) {
        refreshShaderState();
        return;
    }
    const int srcRow = m_filterModel->mapToSource(rows.first()).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount() || !m_model->at(srcRow).enabled()) {
        refreshShaderState();
        return;
    }
    const QString selected = selectedShaderFileName();
    if (selected.isEmpty()) {
        refreshShaderState();
        return;
    }
    ShaderConfigManager::State nextState = m_shaderState;
    nextState.activePack = selected;
    nextState.shadersEnabled = true;
    QString err;
    if (!ShaderConfigManager::saveState(nextState, &err)) {
        QMessageBox::warning(this, tr("Shader Settings"), err);
    }
    refreshShaderState();
}

void ShaderPackPage::configureSelectedShader()
{
    if (!m_instance || !m_model) {
        return;
    }
    const QString selected = selectedShaderFileName();
    const QString target = !selected.isEmpty() ? selected : m_shaderState.activePack;
    if (target.isEmpty()) {
        return;
    }
    ShaderProfileDialog dlg(m_model->dir().absolutePath(), target, m_instance->isRunning(), this);
    dlg.exec();
}
