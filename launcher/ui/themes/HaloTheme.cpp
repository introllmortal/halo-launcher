// SPDX-License-Identifier: GPL-3.0-only
#include "HaloTheme.h"

#include <QObject>

QString HaloTheme::id()
{
    return "halo";
}

QString HaloTheme::name()
{
    return QObject::tr("Halo Dark");
}

QPalette HaloTheme::colorScheme()
{
    QPalette haloPalette;
    // Deep obsidian background
    haloPalette.setColor(QPalette::Window, QColor(24, 27, 34));
    haloPalette.setColor(QPalette::WindowText, QColor(241, 245, 249));
    haloPalette.setColor(QPalette::Base, QColor(19, 22, 28));
    haloPalette.setColor(QPalette::AlternateBase, QColor(30, 34, 43));
    haloPalette.setColor(QPalette::ToolTipBase, QColor(15, 23, 42));
    haloPalette.setColor(QPalette::ToolTipText, QColor(248, 250, 252));
    haloPalette.setColor(QPalette::Text, QColor(241, 245, 249));
    haloPalette.setColor(QPalette::Button, QColor(35, 39, 50));
    haloPalette.setColor(QPalette::ButtonText, QColor(241, 245, 249));
    haloPalette.setColor(QPalette::BrightText, QColor(244, 63, 94));
    haloPalette.setColor(QPalette::Link, QColor(56, 189, 248));
    // Halo Refined Deep Sky Accent
    haloPalette.setColor(QPalette::Highlight, QColor(2, 132, 199));
    haloPalette.setColor(QPalette::HighlightedText, Qt::white);
    haloPalette.setColor(QPalette::PlaceholderText, QColor(100, 116, 139));
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
    return R"(
        QToolTip {
            color: #f8fafc;
            background-color: #0f172a;
            border: 1px solid #0284c7;
            border-radius: 4px;
            padding: 4px;
        }
        QAbstractItemView {
            selection-background-color: transparent;
        }
        QAbstractItemView::item:selected {
            background-color: rgba(2, 132, 199, 0.28);
            color: #ffffff;
            border: 1px solid rgba(2, 132, 199, 0.65);
            border-radius: 4px;
        }
        QAbstractItemView::item:selected:hover {
            background-color: rgba(2, 132, 199, 0.38);
        }
        QAbstractItemView::item:hover:!selected {
            background-color: rgba(255, 255, 255, 0.05);
            border-radius: 4px;
        }
    )";
}

QString HaloTheme::tooltip()
{
    return QObject::tr("Default modern dark theme with refined sky accents");
}
