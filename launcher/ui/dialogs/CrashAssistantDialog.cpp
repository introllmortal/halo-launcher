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
#include "CrashAssistantDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScopeGuard>
#include <QStyle>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "ui/GuiUtil.h"

CrashAssistantDialog::CrashAssistantDialog(const DiagnosisResult& diagnosis,
                                           BaseInstance* instance,
                                           const QString& rawLog,
                                           QWidget* parent)
    : QDialog(parent), m_diagnosis(diagnosis), m_instance(instance), m_rawLog(rawLog)
{
    setupUi();
}

void CrashAssistantDialog::setupUi()
{
    setWindowTitle(tr("Halo Crash Assistant"));
    resize(600, 400);
    setMinimumSize(520, 340);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // 1. Header: Problem title & summary
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    auto* iconLabel = new QLabel(this);
    QIcon icon = QIcon::fromTheme(QStringLiteral("dialog-warning"));
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    }
    iconLabel->setPixmap(icon.pixmap(40, 40));
    iconLabel->setAlignment(Qt::AlignTop);
    headerLayout->addWidget(iconLabel);

    auto* titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(4);

    auto* titleLabel = new QLabel(m_diagnosis.title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    titleLayout->addWidget(titleLabel);

    if (!m_diagnosis.description.isEmpty()) {
        auto* descLabel = new QLabel(m_diagnosis.description, this);
        descLabel->setWordWrap(true);
        descLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        titleLayout->addWidget(descLabel);
    }

    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    // 2. Where & Why card
    if (!m_diagnosis.sourceSummary.isEmpty() || !m_diagnosis.whySummary.isEmpty()) {
        auto* causeFrame = new QFrame(this);
        causeFrame->setFrameShape(QFrame::StyledPanel);
        causeFrame->setStyleSheet(
            QStringLiteral("QFrame { background-color: rgba(148, 163, 184, 0.08); border: 1px solid rgba(148, 163, 184, 0.25); "
                           "border-radius: 6px; padding: 8px; }"));
        auto* causeLayout = new QVBoxLayout(causeFrame);
        causeLayout->setSpacing(6);

        auto addField = [causeFrame, causeLayout](const QString& heading, const QString& value) {
            if (value.isEmpty()) {
                return;
            }
            auto* headingLbl = new QLabel(heading, causeFrame);
            QFont hf = headingLbl->font();
            hf.setBold(true);
            headingLbl->setFont(hf);
            causeLayout->addWidget(headingLbl);

            auto* valLbl = new QLabel(value, causeFrame);
            valLbl->setWordWrap(true);
            valLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
            causeLayout->addWidget(valLbl);
        };

        addField(tr("Where the error came from:"), m_diagnosis.sourceSummary);
        addField(tr("Why this happened:"), m_diagnosis.whySummary);
        mainLayout->addWidget(causeFrame);
    }

    // 3. How to fix (calm guidance, no forced action)
    if (!m_diagnosis.solutionText.isEmpty()) {
        auto* guideFrame = new QFrame(this);
        guideFrame->setFrameShape(QFrame::StyledPanel);
        guideFrame->setStyleSheet(
            QStringLiteral("QFrame { background-color: rgba(14, 165, 233, 0.08); border: 1px solid rgba(14, 165, 233, 0.30); "
                           "border-radius: 6px; padding: 8px; }"));
        auto* guideLayout = new QVBoxLayout(guideFrame);
        guideLayout->setSpacing(4);

        auto* guideHeader = new QLabel(tr("How to fix:"), guideFrame);
        QFont gf = guideHeader->font();
        gf.setBold(true);
        guideHeader->setFont(gf);
        guideLayout->addWidget(guideHeader);

        auto* guideBody = new QLabel(m_diagnosis.solutionText, guideFrame);
        guideBody->setWordWrap(true);
        guideBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
        guideLayout->addWidget(guideBody);

        mainLayout->addWidget(guideFrame);
    }

    // 4. Collapsible Technical Details (collapsed by default)
    if (!m_diagnosis.technicalDetails.isEmpty()) {
        m_btnDetailsToggle = new QPushButton(tr("Show Technical Details"), this);
        m_btnDetailsToggle->setCheckable(true);
        m_btnDetailsToggle->setChecked(false);
        m_btnDetailsToggle->setFlat(true);
        m_btnDetailsToggle->setStyleSheet(QStringLiteral("QPushButton { text-align: left; font-weight: bold; padding: 4px 2px; }"));
        connect(m_btnDetailsToggle, &QPushButton::toggled, this, &CrashAssistantDialog::onToggleTechnicalDetails);
        mainLayout->addWidget(m_btnDetailsToggle);

        m_techEdit = new QTextEdit(this);
        m_techEdit->setReadOnly(true);
        m_techEdit->setFontFamily(QStringLiteral("Consolas"));
        m_techEdit->setPlainText(m_diagnosis.technicalDetails);
        m_techEdit->setMaximumHeight(120);
        m_techEdit->setVisible(false);
        mainLayout->addWidget(m_techEdit);
    }

    mainLayout->addStretch(1);

    // 5. Neutral Bottom Action Bar (user stays in control)
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_btnCopy = new QPushButton(tr("Copy Report"), this);
    connect(m_btnCopy, &QPushButton::clicked, this, &CrashAssistantDialog::onCopyDetails);
    buttonLayout->addWidget(m_btnCopy);

    m_btnOpenFolder = new QPushButton(tr("Open Folder"), this);
    m_btnOpenFolder->setEnabled(!resolveHelpfulFolder().isEmpty());
    connect(m_btnOpenFolder, &QPushButton::clicked, this, &CrashAssistantDialog::onOpenFolder);
    buttonLayout->addWidget(m_btnOpenFolder);

    m_btnUploadLog = new QPushButton(tr("Share Log (mclo.gs)"), this);
    connect(m_btnUploadLog, &QPushButton::clicked, this, &CrashAssistantDialog::onUploadLog);
    buttonLayout->addWidget(m_btnUploadLog);

    buttonLayout->addStretch(1);

    m_btnClose = new QPushButton(tr("Close"), this);
    m_btnClose->setDefault(true);
    connect(m_btnClose, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_btnClose);

    mainLayout->addLayout(buttonLayout);
}

