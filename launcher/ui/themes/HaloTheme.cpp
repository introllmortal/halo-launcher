// SPDX-License-Identifier: GPL-3.0-only
#include "HaloTheme.h"

#include <QObject>

HaloTheme::HaloTheme(HaloAccent accent) : m_accent(accent) {}

QColor HaloTheme::accentColor() const
{
    switch (m_accent) {
        case HaloAccent::Emerald: return QColor(16, 185, 129); // #10b981
        case HaloAccent::Amethyst: return QColor(139, 92, 246); // #8b5cf6
        case HaloAccent::Amber: return QColor(245, 158, 11); // #f59e0b
        case HaloAccent::Cyan:
        default:
            return QColor(14, 165, 233); // #0ea5e9
    }
}

QColor HaloTheme::accentLightColor() const
{
    switch (m_accent) {
        case HaloAccent::Emerald: return QColor(52, 211, 153); // #34d399
        case HaloAccent::Amethyst: return QColor(167, 139, 250); // #a78bfa
        case HaloAccent::Amber: return QColor(251, 191, 36); // #fbbf24
        case HaloAccent::Cyan:
        default:
            return QColor(56, 189, 248); // #38bdf8
    }
}

QString HaloTheme::accentColorHex() const
{
    return accentColor().name();
}

QString HaloTheme::accentLightHex() const
{
    return accentLightColor().name();
}

QString HaloTheme::id()
{
    switch (m_accent) {
        case HaloAccent::Emerald: return "halo-emerald";
        case HaloAccent::Amethyst: return "halo-amethyst";
        case HaloAccent::Amber: return "halo-amber";
        case HaloAccent::Cyan:
        default:
            return "halo";
    }
}

QString HaloTheme::name()
{
    switch (m_accent) {
        case HaloAccent::Emerald: return QObject::tr("Halo (Emerald)");
        case HaloAccent::Amethyst: return QObject::tr("Halo (Amethyst)");
        case HaloAccent::Amber: return QObject::tr("Halo (Amber)");
        case HaloAccent::Cyan:
        default:
            return QObject::tr("Halo (Electric Cyan)");
    }
}

QPalette HaloTheme::colorScheme()
{
    QPalette haloPalette;
    // Deep obsidian background
    haloPalette.setColor(QPalette::Window, QColor(24, 27, 34));
    haloPalette.setColor(QPalette::WindowText, QColor(241, 245, 249));
    haloPalette.setColor(QPalette::Base, QColor(28, 32, 42));
    haloPalette.setColor(QPalette::AlternateBase, QColor(34, 39, 51));
    haloPalette.setColor(QPalette::ToolTipBase, QColor(15, 23, 42));
    haloPalette.setColor(QPalette::ToolTipText, QColor(248, 250, 252));
    haloPalette.setColor(QPalette::Text, QColor(241, 245, 249));
    haloPalette.setColor(QPalette::Button, QColor(36, 42, 54));
    haloPalette.setColor(QPalette::ButtonText, QColor(241, 245, 249));
    haloPalette.setColor(QPalette::BrightText, QColor(244, 63, 94));
    haloPalette.setColor(QPalette::Link, accentLightColor());
    haloPalette.setColor(QPalette::Highlight, accentColor());
    haloPalette.setColor(QPalette::HighlightedText, Qt::white);
    haloPalette.setColor(QPalette::PlaceholderText, QColor(148, 163, 184));
    haloPalette.setColor(QPalette::Midlight, QColor(56, 64, 82));
    haloPalette.setColor(QPalette::Mid, QColor(71, 85, 105));
    haloPalette.setColor(QPalette::Dark, QColor(30, 36, 47));
    haloPalette.setColor(QPalette::Shadow, QColor(10, 12, 16));
    return fadeInactive(haloPalette, fadeAmount(), fadeColor());
}

double HaloTheme::fadeAmount()
{
    return 0.5;
}

QColor HaloTheme::fadeColor()
{
    return QColor(24, 27, 34);
}

bool HaloTheme::hasStyleSheet()
{
    return true;
}

