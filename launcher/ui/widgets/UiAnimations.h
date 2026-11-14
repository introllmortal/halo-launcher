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

#include <QPixmap>
#include <QPoint>
#include <QPointer>
#include <QWidget>

#include <optional>

class QAbstractButton;
class QVariantAnimation;

namespace UiAnimations {

/// Returns true when both Halo Launcher settings and OS animation settings allow UI motion.
bool animationsEnabled();

/// Optional override used by automated tests to control animationsEnabled() deterministically.
void setAnimationsEnabledOverride(std::optional<bool> enabled);

/// Smoothly reveals or collapses a child widget vertically via maximumHeight, restoring QWIDGETSIZE_MAX when done.
void animateVisibleHeight(QWidget* target, bool show, int durationMs = 160);

/// Installs a lightweight translucent hover-glow overlay on a button without disrupting QSS styling or input.
void installButtonHoverGlow(QAbstractButton* button, qreal borderRadius = 8.0, int durationMs = 140);

/// Lightweight transient overlay that cross-fades and slides out a grabbed snapshot over a container.
class TransitionOverlay final : public QWidget {
    Q_OBJECT

   public:
    static void start(QWidget* container, const QPixmap& previousSnapshot, const QPoint& slideOffset = QPoint(0, 6), int durationMs = 130);

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

   private:
    explicit TransitionOverlay(QWidget* container, QPixmap snapshot, QPoint slideOffset, int durationMs);
    void finishImmediately();

    QPointer<QWidget> m_container;
    QPixmap m_snapshot;
    QPoint m_slideOffset;
    QVariantAnimation* m_anim = nullptr;
    qreal m_progress = 0.0;
    bool m_finished = false;
};

}  // namespace UiAnimations