QString CrashAssistantDialog::resolveHelpfulFolder() const
{
    if (!m_diagnosis.offendingConfigPath.isEmpty()) {
        const QString dir = QFileInfo(m_diagnosis.offendingConfigPath).absolutePath();
        if (QDir(dir).exists()) {
            return dir;
        }
    }
    if (!m_diagnosis.offendingModPath.isEmpty()) {
        const QString dir = QFileInfo(m_diagnosis.offendingModPath).absolutePath();
        if (QDir(dir).exists()) {
            return dir;
        }
    }
    if (!m_instance) {
        return {};
    }
    const QString gameRoot = m_instance->gameRoot();
    if (!gameRoot.isEmpty()) {
        const QString crashReports = FS::PathCombine(gameRoot, QStringLiteral("crash-reports"));
        if (QDir(crashReports).exists()) {
            return crashReports;
        }
        const QString logsDir = FS::PathCombine(gameRoot, QStringLiteral("logs"));
        if (QDir(logsDir).exists()) {
            return logsDir;
        }
        if (QDir(gameRoot).exists()) {
            return gameRoot;
        }
    }
    const QString instRoot = m_instance->instanceRoot();
    return (!instRoot.isEmpty() && QDir(instRoot).exists()) ? instRoot : QString();
}

void CrashAssistantDialog::onOpenFolder()
{
    const QString dir = resolveHelpfulFolder();
    if (dir.isEmpty()) {
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void CrashAssistantDialog::onToggleTechnicalDetails(bool expanded)
{
    if (m_techEdit) {
        m_techEdit->setVisible(expanded);
    }
    if (m_btnDetailsToggle) {
        m_btnDetailsToggle->setText(expanded ? tr("Hide Technical Details") : tr("Show Technical Details"));
    }
}

QString CrashAssistantDialog::buildReportText() const
{
    QStringList lines;
    lines.append(QStringLiteral("=== Halo Crash Assistant Report ==="));
    lines.append(QStringLiteral("Problem: %1").arg(m_diagnosis.title));
    if (!m_diagnosis.sourceSummary.isEmpty()) {
        lines.append(QStringLiteral("Source: %1").arg(m_diagnosis.sourceSummary));
    }
    if (!m_diagnosis.whySummary.isEmpty()) {
        lines.append(QStringLiteral("Why: %1").arg(m_diagnosis.whySummary));
    }
    if (!m_diagnosis.description.isEmpty()) {
        lines.append(QStringLiteral("Description: %1").arg(m_diagnosis.description));
    }
    if (!m_diagnosis.solutionText.isEmpty()) {
        lines.append(QStringLiteral("How to fix: %1").arg(m_diagnosis.solutionText));
    }
    if (!m_diagnosis.technicalDetails.isEmpty()) {
        lines.append(QStringLiteral("Technical Details:\n%1").arg(m_diagnosis.technicalDetails));
    }
    return lines.join(QStringLiteral("\n\n"));
}

void CrashAssistantDialog::onUploadLog()
{
    if (m_rawLog.trimmed().isEmpty()) {
        QMessageBox::information(this, tr("Empty Log"), tr("No game log output is available to upload."));
        return;
    }
    if (!m_btnUploadLog) {
        return;
    }

    m_btnUploadLog->setEnabled(false);
    const QString originalText = m_btnUploadLog->text();
    m_btnUploadLog->setText(tr("Uploading..."));

    const QPointer<CrashAssistantDialog> self(this);
    const auto guard = qScopeGuard([self] {
        if (self && self->m_btnUploadLog) {
            self->m_btnUploadLog->setEnabled(true);
        }
    });

    const QString fullLog = buildReportText() + QStringLiteral("\n====================================\n\n") + m_rawLog;
    const auto result = GuiUtil::uploadPaste(tr("Minecraft Crash Log"), fullLog, this);
    if (!self || !self->m_btnUploadLog) {
        return;
    }
    if (result.has_value() && !result->isEmpty()) {
        self->m_btnUploadLog->setText(tr("Link Copied!"));
    } else {
        self->m_btnUploadLog->setText(originalText);
    }
}

void CrashAssistantDialog::onCopyDetails()
{
    QApplication::clipboard()->setText(buildReportText());
    QMessageBox::information(this, tr("Copied"), tr("Crash diagnostic report has been copied to your clipboard."));
}
