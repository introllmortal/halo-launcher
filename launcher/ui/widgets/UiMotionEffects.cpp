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
#include "UiMotionEffects.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QEasingCurve>
#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "UiAnimations.h"

namespace UiAnimations {

namespace {
constexpr auto kActiveIndicatorName = "HaloListActiveIndicator";
constexpr auto kSmoothScrollFilterName = "HaloSmoothScrollFilter";
constexpr auto kBusyShimmerName = "HaloBusyShimmerOverlay";

class ListActiveIndicatorOverlay final : public QWidget {
   public:
    explicit ListActiveIndicatorOverlay(QAbstractItemView* view) : QWidget(view->viewport()), m_view(view)
    {
        setObjectName(kActiveIndicatorName);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setGeometry(view->viewport()->rect());

        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(175);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_currentRect = value.toRectF();
            update();
        });

        view->viewport()->installEventFilter(this);
        if (auto* sel = view->selectionModel()) {
            connect(sel, &QItemSelectionModel::currentChanged, this, [this](const QModelIndex&, const QModelIndex&) { syncToCurrent(true); });
        }
        if (auto* sb = view->verticalScrollBar()) {
            connect(sb, &QScrollBar::valueChanged, this, [this](int) { syncToCurrent(false); });
        }
        if (auto* model = view->model()) {
            connect(model, &QAbstractItemModel::layoutChanged, this, [this]() { syncToCurrent(false); });
            connect(model, &QAbstractItemModel::modelReset, this, [this]() { syncToCurrent(false); });
        }
        syncToCurrent(false);
        show();
        raise();
    }

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (m_view && watched == m_view->viewport() && event->type() == QEvent::Resize) {
            setGeometry(m_view->viewport()->rect());
            syncToCurrent(false);
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent*) override
    {
        if (!m_visible || m_currentRect.isEmpty()) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor accent = m_view ? m_view->palette().color(QPalette::Highlight) : QColor(56, 189, 248);
        if (!accent.isValid()) {
            accent = QColor(56, 189, 248);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawRoundedRect(m_currentRect, 1.5, 1.5);
    }

   private:
    void syncToCurrent(bool animate)
    {
        if (!m_view) {
            m_visible = false;
            update();
            return;
        }
        const QModelIndex current = m_view->currentIndex();
        const QRect itemRect = current.isValid() ? m_view->visualRect(current) : QRect();
        if (!current.isValid() || !itemRect.isValid() || itemRect.isEmpty()) {
            m_visible = false;
            update();
            return;
        }

        const qreal pillHeight = qMax<qreal>(14.0, itemRect.height() * 0.56);
        const qreal pillY = itemRect.top() + (itemRect.height() - pillHeight) * 0.5;
        const QRectF target(3.0, pillY, 3.5, pillHeight);
        m_visible = true;

        if (!animate || !animationsEnabled() || m_currentRect.isEmpty()) {
            m_anim->stop();
            m_currentRect = target;
            update();
            return;
        }

        m_anim->stop();
        m_anim->setStartValue(m_currentRect);
        m_anim->setEndValue(target);
        m_anim->start();
    }

    QPointer<QAbstractItemView> m_view;
    QVariantAnimation* m_anim = nullptr;
    QRectF m_currentRect;
    bool m_visible = false;
};

class SmoothScrollFilter final : public QObject {
   public:
    explicit SmoothScrollFilter(QAbstractItemView* view) : QObject(view), m_view(view)
    {
        setObjectName(kSmoothScrollFilterName);
        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(150);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            if (!m_view) {
                return;
            }
            if (auto* sb = m_view->verticalScrollBar()) {
                sb->setValue(value.toInt());
            }
        });
        if (auto* sb = view->verticalScrollBar()) {
            connect(sb, &QScrollBar::sliderPressed, this, [this]() { m_anim->stop(); });
        }
        view->viewport()->installEventFilter(this);
    }

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!m_view || watched != m_view->viewport() || event->type() != QEvent::Wheel || !animationsEnabled()) {
            return QObject::eventFilter(watched, event);
        }
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers().testFlag(Qt::ControlModifier) || wheel->modifiers().testFlag(Qt::ShiftModifier)) {
            return false;
        }
        auto* sb = m_view->verticalScrollBar();
        if (!sb || !sb->isVisible() || sb->maximum() <= sb->minimum()) {
            return false;
        }

        int deltaPx = 0;
        if (!wheel->pixelDelta().isNull()) {
            deltaPx = wheel->pixelDelta().y();
        } else {
            deltaPx = qRound(static_cast<qreal>(wheel->angleDelta().y()) / 120.0 * qMax(36, sb->singleStep() * 3));
        }
        if (deltaPx == 0) {
            return false;
        }

        const int currentStart = sb->value();
        const int baseTarget = (m_anim->state() == QAbstractAnimation::Running) ? m_targetValue : currentStart;
        m_targetValue = qBound(sb->minimum(), baseTarget - deltaPx, sb->maximum());
        if (m_targetValue == currentStart) {
            return true;
        }

        m_anim->stop();
        m_anim->setStartValue(currentStart);
        m_anim->setEndValue(m_targetValue);
        m_anim->start();
        wheel->accept();
        return true;
    }

   private:
    QPointer<QAbstractItemView> m_view;
    QVariantAnimation* m_anim = nullptr;
    int m_targetValue = 0;
};

