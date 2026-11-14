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

#include <QAbstractListModel>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QTest>
#include <QWheelEvent>

#include "InstanceList.h"
#include "ui/instanceview/InstanceCardAnimator.h"
#include "ui/instanceview/InstanceDelegate.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/widgets/UiAnimations.h"

namespace {
class TestInstanceListModel final : public QAbstractListModel {
   public:
    explicit TestInstanceListModel(int count, QObject* parent = nullptr) : QAbstractListModel(parent)
    {
        for (int i = 0; i < count; ++i) {
            m_items.append(QStringLiteral("Instance %1").arg(i + 1));
        }
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override { return parent.isValid() ? 0 : m_items.size(); }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
            return {};
        }
        switch (role) {
            case Qt::DisplayRole:
                return m_items.at(index.row());
            case InstanceViewRoles::GroupRole:
                return QStringLiteral("Default");
            case InstanceList::InstancePointerRole:
                return QVariant::fromValue<void*>(nullptr);
            default:
                return {};
        }
    }

    void removeLastRow()
    {
        if (m_items.isEmpty()) {
            return;
        }
        const int row = static_cast<int>(m_items.size() - 1);
        beginRemoveRows(QModelIndex(), row, row);
        m_items.removeAt(row);
        endRemoveRows();
    }

   private:
    QStringList m_items;
};
}  // namespace

class InstanceCardAnimatorTest : public QObject {
    Q_OBJECT

   private slots:
    void cleanup() { UiAnimations::setAnimationsEnabledOverride(std::nullopt); }

    void hoverTransitionAdvancesAndSettles()
    {
        TestInstanceListModel model(2);
        const QModelIndex idx = model.index(0, 0);

        InstanceCardAnimator animator;
        QVERIFY(!animator.hasActiveTransitions());
        QCOMPARE(animator.hoverProgress(idx, false), 0.0);
        QCOMPARE(animator.hoverProgress(idx, true), 1.0);

        animator.setHovered(idx, true, false);
        QVERIFY(animator.hasActiveTransitions());
        QCOMPARE(animator.hoverProgress(idx, true), 0.0);

        QList<QModelIndex> dirty;
        animator.step(75, dirty);
        QCOMPARE(dirty.size(), 1);
        const qreal mid = animator.hoverProgress(idx, false);
        QVERIFY(mid > 0.0 && mid < 1.0);

        animator.step(100, dirty);
        QCOMPARE(animator.hoverProgress(idx, false), 1.0);
        // Hover-in settled: no active transitions should remain, preventing idle timer spinning.
        QVERIFY(!animator.hasActiveTransitions());

        // Hover-out must still begin smoothly from 1.0.
        animator.setHovered(idx, false, false);
        QVERIFY(animator.hasActiveTransitions());
        animator.step(75, dirty);
        const qreal fadeOut = animator.hoverProgress(idx, false);
        QVERIFY(fadeOut > 0.0 && fadeOut < 1.0);

        animator.step(100, dirty);
        QVERIFY(!animator.hasActiveTransitions());
        QCOMPARE(animator.hoverProgress(idx, false), 0.0);
    }

    void selectionAndPressReverseSmoothly()
    {
        TestInstanceListModel model(1);
        const QModelIndex idx = model.index(0, 0);

        InstanceCardAnimator animator;
        animator.setSelected(idx, true);
        animator.setPressed(idx, true, false);

        QList<QModelIndex> dirty;
        animator.step(45, dirty);
        const qreal pressMid = animator.pressProgress(idx);
        const qreal selectMid = animator.selectProgress(idx, false);
        QVERIFY(pressMid > 0.0 && pressMid < 1.0);
        QVERIFY(selectMid > 0.0 && selectMid < 1.0);

        // Reverse selection mid-flight without resetting to 1.0 or 0.0.
        animator.setSelected(idx, false);
        animator.step(16, dirty);
        QVERIFY(animator.selectProgress(idx, false) < selectMid);

        animator.setPressed(idx, false, false);
        animator.step(200, dirty);
        QVERIFY(!animator.hasActiveTransitions());
    }

    void invalidatedIndexPrunedOnStep()
    {
        TestInstanceListModel model(2);
        const QModelIndex idx = model.index(1, 0);

        InstanceCardAnimator animator;
        animator.setHovered(idx, true, false);
        QVERIFY(animator.hasActiveTransitions());

        model.removeLastRow();
        QList<QModelIndex> dirty;
        const bool stillActive = animator.step(16, dirty);
        QVERIFY(!stillActive);
        QVERIFY(dirty.isEmpty());
        QVERIFY(!animator.hasActiveTransitions());

        animator.clear();
        QVERIFY(!animator.hasActiveTransitions());
    }

