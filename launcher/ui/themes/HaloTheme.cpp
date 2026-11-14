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
#include "HaloTheme.h"

#include <QGuiApplication>
#include <QObject>
#include <QStyleHints>

void HaloTheme::apply(bool initial)
{
    FusionTheme::apply(initial);
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    if (auto* hints = QGuiApplication::styleHints()) {
        hints->setColorScheme(Qt::ColorScheme::Dark);
    }
#endif
}

QString HaloTheme::id()
{
    return QStringLiteral("halo");
}

QString HaloTheme::name()
{
    return QObject::tr("Halo Dark");
}

QString HaloTheme::tooltip()
{
    return QObject::tr("Modern dark slate theme for Halo Launcher.");
}

QPalette HaloTheme::colorScheme()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(19, 21, 26));         // #13151a
    p.setColor(QPalette::WindowText, QColor(241, 245, 249));  // #f1f5f9
    p.setColor(QPalette::Base, QColor(24, 27, 34));           // #181b22
    p.setColor(QPalette::AlternateBase, QColor(30, 34, 43));  // #1e222b
    p.setColor(QPalette::ToolTipBase, QColor(30, 34, 43));
    p.setColor(QPalette::ToolTipText, QColor(241, 245, 249));
    p.setColor(QPalette::Text, QColor(241, 245, 249));
    p.setColor(QPalette::Button, QColor(34, 38, 48));         // #222630
    p.setColor(QPalette::ButtonText, QColor(241, 245, 249));
    p.setColor(QPalette::BrightText, QColor(248, 113, 113));
    p.setColor(QPalette::Light, QColor(46, 52, 66));
    p.setColor(QPalette::Midlight, QColor(37, 42, 53));
    p.setColor(QPalette::Mid, QColor(28, 32, 41));
    p.setColor(QPalette::Dark, QColor(14, 16, 21));
    p.setColor(QPalette::Shadow, QColor(9, 11, 14));
    p.setColor(QPalette::Link, QColor(56, 189, 248));           // #38bdf8
    p.setColor(QPalette::LinkVisited, QColor(14, 165, 233));   // #0ea5e9
    p.setColor(QPalette::Highlight, QColor(14, 165, 233));      // #0ea5e9
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    p.setColor(QPalette::PlaceholderText, QColor(100, 116, 139));
    return fadeInactive(p, fadeAmount(), fadeColor());
}

double HaloTheme::fadeAmount()
{
    return 0.45;
}

QColor HaloTheme::fadeColor()
{
    return QColor(19, 21, 26);
}

bool HaloTheme::hasStyleSheet()
{
    return true;
}

