// SPDX-License-Identifier: GPL-3.0-only
#include "CrashAssistantDialog.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "settings/SettingsObject.h"
#include "ui/GuiUtil.h"

CrashAssistantDialog::CrashAssistantDialog(const DiagnosisResult& diagnosis,
                                           BaseInstance* instance,
                                           const QString& rawLog,
                                           QWidget* parent)
    : QDialog(parent),
      m_diagnosis(diagnosis),
      m_instance(instance),
      m_rawLog(rawLog)
{
    setupUi();
}

void CrashAssistantDialog::setupUi()
{
    setWindowTitle(tr("Halo Crash Assistant"));
    resize(620, 460);
    setMinimumSize(560, 420);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Header with Icon and Title
    auto headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);

    auto iconLabel = new QLabel(this);
    QIcon icon = QIcon::fromTheme("dialog-warning");
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    }
    iconLabel->setPixmap(icon.pixmap(48, 48));
    iconLabel->setAlignment(Qt::AlignTop);
    headerLayout->addWidget(iconLabel);

    auto titleLayout = new QVBoxLayout();
    auto titleLabel = new QLabel(m_diagnosis.title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    titleLayout->addWidget(titleLabel);

    auto subtitleLabel = new QLabel(tr("Halo Launcher analyzed the game crash and determined the cause:"), this);
    subtitleLabel->setStyleSheet("color: #888888;");
    titleLayout->addWidget(subtitleLabel);

    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    // Problem Description
    auto descLabel = new QLabel(m_diagnosis.description, this);
    descLabel->setWordWrap(true);
    descLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(descLabel);

    // Recommended Solution Frame
    auto solutionFrame = new QFrame(this);
    solutionFrame->setFrameShape(QFrame::StyledPanel);
    solutionFrame->setStyleSheet("QFrame { background-color: rgba(2, 132, 199, 0.1); border: 1px solid rgba(2, 132, 199, 0.4); border-radius: 6px; padding: 8px; }");
    auto solutionLayout = new QVBoxLayout(solutionFrame);
    solutionLayout->setSpacing(4);

    auto solutionHeader = new QLabel(tr("💡 Recommended Solution:"), solutionFrame);
    QFont solHeaderFont = solutionHeader->font();
    solHeaderFont.setBold(true);
    solutionHeader->setFont(solHeaderFont);
    solutionLayout->addWidget(solutionHeader);

    auto solutionBody = new QLabel(m_diagnosis.solutionText, solutionFrame);
    solutionBody->setWordWrap(true);
    solutionLayout->addWidget(solutionBody);

    mainLayout->addWidget(solutionFrame);

    // Technical Details
    if (!m_diagnosis.technicalDetails.isEmpty()) {
        auto techHeader = new QLabel(tr("Technical Details:"), this);
        QFont techFont = techHeader->font();
        techFont.setBold(true);
        techHeader->setFont(techFont);
        mainLayout->addWidget(techHeader);

        auto techEdit = new QTextEdit(this);
        techEdit->setReadOnly(true);
        techEdit->setFontFamily("Courier New");
        techEdit->setText(m_diagnosis.technicalDetails);
        techEdit->setMaximumHeight(100);
        mainLayout->addWidget(techEdit);
    }

    // Button Bar
    auto buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_btnUploadLog = new QPushButton(tr("Upload Log to mclo.gs"), this);
    connect(m_btnUploadLog, &QPushButton::clicked, this, &CrashAssistantDialog::onUploadLog);
    buttonLayout->addWidget(m_btnUploadLog);

    m_btnCopy = new QPushButton(tr("Copy Details"), this);
    connect(m_btnCopy, &QPushButton::clicked, this, &CrashAssistantDialog::onCopyDetails);
    buttonLayout->addWidget(m_btnCopy);

    buttonLayout->addStretch(1);

    // Cancel Button (Dismiss without changes so player can fix manually)
    m_btnCancel = new QPushButton(tr("Cancel"), this);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_btnCancel);

    // Safe Mode button to quickly recover from crash loops
    m_btnSafeMode = new QPushButton(tr("Safe Mode"), this);
    m_btnSafeMode->setToolTip(tr("Temporarily reset active shaders, resource packs, and JVM overrides to recover from crashes."));
    connect(m_btnSafeMode, &QPushButton::clicked, this, &CrashAssistantDialog::onSafeMode);
    buttonLayout->addWidget(m_btnSafeMode);

    // Primary Fix Action Button
    if (m_diagnosis.hasFix() && !m_diagnosis.actionButtonText.isEmpty()) {
        m_btnApplyFix = new QPushButton(m_diagnosis.actionButtonText, this);
        m_btnApplyFix->setDefault(true);
        m_btnApplyFix->setStyleSheet("QPushButton { font-weight: bold; padding: 6px 14px; }");
        connect(m_btnApplyFix, &QPushButton::clicked, this, &CrashAssistantDialog::onApplyFix);
        buttonLayout->addWidget(m_btnApplyFix);
    }

    mainLayout->addLayout(buttonLayout);
}

