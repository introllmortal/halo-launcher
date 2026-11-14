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

#include <QFile>
#include <QGuiApplication>
#include <QObject>
#include <QStyleHints>

static void ensureHaloResources()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        Q_INIT_RESOURCE(halo);
    }
}

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
    return QObject::tr("High-contrast indigo slate theme for Halo Launcher.");
}

QPalette HaloTheme::colorScheme()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(11, 14, 21));         // #0b0e15
    p.setColor(QPalette::WindowText, QColor(241, 245, 249));  // #f1f5f9
    p.setColor(QPalette::Base, QColor(18, 22, 34));           // #121622
    p.setColor(QPalette::AlternateBase, QColor(23, 28, 43));  // #171c2b
    p.setColor(QPalette::ToolTipBase, QColor(27, 34, 52));    // #1b2234
    p.setColor(QPalette::ToolTipText, QColor(248, 250, 252));
    p.setColor(QPalette::Text, QColor(241, 245, 249));
    p.setColor(QPalette::Button, QColor(33, 41, 62));         // #21293e
    p.setColor(QPalette::ButtonText, QColor(241, 245, 249));
    p.setColor(QPalette::BrightText, QColor(248, 113, 113));
    p.setColor(QPalette::Light, QColor(57, 71, 104));          // #394768
    p.setColor(QPalette::Midlight, QColor(43, 53, 79));
    p.setColor(QPalette::Mid, QColor(28, 35, 52));
    p.setColor(QPalette::Dark, QColor(9, 12, 18));
    p.setColor(QPalette::Shadow, QColor(5, 7, 11));
    p.setColor(QPalette::Link, QColor(56, 189, 248));           // #38bdf8
    p.setColor(QPalette::LinkVisited, QColor(14, 165, 233));   // #0ea5e9
    p.setColor(QPalette::Highlight, QColor(2, 132, 199));      // #0284c7
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    p.setColor(QPalette::PlaceholderText, QColor(124, 140, 166));

    p = fadeInactive(p, fadeAmount(), fadeColor());
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(148, 163, 184));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(148, 163, 184));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(115, 130, 155));
    p.setColor(QPalette::Disabled, QPalette::Button, QColor(20, 25, 38));
    p.setColor(QPalette::Disabled, QPalette::Base, QColor(16, 20, 31));
    return p;
}

double HaloTheme::fadeAmount()
{
    return 0.28;
}

QColor HaloTheme::fadeColor()
{
    return QColor(11, 14, 21);
}

bool HaloTheme::hasStyleSheet()
{
    return true;
}

QString HaloTheme::appStyleSheet()
{
    ensureHaloResources();
    QFile file(QStringLiteral(":/halo/halo.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}
