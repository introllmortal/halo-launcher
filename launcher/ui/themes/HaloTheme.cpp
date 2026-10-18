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
    // Signature Halo Electric Cyan Accent
    haloPalette.setColor(QPalette::Highlight, QColor(14, 165, 233));
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
    return "QToolTip { color: #f8fafc; background-color: #0f172a; border: 1px solid #0ea5e9; border-radius: 4px; padding: 4px; }";
}

QString HaloTheme::tooltip()
{
    return QObject::tr("Default modern dark theme with electric cyan accents");
}