QString HaloTheme::appStyleSheet()
{
    return QStringLiteral(R"QSS(
        QMainWindow, QDialog {
            background-color: #13151a;
            color: #f1f5f9;
        }
        QToolTip {
            color: #f1f5f9;
            background-color: #1e222b;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 5px 8px;
        }
        QStatusBar {
            background-color: #15181f;
            color: #94a3b8;
            border-top: 1px solid #252a35;
            padding: 3px 10px;
            min-height: 24px;
        }
        QStatusBar::item {
            border: none;
        }
        QMenuBar {
            background-color: #181b22;
            color: #e2e8f0;
            border-bottom: 1px solid #252a35;
        }
        QMenuBar::item {
            padding: 5px 10px;
            border-radius: 5px;
        }
        QMenuBar::item:selected {
            background-color: #262b36;
        }
        QMenu {
            background-color: #1c2029;
            color: #f1f5f9;
            border: 1px solid #2e3442;
            border-radius: 8px;
            padding: 5px;
        }
        QMenu::item {
            padding: 6px 14px;
            border-radius: 5px;
        }
        QMenu::item:selected {
            background-color: rgba(14, 165, 233, 45);
            color: #ffffff;
        }
        QMenu::item:disabled {
            color: #64748b;
        }
        QMenu::separator {
            height: 1px;
            background: #2b313f;
            margin: 4px 8px;
        }
        QPushButton {
            background-color: #222630;
            color: #f1f5f9;
            border: 1px solid #2e3442;
            border-radius: 7px;
            padding: 6px 14px;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: #2a2f3c;
            border-color: #3d4659;
        }
        QPushButton:pressed {
            background-color: #1c2029;
        }
        QPushButton:disabled {
            background-color: #181b22;
            color: #64748b;
            border-color: #222630;
        }
        QPushButton:default {
            background-color: #0ea5e9;
            color: #ffffff;
            border-color: #0284c7;
            font-weight: 600;
        }
        QPushButton:default:hover {
            background-color: #38bdf8;
        }
        QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #181b22;
            color: #f1f5f9;
            border: 1px solid #2e3442;
            border-radius: 7px;
            padding: 5px 9px;
            selection-background-color: #0ea5e9;
            selection-color: #ffffff;
        }
        QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #0ea5e9;
        }
        QComboBox::drop-down {
            border: none;
            width: 22px;
        }
        QTabWidget::pane {
            border: 1px solid #262b36;
            border-radius: 8px;
            background-color: #161920;
            top: -1px;
        }
        QTabBar::tab {
            background-color: transparent;
            color: #94a3b8;
            padding: 7px 14px;
            margin-right: 4px;
            border-top-left-radius: 7px;
            border-top-right-radius: 7px;
        }
        QTabBar::tab:hover {
            color: #e2e8f0;
            background-color: #1e222b;
        }
        QTabBar::tab:selected {
            color: #ffffff;
            background-color: #222630;
            border-bottom: 2px solid #0ea5e9;
            font-weight: 600;
        }
        QGroupBox {
            border: 1px solid #262b36;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 10px;
            font-weight: 600;
            color: #e2e8f0;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 6px;
        }
        QTreeView, QTableView, QListView {
            background-color: #161920;
            alternate-background-color: #1a1e27;
            border: 1px solid #252a35;
            border-radius: 8px;
            outline: 0;
        }
        QTreeView::item, QListView::item {
            padding: 4px 6px;
            border-radius: 5px;
        }
        QTreeView::item:selected, QTableView::item:selected, QListView::item:selected {
            background-color: rgba(14, 165, 233, 55);
            color: #ffffff;
        }
        QHeaderView::section {
            background-color: #1c2029;
            color: #cbd5e1;
            border: none;
            border-bottom: 1px solid #2a303e;
            border-right: 1px solid #242936;
            padding: 6px 8px;
            font-weight: 600;
        }
        QProgressBar {
            background-color: #1a1d24;
            border: 1px solid #2a303e;
            border-radius: 6px;
            text-align: center;
            color: #f1f5f9;
            min-height: 12px;
        }
        QProgressBar::chunk {
            background-color: #0ea5e9;
            border-radius: 5px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 10px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: #333a4a;
            min-height: 28px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #475269;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: transparent;
            border: none;
            width: 0px;
            height: 0px;
        }
        QScrollBar:horizontal {
            height: 10px;
            margin: 2px;
        }
        QScrollBar::handle:horizontal {
            background: #333a4a;
            min-width: 28px;
            border-radius: 4px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #475269;
        }

        /* Modern MainWindow Shell */
        QWidget#launcherSidebar {
            background-color: #181b22;
            border-right: 1px solid #252a35;
        }
        QLabel#sidebarTitleLabel {
            color: #f8fafc;
            font-size: 15px;
            font-weight: 700;
        }
        QLabel#sidebarVersionLabel {
            color: #64748b;
            font-size: 11px;
        }
        QLabel#sidebarSectionLabel {
            color: #64748b;
            font-size: 11px;
            font-weight: 600;
            padding: 4px 10px 2px 10px;
        }
        QToolButton#sidebarNavButton {
            background-color: transparent;
            color: #cbd5e1;
            border: 1px solid transparent;
            border-radius: 7px;
            padding: 5px 10px;
            font-size: 12px;
            font-weight: 500;
            text-align: left;
        }
        QToolButton#sidebarNavButton::menu-indicator {
            subcontrol-origin: padding;
            subcontrol-position: right center;
            right: 6px;
        }
        QToolButton#sidebarNavButton:hover {
            background-color: #222630;
            color: #f8fafc;
        }
        QToolButton#sidebarNavButton:pressed {
            background-color: #282d39;
        }
        QToolButton#sidebarNavButton:checked,
        QToolButton#sidebarNavButton[active="true"] {
            background-color: rgba(14, 165, 233, 28);
            border: 1px solid rgba(14, 165, 233, 95);
            color: #ffffff;
            font-weight: 600;
        }
        QToolButton#sidebarNavButton:disabled {
            color: #475569;
        }
        QToolButton#sidebarAccountButton {
            background-color: #1e222b;
            color: #f1f5f9;
            border: 1px solid #2a303e;
            border-radius: 8px;
            padding: 6px 9px;
            font-size: 12px;
            font-weight: 600;
            text-align: left;
        }
        QToolButton#sidebarAccountButton:hover {
            background-color: #252a36;
            border-color: #3a4356;
        }
        QToolButton#sidebarAccountButton::menu-indicator {
            subcontrol-position: right center;
            right: 8px;
        }

        QWidget#centerWorkspace {
            background-color: #13151a;
        }
        QWidget#centerTopBar {
            background-color: #15181f;
            border-bottom: 1px solid #222630;
        }
        QLineEdit#instanceSearchEdit {
            background-color: #1c2029;
            color: #f1f5f9;
            border: 1px solid #2a303e;
            border-radius: 8px;
            padding: 6px 12px;
            font-size: 13px;
            min-height: 22px;
        }
        QLineEdit#instanceSearchEdit:focus {
            border-color: #0ea5e9;
            background-color: #1e232d;
        }
        QToolButton#newsPillButton {
            background-color: #1c2029;
            color: #cbd5e1;
            border: 1px solid #2a303e;
            border-radius: 15px;
            padding: 5px 12px;
            font-size: 12px;
        }
        QToolButton#newsPillButton:hover {
            background-color: #252a36;
            color: #ffffff;
            border-color: #3a4356;
        }

        QWidget#instanceInspector {
            background-color: #181b22;
            border-left: 1px solid #252a35;
        }
        QToolButton#inspectorIconButton {
            background-color: #20242e;
            border: 1px solid #2e3442;
            border-radius: 12px;
            padding: 6px;
        }
        QToolButton#inspectorIconButton:hover {
            border-color: #0ea5e9;
            background-color: #252a36;
        }
        QLabel#inspectorTitleLabel {
            color: #f8fafc;
            font-size: 16px;
            font-weight: 700;
        }
        QLabel#inspectorMetaLabel {
            color: #94a3b8;
            font-size: 12px;
        }
        QFrame#inspectorMetaCard {
            background-color: #1e222b;
            border: 1px solid #282d39;
            border-radius: 10px;
        }
        QLabel#inspectorBadgeRunning {
            background-color: rgba(27, 217, 106, 30);
            color: #4ade80;
            border: 1px solid rgba(27, 217, 106, 95);
            border-radius: 9px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#inspectorBadgeUpdate {
            background-color: rgba(14, 165, 233, 30);
            color: #38bdf8;
            border: 1px solid rgba(14, 165, 233, 95);
            border-radius: 9px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: 600;
        }
        QLabel#inspectorBadgeError {
            background-color: rgba(239, 68, 68, 30);
            color: #f87171;
            border: 1px solid rgba(239, 68, 68, 95);
            border-radius: 9px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: 600;
        }
        QToolButton#playPrimaryButton {
            background-color: #16a34a;
            color: #ffffff;
            border: none;
            border-radius: 8px;
            padding: 6px 26px 6px 14px;
            font-size: 13px;
            font-weight: 700;
            min-height: 20px;
        }
        QToolButton#playPrimaryButton:hover {
            background-color: #1db954;
        }
        QToolButton#playPrimaryButton:pressed {
            background-color: #15803d;
        }
        QToolButton#playPrimaryButton:disabled {
            background-color: #1f2924;
            color: #64748b;
        }
        QToolButton#playPrimaryButton::menu-button {
            background-color: rgba(0, 0, 0, 26);
            border-left: 1px solid rgba(255, 255, 255, 55);
            width: 24px;
            border-top-right-radius: 8px;
            border-bottom-right-radius: 8px;
        }
        QToolButton#playPrimaryButton::menu-button:hover {
            background-color: rgba(0, 0, 0, 50);
        }
        QToolButton#playPrimaryButton[running="true"] {
            background-color: #dc2626;
            color: #ffffff;
            padding: 6px 14px;
        }
        QToolButton#playPrimaryButton[running="true"]:hover {
            background-color: #ef4444;
        }
        QToolButton#inspectorActionButton, QPushButton#inspectorActionButton {
            background-color: #222630;
            color: #e2e8f0;
            border: 1px solid #2e3442;
            border-radius: 7px;
            padding: 5px 8px;
            font-size: 12px;
            font-weight: 500;
            text-align: left;
            min-height: 16px;
        }
        QToolButton#inspectorActionButton::menu-indicator {
            subcontrol-origin: padding;
            subcontrol-position: right center;
            right: 6px;
        }
        QToolButton#inspectorActionButton:hover, QPushButton#inspectorActionButton:hover {
            background-color: #2a2f3c;
            border-color: #3d4659;
            color: #ffffff;
        }
        QToolButton#inspectorActionButton:disabled, QPushButton#inspectorActionButton:disabled {
            color: #475569;
            background-color: #1a1d24;
            border-color: #222630;
        }
        QPushButton#inspectorDangerButton {
            background-color: #271a1e;
            color: #fca5a5;
            border: 1px solid #4c232b;
            border-radius: 7px;
            padding: 5px 10px;
            font-size: 12px;
            font-weight: 600;
            min-height: 16px;
        }
        QPushButton#inspectorDangerButton:hover {
            background-color: #3b2027;
            border-color: #7f2937;
            color: #fecaca;
        }
    )QSS");
}
