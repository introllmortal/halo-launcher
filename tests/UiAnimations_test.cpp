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

#include <QEnterEvent>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTest>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "ui/pages/BasePage.h"
#include "ui/pages/BasePageProvider.h"
#include "ui/widgets/InstanceInspector.h"
#include "ui/widgets/PageContainer.h"
#include "ui/widgets/UiAnimations.h"

namespace {
class TestPage final : public QWidget, public BasePage {
   public:
    TestPage(QString pageId, QString name) : m_id(std::move(pageId)), m_name(std::move(name)) {}
    QString id() const override { return m_id; }
    QString displayName() const override { return m_name; }
    QIcon icon() const override { return {}; }

   private:
    QString m_id;
    QString m_name;
};

class ZeroHeightWidget final : public QWidget {
   public:
    explicit ZeroHeightWidget(QWidget* parent = nullptr) : QWidget(parent) {}
    QSize sizeHint() const override { return QSize(80, 0); }
    QSize minimumSizeHint() const override { return QSize(0, 0); }
};
}  // namespace

class UiAnimationsTest : public QObject {
    Q_OBJECT

   private slots:
    void cleanup() { UiAnimations::setAnimationsEnabledOverride(std::nullopt); }

    void animationsToggleRespectsOverride()
    {
        UiAnimations::setAnimationsEnabledOverride(true);
        QVERIFY(UiAnimations::animationsEnabled());

        UiAnimations::setAnimationsEnabledOverride(false);
        QVERIFY(!UiAnimations::animationsEnabled());
    }

    void animateVisibleHeightGuardsAndHiddenAncestors()
    {
        UiAnimations::setAnimationsEnabledOverride(true);

        // nullptr target must be a safe no-op.
        UiAnimations::animateVisibleHeight(nullptr, true, 60);

        // Hidden parent container must still mark child explicitly hidden when hiding,
        // and must reveal immediately without starting a stuck animation when showing.
        QWidget window;
        auto* windowLayout = new QVBoxLayout(&window);
        auto* hiddenParent = new QWidget(&window);
        auto* parentLayout = new QVBoxLayout(hiddenParent);
        auto* child = new QPushButton("Button", hiddenParent);
        parentLayout->addWidget(child);
        windowLayout->addWidget(hiddenParent);
        hiddenParent->hide();
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        QVERIFY(!child->isHidden());
        QVERIFY(!child->isVisible());
        UiAnimations::animateVisibleHeight(child, false, 60);
        QVERIFY(child->isHidden());
        QCOMPARE(child->maximumHeight(), QWIDGETSIZE_MAX);

        // Zero-duration call applies state immediately.
        UiAnimations::animateVisibleHeight(child, true, 0);
        QVERIFY(!child->isHidden());
        QCOMPARE(child->maximumHeight(), QWIDGETSIZE_MAX);

        // Zero-height sizeHint widget resets maximumHeight safely when shown.
        hiddenParent->show();
        auto* zeroChild = new ZeroHeightWidget(hiddenParent);
        parentLayout->addWidget(zeroChild);
        zeroChild->hide();
        UiAnimations::animateVisibleHeight(zeroChild, true, 60);
        QVERIFY(!zeroChild->isHidden());
        QCOMPARE(zeroChild->maximumHeight(), QWIDGETSIZE_MAX);
    }

    void animateVisibleHeightWhenDisabledAndMidFlightDisable()
    {
        UiAnimations::setAnimationsEnabledOverride(true);

        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        auto* child = new QPushButton("Action", &host);
        layout->addWidget(child);
        child->hide();
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        // Start animation then disable animations and reverse immediately.
        UiAnimations::animateVisibleHeight(child, true, 160);
        QVERIFY(child->isVisible());

        UiAnimations::setAnimationsEnabledOverride(false);
        UiAnimations::animateVisibleHeight(child, false, 160);
        QVERIFY(!child->isVisible());
        QCOMPARE(child->maximumHeight(), QWIDGETSIZE_MAX);

        UiAnimations::animateVisibleHeight(child, true, 160);
        QVERIFY(child->isVisible());
        QCOMPARE(child->maximumHeight(), QWIDGETSIZE_MAX);
    }

