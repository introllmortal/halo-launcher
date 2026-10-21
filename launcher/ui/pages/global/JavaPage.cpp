// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (c) 2022 Jamie Mansfield <jmansfield@cadixdev.org>
 *  Copyright (C) 2022 Sefa Eyeoglu <contact@scrumplex.net>
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

#include "JavaPage.h"
#include "diagnostics/GpuSanityChecker.h"
#include "BuildConfig.h"
#include "JavaCommon.h"
#include "java/JavaInstall.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/java/InstallJavaDialog.h"
#include "ui_JavaPage.h"

#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QStringListModel>
#include <QTabBar>

#include "ui/dialogs/VersionSelectDialog.h"

#include "java/JavaInstallList.h"
#include "java/JavaUtils.h"

#include <FileSystem.h>
#include "Application.h"
#include "settings/SettingsObject.h"

JavaPage::JavaPage(QWidget* parent) : QWidget(parent), ui(new Ui::JavaPage)
{
    ui->setupUi(this);

    if (BuildConfig.JAVA_DOWNLOADER_ENABLED) {
        ui->managedJavaList->initialize(new JavaInstallList(this, true));
        ui->managedJavaList->setResizeOn(2);
        ui->managedJavaList->selectCurrent();
        ui->managedJavaList->setEmptyString(tr("No managed Java versions are installed"));
        ui->managedJavaList->setEmptyErrorString(tr("Couldn't load the managed Java list!"));
    } else
        ui->tabWidget->tabBar()->hide();

    connect(ui->btnEnforceGpu, &QPushButton::clicked, this, &JavaPage::enforceGpuClicked);
    connect(ui->btnResetGpu, &QPushButton::clicked, this, &JavaPage::resetGpuClicked);
    updateGpuStatus();

    if (APPLICATION->settings()->getSetting("JavaPath")) {
        connect(APPLICATION->settings()->getSetting("JavaPath").get(), &Setting::SettingChanged, this, &JavaPage::updateGpuStatus);
    }
}

JavaPage::~JavaPage()
{
    delete ui;
}

void JavaPage::retranslate()
{
    ui->retranslateUi(this);
    updateGpuStatus();
}

bool JavaPage::apply()
{
    ui->javaSettings->saveSettings();
    JavaCommon::checkJVMArgs(APPLICATION->settings()->get("JvmArgs").toString(), this);
    updateGpuStatus();
    return true;
}

void JavaPage::on_downloadJavaButton_clicked()
{
    auto jdialog = new Java::InstallDialog({}, nullptr, this);
    jdialog->exec();
    ui->managedJavaList->loadList();
}

void JavaPage::on_removeJavaButton_clicked()
{
    auto version = ui->managedJavaList->selectedVersion();
    auto dcast = std::dynamic_pointer_cast<JavaInstall>(version);
    if (!dcast) {
        return;
    }
    QDir dir(APPLICATION->javaPath());

    auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (auto& entry : entries) {
        if (dcast->path.startsWith(entry.canonicalFilePath())) {
            auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                         tr("You are about to remove  the Java installation named \"%1\".\n"
                                                            "Are you sure?")
                                                             .arg(entry.fileName()),
                                                         QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                                ->exec();

            if (response == QMessageBox::Yes) {
                FS::deletePath(entry.canonicalFilePath());
                ui->managedJavaList->loadList();
            }
            break;
        }
    }
}
void JavaPage::on_refreshJavaButton_clicked()
{
    ui->managedJavaList->loadList();
}

