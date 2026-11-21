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
#include "StorageCleanerDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtConcurrentRun>

#include "BaseInstance.h"
#include "ui/dialogs/CustomMessageBox.h"

StorageCleanerDialog::StorageCleanerDialog(BaseInstance* instance, QWidget* parent)
    : QDialog(parent), m_instance(instance)
{
    setWindowTitle(tr("Instance Storage & Cache Cleaner"));
    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);
    resize(520, 380);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    m_headerLabel = new QLabel(
        m_instance ? tr("Storage analysis for \"%1\"").arg(m_instance->name()) : tr("No instance selected"), this);
    QFont f = m_headerLabel->font();
    f.setPointSize(f.pointSize() + 2);
    f.setBold(true);
    m_headerLabel->setFont(f);
    layout->addWidget(m_headerLabel);

    m_statusLabel = new QLabel(tr("Scanning instance folders..."), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_listLayout = new QVBoxLayout();
    m_listLayout->setSpacing(8);
    layout->addLayout(m_listLayout);
    layout->addStretch(1);

    auto* bottom = new QHBoxLayout();
    m_refreshButton = new QPushButton(tr("Rescan"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &StorageCleanerDialog::startScan);

    m_cleanButton = new QPushButton(tr("Clean Selected"), this);
    m_cleanButton->setEnabled(false);
    connect(m_cleanButton, &QPushButton::clicked, this, &StorageCleanerDialog::cleanSelected);

    auto* closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(closeBox, &QDialogButtonBox::rejected, this, &QDialog::accept);

    bottom->addWidget(m_refreshButton);
    bottom->addStretch(1);
    bottom->addWidget(m_cleanButton);
    bottom->addWidget(closeBox);
    layout->addLayout(bottom);

    connect(&m_watcher, &QFutureWatcher<InstanceStorageCleaner::ScanReport>::finished, this,
            &StorageCleanerDialog::onScanFinished);
    startScan();
}

StorageCleanerDialog::~StorageCleanerDialog()
{
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

void StorageCleanerDialog::startScan()
{
    if (!m_instance || m_watcher.isRunning()) {
        return;
    }
    for (QCheckBox* cb : m_checkBoxes) {
        cb->deleteLater();
    }
    m_checkBoxes.clear();
    m_cleanButton->setEnabled(false);
    m_refreshButton->setEnabled(false);
    m_statusLabel->setText(tr("Scanning instance folders..."));

    const InstanceStorageCleaner::InstancePaths paths = InstanceStorageCleaner::capturePaths(m_instance);
    m_watcher.setFuture(QtConcurrent::run([paths]() { return InstanceStorageCleaner::scanPaths(paths); }));
}

void StorageCleanerDialog::onScanFinished()
{
    m_refreshButton->setEnabled(true);
    m_report = m_watcher.result();
    if (!m_report.error.isEmpty()) {
        m_statusLabel->setText(m_report.error);
        return;
    }

    for (const auto& cat : m_report.categories) {
        const QString text = QStringLiteral("%1 — %2 (%3 files)")
                                 .arg(cat.displayName, InstanceStorageCleaner::formatBytes(cat.bytes))
                                 .arg(cat.fileCount);
        auto* cb = new QCheckBox(text, this);
        const bool hasFiles = cat.fileCount > 0;
        const bool defaultOn = hasFiles && cat.category != InstanceStorageCleaner::Category::ModBackups;
        cb->setChecked(defaultOn);
        cb->setEnabled(hasFiles);
        cb->setProperty("haloCategory", static_cast<int>(cat.category));
        cb->setProperty("haloBytes", cat.bytes);
        if (cat.category == InstanceStorageCleaner::Category::ModBackups) {
            cb->setToolTip(tr("Contains temporary .jar.bak / .halo_bak backup files"));
        }
        connect(cb, &QCheckBox::toggled, this, &StorageCleanerDialog::updateTotalLabel);
        m_listLayout->addWidget(cb);
        m_checkBoxes.append(cb);
    }

    updateTotalLabel();
}

void StorageCleanerDialog::updateTotalLabel()
{
    qint64 selectedBytes = 0;
    for (QCheckBox* cb : m_checkBoxes) {
        if (cb->isChecked()) {
            selectedBytes += cb->property("haloBytes").toLongLong();
        }
    }
    m_statusLabel->setText(tr("Total instance size: %1  |  Cleanable: %2  |  Selected: %3")
                               .arg(InstanceStorageCleaner::formatBytes(m_report.totalInstanceBytes),
                                    InstanceStorageCleaner::formatBytes(m_report.totalCleanableBytes),
                                    InstanceStorageCleaner::formatBytes(selectedBytes)));
    m_cleanButton->setEnabled(selectedBytes > 0 && m_instance && !m_instance->isRunning());
}

void StorageCleanerDialog::cleanSelected()
{
    if (!m_instance) {
        return;
    }
    if (m_instance->isRunning()) {
        CustomMessageBox::selectable(this, tr("Instance Running"),
                                     tr("Stop Minecraft before cleaning instance files."), QMessageBox::Warning)
            ->show();
        return;
    }

    QList<InstanceStorageCleaner::Category> selected;
    for (QCheckBox* cb : m_checkBoxes) {
        if (cb->isChecked()) {
            selected.append(static_cast<InstanceStorageCleaner::Category>(cb->property("haloCategory").toInt()));
        }
    }
    if (selected.isEmpty()) {
        return;
    }

    QString err;
    const qint64 freed = InstanceStorageCleaner::cleanCategories(m_instance, selected, 3, &err);
    if (!err.isEmpty()) {
        CustomMessageBox::selectable(this, tr("Cleanup Warning"), err, QMessageBox::Warning)->show();
    } else {
        CustomMessageBox::selectable(this, tr("Cleanup Complete"),
                                     tr("Freed %1 from selected categories.").arg(InstanceStorageCleaner::formatBytes(freed)),
                                     QMessageBox::Information)
            ->show();
    }
    startScan();
}
