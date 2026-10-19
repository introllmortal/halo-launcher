// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>
#include <QPointer>
#include <memory>

#include "diagnostics/CrashAnalyzer.h"

class BaseInstance;
class QPushButton;
class QLabel;
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
    void onApplyFix();
    void onSafeMode();
    void onUploadLog();
    void onCopyDetails();

private:
    void setupUi();

    DiagnosisResult m_diagnosis;
    QPointer<BaseInstance> m_instance;
    QString m_rawLog;

    QPushButton* m_btnApplyFix = nullptr;
    QPushButton* m_btnSafeMode = nullptr;
    QPushButton* m_btnUploadLog = nullptr;
    QPushButton* m_btnCopy = nullptr;
    QPushButton* m_btnCancel = nullptr;
};