void JavaPage::updateGpuStatus()
{
    if (!GpuSanityChecker::isSupported()) {
        ui->gpuGroupBox->hide();
        return;
    }

    ui->gpuGroupBox->show();
    QString javaPath = APPLICATION->settings()->get("JavaPath").toString();
    QString validJavaPath = GpuSanityChecker::validateAndCanonicalizeJavaPath(javaPath);

    bool isDual = GpuSanityChecker::isDualGpu();
    QString dGpu = GpuSanityChecker::discreteGpuNames().join(", ");
    QString iGpu = GpuSanityChecker::integratedGpuNames().join(", ");

    if (validJavaPath.isEmpty()) {
        ui->gpuStatusLabel->setText(tr("Select or configure a valid Java path above to manage GPU preferences."));
        ui->btnEnforceGpu->setEnabled(false);
        ui->btnResetGpu->setEnabled(false);
        return;
    }

    bool isEnforced = GpuSanityChecker::isHighPerformanceEnforced(validJavaPath);

    if (isDual) {
        if (isEnforced) {
            ui->gpuStatusLabel->setText(tr("<b>Status: Optimal</b> — High-Performance GPU preference (<code>GpuPreference=2;</code>) is active in Windows for Java.<br/>"
                                           "Dedicated GPU: <b>%1</b> (Integrated: %2)")
                                            .arg(dGpu.isEmpty() ? tr("Discrete GPU") : dGpu, iGpu.isEmpty() ? tr("Integrated GPU") : iGpu));
            ui->btnEnforceGpu->setText(tr("Re-apply High-Performance"));
            ui->btnEnforceGpu->setEnabled(true);
            ui->btnResetGpu->setEnabled(true);
        } else {
            ui->gpuStatusLabel->setText(tr("<b>Status: Warning</b> — Hybrid GPU detected (%1 + %2).<br/>"
                                           "Minecraft is at risk of running on the slower integrated graphics, which can cause lag or shader crashes.")
                                            .arg(dGpu.isEmpty() ? tr("Discrete GPU") : dGpu, iGpu.isEmpty() ? tr("Integrated GPU") : iGpu));
            ui->btnEnforceGpu->setText(tr("Enforce High-Performance GPU (1-Click)"));
            ui->btnEnforceGpu->setEnabled(true);
            ui->btnResetGpu->setEnabled(false);
        }
    } else {
        if (isEnforced) {
            ui->gpuStatusLabel->setText(tr("Single GPU detected (%1). High-performance GPU preference is configured.").arg(dGpu.isEmpty() ? iGpu : dGpu));
            ui->btnEnforceGpu->setText(tr("Re-apply High-Performance"));
            ui->btnEnforceGpu->setEnabled(true);
            ui->btnResetGpu->setEnabled(true);
        } else {
            ui->gpuStatusLabel->setText(tr("Single GPU detected (%1). High-performance preference is optional.").arg(dGpu.isEmpty() ? iGpu : dGpu));
            ui->btnEnforceGpu->setText(tr("Enforce High-Performance GPU"));
            ui->btnEnforceGpu->setEnabled(true);
            ui->btnResetGpu->setEnabled(false);
        }
    }
}

void JavaPage::enforceGpuClicked()
{
    QString javaPath = APPLICATION->settings()->get("JavaPath").toString();
    if (GpuSanityChecker::setGpuPreference(javaPath)) {
        CustomMessageBox::selectable(this, tr("GPU Preference Set"),
                                     tr("High-Performance GPU preference (GpuPreference=2) has been successfully applied in Windows Registry for:\n%1")
                                         .arg(GpuSanityChecker::validateAndCanonicalizeJavaPath(javaPath)),
                                     QMessageBox::Information)
            ->exec();
    } else {
        CustomMessageBox::selectable(this, tr("Failed to Set GPU Preference"),
                                     tr("Could not apply GPU preference. Ensure that the Java path exists and points to java.exe or javaw.exe."),
                                     QMessageBox::Warning)
            ->exec();
    }
    updateGpuStatus();
}

void JavaPage::resetGpuClicked()
{
    QString javaPath = APPLICATION->settings()->get("JavaPath").toString();
    if (GpuSanityChecker::removeGpuPreference(javaPath)) {
        CustomMessageBox::selectable(this, tr("GPU Preference Reset"),
                                     tr("GPU preference has been removed from Windows Registry. System default GPU selection will be used."),
                                     QMessageBox::Information)
            ->exec();
    } else {
        CustomMessageBox::selectable(this, tr("Failed to Reset GPU Preference"),
                                     tr("Could not remove GPU preference from Windows Registry."),
                                     QMessageBox::Warning)
            ->exec();
    }
    updateGpuStatus();
}
