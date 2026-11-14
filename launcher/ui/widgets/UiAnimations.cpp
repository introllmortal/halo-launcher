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
#include "UiAnimations.h"

#include <QAbstractButton>
#include <QEasingCurve>
#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPropertyAnimation>
#include <QVariantAnimation>

#include "Application.h"
#include "settings/SettingsObject.h"

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace UiAnimations {

namespace {
std::optional<bool> g_animationsOverride;
constexpr auto kHeightAnimObjectName = "_haloHeightAnim";
constexpr auto kOverlayObjectName = "HaloTransitionOverlay";
constexpr auto kGlowOverlayObjectName = "HaloHoverGlowOverlay";

class HoverGlowOverlay final : public QWidget {
   public:
    explicit HoverGlowOverlay(QAbstractButton* button, qreal radius, int durationMs)
        : QWidget(button), m_button(button), m_radius(qMax<qreal>(0.0, radius)), m_durationMs(qMax(1, durationMs))
    {
        setObjectName(kGlowOverlayObjectName);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setGeometry(button->rect());
        hide();

        m_anim = new QVariantAnimation(this);
        m_anim->setEasingCurve(QEasingCurve::InOutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_progress = value.toReal();
            update();
        });
        connect(m_anim, &QVariantAnimation::finished, this, [this]() {
            if (m_progress <= 0.005) {
                m_progress = 0.0;
                hide();
            }
        });

        button->installEventFilter(this);
    }

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != m_button) {
            return QWidget::eventFilter(watched, event);
        }
        switch (event->type()) {
            case QEvent::Resize:
                setGeometry(m_button->rect());
                break;
            case QEvent::Enter:
                animateTo(m_button->isEnabled() ? 1.0 : 0.0);
                break;
            case QEvent::Leave:
                animateTo(0.0);
                break;
            case QEvent::EnabledChange:
                if (!m_button->isEnabled()) {
                    resetImmediately();
                }
                break;
            case QEvent::Hide:
                resetImmediately();
                break;
            default:
                break;
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent*) override
    {
        if (m_progress <= 0.005 || !m_button || !m_button->isEnabled() || !animationsEnabled()) {
            return;
        }
        const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        if (bounds.isEmpty()) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        QLinearGradient sheen(bounds.topLeft(), bounds.bottomLeft());
        sheen.setColorAt(0.0, QColor(255, 255, 255, qRound(26.0 * m_progress)));
        sheen.setColorAt(1.0, QColor(255, 255, 255, qRound(6.0 * m_progress)));
        painter.setPen(QPen(QColor(255, 255, 255, qRound(44.0 * m_progress)), 1.0));
        painter.setBrush(sheen);
        painter.drawRoundedRect(bounds, m_radius, m_radius);
    }

   private:
    void animateTo(qreal target)
    {
        if (!animationsEnabled() || !m_button || !m_button->isVisible()) {
            resetImmediately();
            return;
        }
        if (qFuzzyCompare(m_progress + 1.0, target + 1.0) && m_anim->state() != QAbstractAnimation::Running) {
            if (target <= 0.0) {
                hide();
            }
            return;
        }
        m_anim->stop();
        if (target > 0.0) {
            setGeometry(m_button->rect());
            show();
            raise();
        }
        const int scaledDuration = qMax(16, qRound(static_cast<qreal>(m_durationMs) * qAbs(target - m_progress)));
        m_anim->setDuration(scaledDuration);
        m_anim->setStartValue(m_progress);
        m_anim->setEndValue(target);
        m_anim->start();
    }

    void resetImmediately()
    {
        if (m_anim) {
            m_anim->stop();
        }
        m_progress = 0.0;
        hide();
    }

    QPointer<QAbstractButton> m_button;
    qreal m_radius = 8.0;
    int m_durationMs = 140;
    QVariantAnimation* m_anim = nullptr;
    qreal m_progress = 0.0;
};

bool osClientAreaAnimationsEnabled()
{
#ifdef Q_OS_WIN
    BOOL enabled = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0)) {
        return enabled != FALSE;
    }
#endif
    return true;
}

void cancelActiveHeightAnimation(QWidget* target)
{
    if (auto* existing = target->findChild<QPropertyAnimation*>(kHeightAnimObjectName, Qt::FindDirectChildrenOnly)) {
        existing->disconnect();
        existing->stop();
        existing->setObjectName(QString());
        existing->deleteLater();
    }
}
}  // namespace

bool animationsEnabled()
{
    if (g_animationsOverride.has_value()) {
        return *g_animationsOverride;
    }
    if (APPLICATION_DYN != nullptr && APPLICATION->settings()) {
        if (!APPLICATION->settings()->get("EnableUiAnimations").toBool()) {
            return false;
        }
    }
    return osClientAreaAnimationsEnabled();
}

void setAnimationsEnabledOverride(std::optional<bool> enabled)
{
    g_animationsOverride = enabled;
}

