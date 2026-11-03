#include "core/swipe_navigation.h"

#include <QTest>

using namespace red::core;

namespace {
// A gesture of `steps` movements of `delta` that the page entirely leaves unused.
SwipeAction swipe(SwipeTracker& t, QPoint delta, int steps, bool inverted = false, int width = 1280)
{
    t.begin(inverted);
    for (int i = 0; i < steps; ++i) {
        t.update(delta);
        t.unused(delta);
    }
    t.end();
    return t.finish(width);
}
} // namespace

class TestSwipeNavigation : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void fingersRightGoBack()
    {
        SwipeTracker t;
        // Classic scrolling: fingers to the right scroll right, a negative wheel delta.
        QCOMPARE(swipe(t, QPoint(-30, 0), 10), SwipeAction::Back);
        QCOMPARE(swipe(t, QPoint(30, 0), 10), SwipeAction::Forward);
    }

    void naturalScrollingKeepsFingerDirection()
    {
        SwipeTracker t;
        QCOMPARE(swipe(t, QPoint(30, 0), 10, true), SwipeAction::Back);
        QCOMPARE(swipe(t, QPoint(-30, 0), 10, true), SwipeAction::Forward);
    }

    void shortSwipeDoesNothing()
    {
        SwipeTracker t;
        QCOMPARE(swipe(t, QPoint(-30, 0), 3), SwipeAction::None); // 90 px of 220
    }

    void thresholdIsAShareOfNarrowViews()
    {
        SwipeTracker t;
        // 400 px wide: a quarter is 100 px.
        QCOMPARE(swipe(t, QPoint(-30, 0), 4, false, 400), SwipeAction::Back);
        QCOMPARE(swipe(t, QPoint(-30, 0), 3, false, 400), SwipeAction::None);
    }

    void scrollThePageUsedNeverNavigates()
    {
        SwipeTracker t;
        t.begin(false);
        for (int i = 0; i < 20; ++i) {
            t.update(QPoint(-40, 0)); // a strip scrolled sideways: nothing comes back
        }
        t.end();
        QCOMPARE(t.finish(1280), SwipeAction::None);
    }

    void verticalScrollWithDriftDoesNothing()
    {
        SwipeTracker t;
        t.begin(false);
        for (int i = 0; i < 20; ++i) {
            t.update(QPoint(-20, 30));
            t.unused(QPoint(-20, 0));
        }
        t.end();
        QCOMPARE(t.finish(1280), SwipeAction::None);
    }

    void unusedScrollOutsideAGestureIsIgnored()
    {
        SwipeTracker t;
        t.unused(QPoint(-500, 0)); // a mouse's tilt wheel: no gesture began
        QVERIFY(!t.active());
        QCOMPARE(t.progress(1280), 0.0);
        QCOMPARE(t.finish(1280), SwipeAction::None);
    }

    void lateUnusedScrollStillCounts()
    {
        SwipeTracker t;
        t.begin(false);
        for (int i = 0; i < 10; ++i) {
            t.update(QPoint(-30, 0));
        }
        t.end();
        for (int i = 0; i < 10; ++i) {
            t.unused(QPoint(-30, 0)); // the page's answers arrive after the fingers lifted
        }
        QCOMPARE(t.finish(1280), SwipeAction::Back);
    }

    void progressIsClampedAndSigned()
    {
        SwipeTracker t;
        t.begin(false);
        t.update(QPoint(-110, 0));
        t.unused(QPoint(-110, 0));
        QCOMPARE(t.progress(1280), 0.5);
        t.update(QPoint(-500, 0));
        t.unused(QPoint(-500, 0));
        QCOMPARE(t.progress(1280), 1.0);
        t.cancel();
        QVERIFY(!t.active());
        QCOMPARE(t.progress(1280), 0.0);
    }
};

QTEST_GUILESS_MAIN(TestSwipeNavigation)
#include "tst_swipe_navigation.moc"