    void instanceViewSettledHoverStopsTimerAndFilterZeroRowsClearsState()
    {
        UiAnimations::setAnimationsEnabledOverride(true);

        TestInstanceListModel source(3);
        QSortFilterProxyModel proxy;
        proxy.setSourceModel(&source);

        InstanceView view;
        view.setItemDelegate(new ListViewDelegate(&view));
        view.setModel(&proxy);
        view.resize(480, 320);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.updateGeometries();

        const QModelIndex idx = proxy.index(0, 0);
        const QRect cardRect = view.visualRect(idx);
        QVERIFY(cardRect.isValid() && !cardRect.isEmpty());
        const QPointF cardCenter(cardRect.center());
        QMouseEvent hoverMove(QEvent::MouseMove, cardCenter, view.viewport()->mapToGlobal(cardCenter.toPoint()), Qt::NoButton,
                              Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(view.viewport(), &hoverMove);
        QVERIFY(view.hasRunningAnimationTimer());

        // Wait past the 150 ms hover transition; timer must stop once hover settles.
        QTRY_VERIFY_WITH_TIMEOUT(!view.hasRunningAnimationTimer(), 600);
        QCOMPARE(view.hoverProgress(idx, false), 1.0);

        // Filtering down to 0 rows via proxy must keep timer stopped and paint cleanly.
        proxy.setFilterFixedString(QStringLiteral("__no_such_instance__"));
        QCOMPARE(proxy.rowCount(), 0);
        QCoreApplication::processEvents();
        QVERIFY(!view.hasRunningAnimationTimer());
    }

    void instanceViewWheelReverseCancellationAndPixelDelta()
    {
        UiAnimations::setAnimationsEnabledOverride(true);

        TestInstanceListModel source(24);
        InstanceView view;
        view.setItemDelegate(new ListViewDelegate(&view));
        view.setModel(&source);
        view.resize(340, 220);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.updateGeometries();
        QVERIFY(view.verticalScrollBar()->maximum() > 50);

        const QPointF center = QRectF(view.viewport()->rect()).center();
        const QPointF globalCenter = view.viewport()->mapToGlobal(center.toPoint());

        // Step down one wheel notch to start smooth scrolling.
        QWheelEvent downWheel(center, globalCenter, QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false,
                              Qt::MouseEventNotSynthesized);
        QCoreApplication::sendEvent(view.viewport(), &downWheel);
        QVERIFY(view.hasRunningSmoothScroll());
        QTest::qWait(20);

        // Step down again slightly, then reverse one notch; m_scrollAnim must stop or reverse cleanly to ~0 without snapping back down.
        QWheelEvent upWheel(center, globalCenter, QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false,
                            Qt::MouseEventNotSynthesized);
        QCoreApplication::sendEvent(view.viewport(), &upWheel);
        QTRY_VERIFY_WITH_TIMEOUT(!view.hasRunningSmoothScroll(), 500);
        QCOMPARE(view.verticalScrollBar()->value(), 0);

        // Precision touchpad pixelDelta must bypass m_scrollAnim and scroll natively.
        QWheelEvent touchpadWheel(center, globalCenter, QPoint(0, -35), QPoint(0, -30), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase,
                                  false, Qt::MouseEventNotSynthesized);
        QCoreApplication::sendEvent(view.viewport(), &touchpadWheel);
        QVERIFY(!view.hasRunningSmoothScroll());
        QVERIFY(view.verticalScrollBar()->value() > 0);
    }

    void disablingAnimationsClearsStaleHoverStateOnLeave()
    {
        UiAnimations::setAnimationsEnabledOverride(true);

        TestInstanceListModel source(2);
        InstanceView view;
        view.setItemDelegate(new ListViewDelegate(&view));
        view.setModel(&source);
        view.resize(400, 240);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.updateGeometries();

        const QModelIndex idx = source.index(0, 0);
        const QPointF cardCenter(view.visualRect(idx).center());
        QMouseEvent hoverMove(QEvent::MouseMove, cardCenter, view.viewport()->mapToGlobal(cardCenter.toPoint()), Qt::NoButton,
                              Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(view.viewport(), &hoverMove);
        QTRY_VERIFY_WITH_TIMEOUT(!view.hasRunningAnimationTimer(), 600);
        QCOMPARE(view.hoverProgress(idx, false), 1.0);

        // Disable animations while settled, leave card, then re-enable animations: hoverProgress must not stick at 1.0.
        UiAnimations::setAnimationsEnabledOverride(false);
        QEvent leaveEvt(QEvent::Leave);
        QCoreApplication::sendEvent(&view, &leaveEvt);
        UiAnimations::setAnimationsEnabledOverride(true);
        QCOMPARE(view.hoverProgress(idx, false), 0.0);
    }
};

QTEST_MAIN(InstanceCardAnimatorTest)

#include "InstanceCardAnimator_test.moc"