void animateVisibleHeight(QWidget* target, bool show, int durationMs)
{
    if (!target) {
        return;
    }

    const bool hasActiveAnim = target->findChild<QPropertyAnimation*>(kHeightAnimObjectName, Qt::FindDirectChildrenOnly) != nullptr;

    // Skip redundant work if the widget has already settled in the requested state.
    if (!hasActiveAnim) {
        if (show && target->isVisible() && target->maximumHeight() >= QWIDGETSIZE_MAX) {
            return;
        }
        if (!show && target->isHidden()) {
            target->setMaximumHeight(QWIDGETSIZE_MAX);
            return;
        }
    }

    cancelActiveHeightAnimation(target);

    QWidget* topWindow = target->window();
    const bool parentVisible = !target->parentWidget() || target->parentWidget()->isVisible();
    const bool canAnimate = animationsEnabled() && durationMs > 0 && topWindow && topWindow->isVisible() && parentVisible;
    if (!canAnimate) {
        target->setMaximumHeight(QWIDGETSIZE_MAX);
        target->setVisible(show);
        return;
    }

    int startHeight = target->height();
    int endHeight = 0;
    if (show) {
        target->setMaximumHeight(QWIDGETSIZE_MAX);
        endHeight = qMax(target->sizeHint().height(), target->minimumSizeHint().height());
        if (endHeight <= 0) {
            target->setVisible(true);
            return;
        }
        startHeight = target->isVisible() ? qBound(0, target->height(), endHeight) : 0;
        if (startHeight == endHeight) {
            target->setVisible(true);
            return;
        }
    } else if (startHeight <= 0) {
        target->setVisible(false);
        target->setMaximumHeight(QWIDGETSIZE_MAX);
        return;
    }

    target->setMaximumHeight(startHeight);
    if (show) {
        target->setVisible(true);
    }

    auto* anim = new QPropertyAnimation(target, "maximumHeight", target);
    anim->setObjectName(kHeightAnimObjectName);
    anim->setDuration(durationMs);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->setStartValue(startHeight);
    anim->setEndValue(endHeight);
    QObject::connect(anim, &QPropertyAnimation::finished, target, [target, anim, show]() {
        if (anim->objectName() == kHeightAnimObjectName) {
            anim->setObjectName(QString());
            if (!show) {
                target->setVisible(false);
            }
            if (!target->findChild<QPropertyAnimation*>(kHeightAnimObjectName, Qt::FindDirectChildrenOnly)) {
                target->setMaximumHeight(QWIDGETSIZE_MAX);
            }
        }
        anim->deleteLater();
    });
    anim->start();
}

void installButtonHoverGlow(QAbstractButton* button, qreal borderRadius, int durationMs)
{
    if (!button) {
        return;
    }
    if (button->findChild<QWidget*>(kGlowOverlayObjectName, Qt::FindDirectChildrenOnly)) {
        return;
    }
    new HoverGlowOverlay(button, borderRadius, durationMs);
}

void TransitionOverlay::start(QWidget* container, const QPixmap& previousSnapshot, const QPoint& slideOffset, int durationMs)
{
    if (!container) {
        return;
    }

    if (auto* existing = container->findChild<TransitionOverlay*>(kOverlayObjectName, Qt::FindDirectChildrenOnly)) {
        existing->finishImmediately();
    }

    if (previousSnapshot.isNull() || durationMs <= 0 || !animationsEnabled() || !container->isVisible()) {
        return;
    }

    new TransitionOverlay(container, previousSnapshot, slideOffset, durationMs);
}

TransitionOverlay::TransitionOverlay(QWidget* container, QPixmap snapshot, QPoint slideOffset, int durationMs)
    : QWidget(container), m_container(container), m_snapshot(std::move(snapshot)), m_slideOffset(slideOffset)
{
    setObjectName(kOverlayObjectName);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setGeometry(container->rect());
    container->installEventFilter(this);

    m_anim = new QVariantAnimation(this);
    m_anim->setDuration(durationMs);
    m_anim->setStartValue(0.0);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);

    connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_progress = value.toReal();
        update();
    });
    connect(m_anim, &QVariantAnimation::finished, this, &TransitionOverlay::finishImmediately);

    show();
    raise();
    m_anim->start();
}

bool TransitionOverlay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_container && (event->type() == QEvent::Resize || event->type() == QEvent::Hide)) {
        finishImmediately();
    }
    return QWidget::eventFilter(watched, event);
}

void TransitionOverlay::paintEvent(QPaintEvent*)
{
    if (m_snapshot.isNull() || m_finished) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setOpacity(qBound(0.0, 1.0 - m_progress, 1.0));
    const QPointF offset(static_cast<qreal>(m_slideOffset.x()) * m_progress, static_cast<qreal>(m_slideOffset.y()) * m_progress);
    painter.drawPixmap(rect().translated(offset.toPoint()), m_snapshot);
}

void TransitionOverlay::finishImmediately()
{
    if (m_finished) {
        return;
    }
    m_finished = true;
    setObjectName(QString());
    if (m_container) {
        m_container->removeEventFilter(this);
    }
    if (m_anim) {
        m_anim->disconnect(this);
        m_anim->stop();
    }
    hide();
    deleteLater();
}

}  // namespace UiAnimations