QString HaloTheme::appStyleSheet()
{
    QString prim = accentColorHex();
    QString light = accentLightHex();
    QString rgba = QString("%1, %2, %3").arg(accentColor().red()).arg(accentColor().green()).arg(accentColor().blue());

    return QString(R"(
        QToolTip {
            color: #f8fafc;
            background-color: #0f172a;
            border: 1px solid %1;
            border-radius: 4px;
            padding: 4px 6px;
        }
        QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
            background-color: #1e222b;
            border: 1px solid #3b4252;
            border-radius: 4px;
            padding: 3px 6px;
            color: #f1f5f9;
        }
        QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
            border: 1px solid %1;
            background-color: #242a36;
        }
        QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover {
            border: 1px solid #64748b;
        }
        QLineEdit:read-only, QTextEdit:read-only, QPlainTextEdit:read-only {
            background-color: #161920;
            border: 1px solid #2a303f;
            color: #94a3b8;
        }
        QCheckBox {
            color: #f1f5f9;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 15px;
            height: 15px;
            background-color: #1e222b;
            border: 1.5px solid #64748b;
            border-radius: 3px;
        }
        QCheckBox::indicator:hover {
            border: 1.5px solid %1;
        }
        QCheckBox::indicator:checked {
            background-color: %1;
            border: 1.5px solid %2;
            image: url(:/icons/flat_white/scalable/status-good.svg);
        }
        QCheckBox::indicator:disabled {
            background-color: #161920;
            border: 1.5px solid #333a4d;
        }
        QRadioButton {
            color: #f1f5f9;
            spacing: 6px;
        }
        QRadioButton::indicator {
            width: 15px;
            height: 15px;
            background-color: #1e222b;
            border: 1.5px solid #64748b;
            border-radius: 8px;
        }
        QRadioButton::indicator:hover {
            border: 1.5px solid %1;
        }
        QRadioButton::indicator:checked {
            background-color: %1;
            border: 1.5px solid %2;
        }
        QComboBox {
            background-color: #1e222b;
            border: 1px solid #3b4252;
            border-radius: 4px;
            padding: 3px 8px;
            color: #f1f5f9;
        }
        QComboBox:hover {
            border: 1px solid #64748b;
        }
        QComboBox:focus {
            border: 1px solid %1;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox QAbstractItemView {
            background-color: #181b22;
            border: 1px solid #3b4252;
            selection-background-color: rgba(%3, 0.45);
            selection-color: #ffffff;
            color: #f1f5f9;
        }
        QGroupBox {
            border: 1px solid #333a4d;
            border-radius: 6px;
            margin-top: 12px;
            padding-top: 12px;
            color: #94a3b8;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 4px;
            background-color: #181b22;
        }
        QTabBar::tab {
            background-color: #1e222b;
            color: #94a3b8;
            border: 1px solid #333a4d;
            padding: 6px 14px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:hover {
            color: #f1f5f9;
            border-color: #475569;
        }
        QTabBar::tab:selected {
            background-color: #242a36;
            color: %2;
            border-color: %1;
            border-bottom-color: #242a36;
            font-weight: bold;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #333a4d;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover {
            background: #475569;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QAbstractItemView::item:selected {
            background-color: rgba(%3, 0.35);
            color: #ffffff;
            border: 1px solid rgba(%3, 0.70);
            border-radius: 4px;
        }
        QAbstractItemView::item:selected:hover {
            background-color: rgba(%3, 0.45);
        }
        QAbstractItemView::item:hover:!selected {
            background-color: rgba(255, 255, 255, 0.06);
            border-radius: 4px;
        }
    )").arg(prim, light, rgba);
}

QString HaloTheme::tooltip()
{
    switch (m_accent) {
        case HaloAccent::Emerald: return QObject::tr("Halo obsidian theme with emerald green accents");
        case HaloAccent::Amethyst: return QObject::tr("Halo obsidian theme with vibrant amethyst violet accents");
        case HaloAccent::Amber: return QObject::tr("Halo obsidian theme with warm amber gold accents");
        case HaloAccent::Cyan:
        default:
            return QObject::tr("Default modern dark theme with refined sky cyan accents");
    }
}

LogColors HaloTheme::logColorScheme()
{
    LogColors colors;
    colors.background[MessageLevel::Fatal] = QColor(63, 18, 25);
    colors.foreground[MessageLevel::Fatal] = QColor(255, 77, 109);
    colors.foreground[MessageLevel::Error] = QColor(248, 113, 113);
    colors.foreground[MessageLevel::Warning] = QColor(251, 191, 36);
    colors.foreground[MessageLevel::Launcher] = accentLightColor();
    colors.foreground[MessageLevel::Debug] = QColor(148, 163, 184);
    colors.foreground[MessageLevel::StdOut] = QColor(226, 232, 240);
    colors.foreground[MessageLevel::StdErr] = QColor(252, 165, 165);
    return colors;
}
