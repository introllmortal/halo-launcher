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

#include <QDialog>
#include <QPointer>

#include "diagnostics/CrashAnalyzer.h"

class BaseInstance;
class QPushButton;
class QTextEdit;

class CrashAssistantDialog : public QDialog {
    Q_OBJECT

   public:
    explicit CrashAssistantDialog(const DiagnosisResult& diagnosis,
                                  BaseInstance* instance,
                                  const QString& rawLog,
                                  QWidget* parent = nullptr);
    ~CrashAssistantDialog() override = default;

   private slots:
    void onOpenFolder();
    void onUploadLog();
    void onCopyDetails();
    void onToggleTechnicalDetails(bool expanded);

   private:
    void setupUi();
    QString resolveHelpfulFolder() const;
    QString buildReportText() const;

    DiagnosisResult m_diagnosis;
    QPointer<BaseInstance> m_instance;
    QString m_rawLog;

    QPushButton* m_btnDetailsToggle = nullptr;
    QTextEdit* m_techEdit = nullptr;
    QPushButton* m_btnOpenFolder = nullptr;
    QPushButton* m_btnUploadLog = nullptr;
    QPushButton* m_btnCopy = nullptr;
    QPushButton* m_btnClose = nullptr;
};
