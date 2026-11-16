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
#include "LaunchPreflightDialog.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

LaunchPreflightDialog::LaunchPreflightDialog(const PreflightReport& report, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Pre-Launch Check"));
    setMinimumSize(520, 300);
    resize(560, 360);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(12);

    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(10);
    auto* iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("status-yellow")).pixmap(30, 30));
    headerRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* titleCol = new QVBoxLayout();
    titleCol->setSpacing(2);
    auto* heading = new QLabel(tr("Potential issues detected before launch"), this);
    QFont headingFont = heading->font();
    headingFont.setBold(true);
    headingFont.setPointSize(headingFont.pointSize() + 2);
    heading->setFont(headingFont);
    titleCol->addWidget(heading);

    auto* subtitle = new QLabel(tr("Review the checks below or apply recommended automatic fixes."), this);
    subtitle->setObjectName(QStringLiteral("inspectorMetaLabel"));
    subtitle->setWordWrap(true);
    titleCol->addWidget(subtitle);
    headerRow->addLayout(titleCol, 1);
    root->addLayout(headerRow);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* listHost = new QWidget(scroll);
    auto* listLayout = new QVBoxLayout(listHost);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(8);

    for (const PreflightIssue& issue : report.issues) {
        auto* card = new QFrame(listHost);
        card->setObjectName(QStringLiteral("inspectorMetaCard"));
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(4);

        auto* titleRow = new QHBoxLayout();
        titleRow->setSpacing(8);
        auto* badge = new QLabel(
            issue.severity == PreflightSeverity::Critical ? tr("Critical") : tr("Warning"),
            card);
        badge->setObjectName(issue.severity == PreflightSeverity::Critical
                                 ? QStringLiteral("inspectorBadgeError")
                                 : QStringLiteral("inspectorBadgeUpdate"));
        titleRow->addWidget(badge, 0);

        auto* issueTitle = new QLabel(issue.title, card);
        QFont titleFont = issueTitle->font();
        titleFont.setBold(true);
        issueTitle->setFont(titleFont);
        issueTitle->setWordWrap(true);
        titleRow->addWidget(issueTitle, 1);
        cardLayout->addLayout(titleRow);

        if (!issue.description.isEmpty()) {
            auto* desc = new QLabel(issue.description, card);
            desc->setObjectName(QStringLiteral("inspectorMetaLabel"));
            desc->setWordWrap(true);
            cardLayout->addWidget(desc);
        }
        if (!issue.fixSummary.isEmpty()) {
            auto* fixLabel = new QLabel(tr("Auto-fix: %1").arg(issue.fixSummary), card);
            fixLabel->setStyleSheet(QStringLiteral("color: #38bdf8; font-size: 12px; font-weight: 600;"));
            fixLabel->setWordWrap(true);
            cardLayout->addWidget(fixLabel);
        }
        listLayout->addWidget(card);
    }
    listLayout->addStretch(1);
    scroll->setWidget(listHost);
    root->addWidget(scroll, 1);

    auto* buttons = new QHBoxLayout();
    buttons->setSpacing(8);

    auto* cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        m_action = ResultAction::Cancel;
        reject();
    });
    buttons->addWidget(cancelBtn);

    buttons->addStretch(1);

    auto* launchAnywayBtn = new QPushButton(tr("Launch Anyway"), this);
    connect(launchAnywayBtn, &QPushButton::clicked, this, [this]() {
        m_action = ResultAction::LaunchAnyway;
        accept();
    });
    buttons->addWidget(launchAnywayBtn);

    if (report.hasFixableIssues()) {
        auto* fixLaunchBtn = new QPushButton(tr("Fix & Launch"), this);
        fixLaunchBtn->setDefault(true);
        connect(fixLaunchBtn, &QPushButton::clicked, this, [this]() {
            m_action = ResultAction::FixAndLaunch;
            accept();
        });
        buttons->addWidget(fixLaunchBtn);
    } else {
        launchAnywayBtn->setDefault(true);
    }

    root->addLayout(buttons);
}