    void animateVisibleHeightTransitionsRapidReverseAndTargetDestruction()
    {
        UiAnimations::setAnimationsEnabledOverride(true);

        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        QPointer<QPushButton> child = new QPushButton("Update", &host);
        layout->addWidget(child);
        child->hide();
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));

        UiAnimations::animateVisibleHeight(child, true, 60);
        QVERIFY(child->isVisible());

        // Immediately reverse mid-animation and verify maximumHeight recovers cleanly.
        UiAnimations::animateVisibleHeight(child, false, 60);
        QTRY_VERIFY(!child->isVisible());
        QCOMPARE(child->maximumHeight(), QWIDGETSIZE_MAX);

        // Reveal to completion and ensure no stale height animation remains.
        UiAnimations::animateVisibleHeight(child, true, 60);
        QTRY_COMPARE(child->maximumHeight(), QWIDGETSIZE_MAX);
        QVERIFY(child->isVisible());

        // Deleting target mid-animation must be completely safe.
        UiAnimations::animateVisibleHeight(child, false, 120);
        delete child;
        QVERIFY(child.isNull());
        QTest::qWait(30);
    }

    void transitionOverlayGuardsReplaceResizeAndHide()
    {
        UiAnimations::setAnimationsEnabledOverride(true);

        // Guard branches: nullptr container & null pixmap.
        UiAnimations::TransitionOverlay::start(nullptr, QPixmap(), QPoint(0, 6), 60);

        QWidget host;
        host.resize(160, 120);
        // Hidden container guard.
        QPixmap dummy(16, 16);
        dummy.fill(Qt::black);
        UiAnimations::TransitionOverlay::start(&host, dummy, QPoint(0, 6), 60);
        QVERIFY(host.findChildren<UiAnimations::TransitionOverlay*>(QString(), Qt::FindDirectChildrenOnly).isEmpty());

        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        const QPixmap snap = host.grab();
        QVERIFY(!snap.isNull());

        // Null snapshot & zero duration guards.
        UiAnimations::TransitionOverlay::start(&host, QPixmap(), QPoint(0, 6), 60);
        UiAnimations::TransitionOverlay::start(&host, snap, QPoint(0, 6), 0);
        QVERIFY(host.findChildren<UiAnimations::TransitionOverlay*>(QString(), Qt::FindDirectChildrenOnly).isEmpty());

        // Rapid replace creates a distinct active overlay and retires the previous one cleanly.
        UiAnimations::TransitionOverlay::start(&host, snap, QPoint(0, 6), 120);
        QPointer<UiAnimations::TransitionOverlay> firstOverlay =
            host.findChild<UiAnimations::TransitionOverlay*>(QStringLiteral("HaloTransitionOverlay"), Qt::FindDirectChildrenOnly);
        QVERIFY(!firstOverlay.isNull());

        UiAnimations::TransitionOverlay::start(&host, snap, QPoint(0, -6), 120);
        const auto activeAfterReplace =
            host.findChildren<UiAnimations::TransitionOverlay*>(QStringLiteral("HaloTransitionOverlay"), Qt::FindDirectChildrenOnly);
        QCOMPARE(activeAfterReplace.size(), 1);
        QPointer<UiAnimations::TransitionOverlay> secondOverlay = activeAfterReplace.first();
        QVERIFY(!secondOverlay.isNull());
        QVERIFY(secondOverlay != firstOverlay);

        // Resizing container cancels active overlay immediately.
        host.resize(200, 140);
        QTRY_VERIFY(host.findChildren<UiAnimations::TransitionOverlay*>(QString(), Qt::FindDirectChildrenOnly).isEmpty());

        // Hiding container cancels active overlay immediately.
        UiAnimations::TransitionOverlay::start(&host, snap, QPoint(0, 6), 120);
        QVERIFY(host.findChild<UiAnimations::TransitionOverlay*>(QStringLiteral("HaloTransitionOverlay"), Qt::FindDirectChildrenOnly)
                != nullptr);
        host.hide();
        QTRY_VERIFY(host.findChildren<UiAnimations::TransitionOverlay*>(QString(), Qt::FindDirectChildrenOnly).isEmpty());
    }

    void buttonHoverGlowLifecycleAndIdempotency()
    {
        // Null button guard.
        UiAnimations::installButtonHoverGlow(nullptr);

        UiAnimations::setAnimationsEnabledOverride(true);
        QPushButton button(QStringLiteral("Launch"));
        button.resize(140, 34);

        UiAnimations::installButtonHoverGlow(&button, 8.0, 50);
        // Second call must be idempotent and not duplicate child overlays.
        UiAnimations::installButtonHoverGlow(&button, 8.0, 50);
        const auto overlays = button.findChildren<QWidget*>(QStringLiteral("HaloHoverGlowOverlay"), Qt::FindDirectChildrenOnly);
        QCOMPARE(overlays.size(), 1);
        QWidget* overlay = overlays.first();
        QVERIFY(overlay->isHidden());

        button.show();
        QVERIFY(QTest::qWaitForWindowExposed(&button));

        QEnterEvent enterEvt(QPointF(10, 10), QPointF(10, 10), button.mapToGlobal(QPoint(10, 10)));
        QCoreApplication::sendEvent(&button, &enterEvt);
        QVERIFY(!overlay->isHidden());

        // Leaving smoothly fades back out and hides overlay at rest.
        QEvent leaveEvt(QEvent::Leave);
        QCoreApplication::sendEvent(&button, &leaveEvt);
        QTRY_VERIFY_WITH_TIMEOUT(overlay->isHidden(), 500);

        // Disabling animations hides any active hover glow immediately on leave/enter.
        QCoreApplication::sendEvent(&button, &enterEvt);
        QVERIFY(!overlay->isHidden());
        UiAnimations::setAnimationsEnabledOverride(false);
        QCoreApplication::sendEvent(&button, &leaveEvt);
        QVERIFY(overlay->isHidden());

        UiAnimations::setAnimationsEnabledOverride(true);
        QCoreApplication::sendEvent(&button, &enterEvt);
        QVERIFY(!overlay->isHidden());

        // Visible glow overlay must not swallow mouse clicks on the host button.
        int clickCount = 0;
        QObject::connect(&button, &QPushButton::clicked, &button, [&clickCount]() { ++clickCount; });
        QTest::mouseClick(&button, Qt::LeftButton);
        QCOMPARE(clickCount, 1);

        // Hiding the button resets any active glow immediately.
        button.hide();
        QVERIFY(overlay->isHidden());

        button.show();
        QVERIFY(QTest::qWaitForWindowExposed(&button));
        QCoreApplication::sendEvent(&button, &enterEvt);
        QVERIFY(!overlay->isHidden());
        button.setEnabled(false);
        QVERIFY(overlay->isHidden());

        // Enter while disabled must keep overlay hidden.
        QCoreApplication::sendEvent(&button, &enterEvt);
        QVERIFY(overlay->isHidden());
    }

    void instanceInspectorInstallsHoverGlowsWithoutInitialOverlay()
    {
        UiAnimations::setAnimationsEnabledOverride(true);
        InstanceInspector inspector(InstanceInspector::Actions{});
        inspector.show();
        QVERIFY(QTest::qWaitForWindowExposed(&inspector));

        auto* stack = inspector.findChild<QStackedWidget*>();
        QVERIFY(stack != nullptr);
        QCOMPARE(stack->currentIndex(), 0);
        QVERIFY(stack->findChildren<QWidget*>(QStringLiteral("HaloTransitionOverlay"), Qt::FindDirectChildrenOnly).isEmpty());

        auto* playBtn = inspector.findChild<QToolButton*>(QStringLiteral("playPrimaryButton"));
        auto* updateBtn = inspector.findChild<QPushButton*>(QStringLiteral("inspectorUpdateButton"));
        QVERIFY(playBtn != nullptr);
        QVERIFY(updateBtn != nullptr);
        QCOMPARE(playBtn->findChildren<QWidget*>(QStringLiteral("HaloHoverGlowOverlay"), Qt::FindDirectChildrenOnly).size(), 1);
        QCOMPARE(updateBtn->findChildren<QWidget*>(QStringLiteral("HaloHoverGlowOverlay"), Qt::FindDirectChildrenOnly).size(), 1);
    }

    void pageContainerDirectionalTransitionsAndGuards()
    {
        UiAnimations::setAnimationsEnabledOverride(true);

        GenericPageProvider provider(QStringLiteral("Settings"));
        provider.addPageCreator([]() { return new TestPage(QStringLiteral("p1"), QStringLiteral("Page 1")); });
        provider.addPageCreator([]() { return new TestPage(QStringLiteral("p2"), QStringLiteral("Page 2")); });
        provider.addPageCreator([]() { return new TestPage(QStringLiteral("p3"), QStringLiteral("Page 3")); });

        PageContainer container(&provider, QStringLiteral("p1"));
        container.resize(480, 320);

        // Initial page selection while hidden must not leave an active transition overlay.
        QVERIFY(container.findChildren<UiAnimations::TransitionOverlay*>(QStringLiteral("HaloTransitionOverlay")).isEmpty());

        container.show();
        QVERIFY(QTest::qWaitForWindowExposed(&container));

        // Switching down (row 0 -> 1) starts a transition overlay that settles and cleans up.
        QVERIFY(container.selectPage(QStringLiteral("p2")));
        auto activeDown = container.findChildren<UiAnimations::TransitionOverlay*>(QStringLiteral("HaloTransitionOverlay"));
        QCOMPARE(activeDown.size(), 1);

        // Rapid reverse switch (row 1 -> 0) replaces cleanly without stacking multiple overlays.
        QVERIFY(container.selectPage(QStringLiteral("p1")));
        auto activeUp = container.findChildren<UiAnimations::TransitionOverlay*>(QStringLiteral("HaloTransitionOverlay"));
        QCOMPARE(activeUp.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(
            container.findChildren<UiAnimations::TransitionOverlay*>(QStringLiteral("HaloTransitionOverlay")).isEmpty(), 500);

        // Disabled animations skip overlay creation entirely.
        UiAnimations::setAnimationsEnabledOverride(false);
        QVERIFY(container.selectPage(QStringLiteral("p3")));
        QVERIFY(container.findChildren<UiAnimations::TransitionOverlay*>(QStringLiteral("HaloTransitionOverlay")).isEmpty());
    }
};

QTEST_MAIN(UiAnimationsTest)
#include "UiAnimations_test.moc"