void CrashAssistantDialog::onApplyFix()
{
    switch (m_diagnosis.fixAction) {
        case RecommendedFix::IncreaseRam: {
            if (m_instance && m_instance->settings()) {
                m_instance->settings()->set("OverrideMemory", true);
                m_instance->settings()->set("MaxMemAlloc", m_diagnosis.suggestedRamMb);
                m_instance->saveNow();
                QMessageBox::information(this, tr("RAM Updated"),
                    tr("Allocated RAM has been increased to %1 MB!\nYou can now relaunch Minecraft.")
                    .arg(m_diagnosis.suggestedRamMb));
                accept();
            }
            break;
        }
        case RecommendedFix::DisableMod: {
            if (!m_diagnosis.offendingModPath.isEmpty() && QFile::exists(m_diagnosis.offendingModPath)) {
                QString disabledPath = m_diagnosis.offendingModPath + ".disabled";
                if (QFile::exists(disabledPath)) {
                    QFile::remove(disabledPath);
                }
                if (QFile::rename(m_diagnosis.offendingModPath, disabledPath)) {
                    QMessageBox::information(this, tr("Mod Disabled"),
                        tr("Conflicting mod has been disabled:\n%1\nYou can now relaunch Minecraft.")
                        .arg(m_diagnosis.offendingModName));
                    accept();
                } else {
                    QMessageBox::warning(this, tr("Error"), tr("Could not disable mod file. Please check file permissions."));
                }
            }
            break;
        }
        case RecommendedFix::DeleteConfig: {
            if (!m_diagnosis.offendingConfigPath.isEmpty() && QFile::exists(m_diagnosis.offendingConfigPath)) {
                QString backupPath = m_diagnosis.offendingConfigPath + ".backup";
                if (QFile::exists(backupPath)) {
                    QFile::remove(backupPath);
                }
                QFile::rename(m_diagnosis.offendingConfigPath, backupPath);
                QMessageBox::information(this, tr("Config Reset"),
                    tr("The corrupted config file has been backed up and reset.\nMinecraft will recreate a clean default config upon next launch."));
                accept();
            }
            break;
        }
        case RecommendedFix::SwitchJava: {
            QMessageBox::information(this, tr("Java Setting"),
                tr("Please configure Java %1 in instance settings under Settings -> Java.")
                .arg(m_diagnosis.requiredJavaVersion));
            accept();
            break;
        }
        case RecommendedFix::DisableShaders: {
            if (m_instance) {
                QString optionsShaders = FS::PathCombine(m_instance->gameRoot(), "optionsshaders.txt");
                if (QFile::exists(optionsShaders)) {
                    QFile::remove(optionsShaders);
                }
                QString irisProps = FS::PathCombine(m_instance->gameRoot(), "config/iris.properties");
                if (QFile::exists(irisProps)) {
                    QFile::remove(irisProps);
                }
                QString oculusProps = FS::PathCombine(m_instance->gameRoot(), "config/oculus.properties");
                if (QFile::exists(oculusProps)) {
                    QFile::remove(oculusProps);
                }
                QMessageBox::information(this, tr("Shaders Disabled"),
                    tr("Active shaders have been reset to prevent graphics driver crashes."));
                accept();
            }
            break;
        }
        case RecommendedFix::UploadLog: {
            onUploadLog();
            break;
        }
        case RecommendedFix::None:
            break;
    }
}

void CrashAssistantDialog::onSafeMode()
{
    if (!m_instance) {
        QMessageBox::warning(this, tr("Warning"), tr("Instance is no longer available."));
        return;
    }

    // 1. Reset active shader configurations
    QString optionsShaders = FS::PathCombine(m_instance->gameRoot(), "optionsshaders.txt");
    if (QFile::exists(optionsShaders)) {
        QFile::remove(optionsShaders);
    }
    QString irisProps = FS::PathCombine(m_instance->gameRoot(), "config/iris.properties");
    if (QFile::exists(irisProps)) {
        QFile::remove(irisProps);
    }
    QString oculusProps = FS::PathCombine(m_instance->gameRoot(), "config/oculus.properties");
    if (QFile::exists(oculusProps)) {
        QFile::remove(oculusProps);
    }

    // 2. Clear enabled resource packs in options.txt
    QString optionsPath = FS::PathCombine(m_instance->gameRoot(), "options.txt");
    if (QFile::exists(optionsPath)) {
        QFile optFile(optionsPath);
        if (optFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = QString::fromUtf8(optFile.readAll());
            optFile.close();
            static const QRegularExpression rpRegex(R"(resourcePacks:\[.*\])");
            content.replace(rpRegex, "resourcePacks:[]");
            if (optFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                optFile.write(content.toUtf8());
                optFile.close();
            }
        }
    }

    // 3. Reset hazardous JVM overrides
    if (m_instance->settings()) {
        m_instance->settings()->set("OverrideJavaArgs", false);
        m_instance->saveNow();
    }

    QMessageBox::information(this, tr("Safe Mode Prepared"),
        tr("Shaders, resource packs, and custom Java parameters have been safely reset.\nYou can now relaunch Minecraft."));
    accept();
}

void CrashAssistantDialog::onUploadLog()
{
    if (m_rawLog.trimmed().isEmpty()) {
        QMessageBox::information(this, tr("Empty Log"), tr("No game log output is available to upload."));
        return;
    }
    GuiUtil::uploadPaste(tr("Minecraft Crash Log"), m_rawLog, this);
}

void CrashAssistantDialog::onCopyDetails()
{
    QString text = QString("=== Halo Crash Assistant Report ===\n"
                           "Issue: %1\n\n"
                           "Description:\n%2\n\n"
                           "Solution:\n%3\n\n"
                           "Technical Details:\n%4\n")
                       .arg(m_diagnosis.title)
                       .arg(m_diagnosis.description)
                       .arg(m_diagnosis.solutionText)
                       .arg(m_diagnosis.technicalDetails);

    QApplication::clipboard()->setText(text);
    QMessageBox::information(this, tr("Copied"), tr("Crash diagnostic report has been copied to your clipboard."));
}