class BusyShimmerOverlay final : public QWidget {
   public:
    explicit BusyShimmerOverlay(QAbstractButton* button) : QWidget(button), m_button(button)
    {
        setObjectName(kBusyShimmerName);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setGeometry(button->rect());

        m_anim = new QVariantAnimation(this);
        m_anim->setDuration(1500);
        m_anim->setStartValue(-0.35);
        m_anim->setEndValue(1.35);
        m_anim->setLoopCount(-1);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_pos = value.toReal();
            update();
        });
        button->installEventFilter(this);
        hide();
    }

    void setActive(bool active)
    {
        if (m_active == active) {
            return;
        }
        m_active = active;
        if (m_active && animationsEnabled()) {
            setGeometry(m_button ? m_button->rect() : QRect());
            show();
            raise();
            m_anim->start();
        } else {
            m_anim->stop();
            hide();
        }
    }

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_button && event->type() == QEvent::Resize) {
            setGeometry(m_button->rect());
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent*) override
    {
        if (!m_active || !m_button || !animationsEnabled()) {
            return;
        }
        const QRectF bounds = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        if (bounds.isEmpty()) {
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal centerX = bounds.left() + bounds.width() * m_pos;
        const qreal bandHalf = qMax<qreal>(28.0, bounds.width() * 0.22);
        QLinearGradient grad(centerX - bandHalf, bounds.top(), centerX + bandHalf, bounds.bottom());
        grad.setColorAt(0.0, QColor(255, 255, 255, 0));
        grad.setColorAt(0.5, QColor(255, 255, 255, 36));
        grad.setColorAt(1.0, QColor(255, 255, 255, 0));

        painter.setPen(Qt::NoPen);
        painter.setBrush(grad);
        painter.drawRoundedRect(bounds, 7.0, 7.0);
    }

   private:
    QPointer<QAbstractButton> m_button;
    QVariantAnimation* m_anim = nullptr;
    qreal m_pos = -0.35;
    bool m_active = false;
};

}  // namespace

void installListActiveIndicator(QAbstractItemView* view)
{
    if (!view || !view->viewport()) {
        return;
    }
    if (view->viewport()->findChild<QWidget*>(kActiveIndicatorName, Qt::FindDirectChildrenOnly)) {
        return;
    }
    new ListActiveIndicatorOverlay(view);
}

void installSmoothScroll(QAbstractItemView* view)
{
    if (!view || !view->viewport()) {
        return;
    }
    if (view->findChild<QObject*>(kSmoothScrollFilterName, Qt::FindDirectChildrenOnly)) {
        return;
    }
    new SmoothScrollFilter(view);
}

void setButtonBusyShimmer(QAbstractButton* button, bool active)
{
    if (!button) {
        return;
    }
    auto* overlay = static_cast<BusyShimmerOverlay*>(button->findChild<QWidget*>(kBusyShimmerName, Qt::FindDirectChildrenOnly));
    if (!overlay && active) {
        overlay = new BusyShimmerOverlay(button);
    }
    if (overlay) {
        overlay->setActive(active);
    }
}

}  // namespace UiAnimations
