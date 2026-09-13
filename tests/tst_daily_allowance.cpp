#include "core/licensing/daily_allowance.h"

#include <QtTest>

using namespace Qt::StringLiterals;
using red::core::DailyAllowance;

class TstDailyAllowance : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void freshHasTheWholeLimit()
    {
        const DailyAllowance a;
        QCOMPARE(a.remaining(5, QDate(2026, 9, 13)), 5);
    }
    void spendingCountsDown()
    {
        const QDate today(2026, 9, 13);
        DailyAllowance a;
        a = a.spent(1, today);
        a = a.spent(3, today);
        QCOMPARE(a.day, u"2026-09-13"_s);
        QCOMPARE(a.used, 4);
        QCOMPARE(a.remaining(5, today), 1);
        QCOMPARE(a.spent(2, today).remaining(5, today), 0); // never negative
    }
    void aNewDayResets()
    {
        const DailyAllowance a{u"2026-09-12"_s, 5};
        const QDate today(2026, 9, 13);
        QCOMPARE(a.remaining(5, today), 5);
        QCOMPARE(a.spent(1, today).used, 1);
    }
    void garbageIsHarmless()
    {
        const DailyAllowance a{u"not a date"_s, -3};
        QCOMPARE(a.remaining(5, QDate(2026, 9, 13)), 5);
        QCOMPARE(a.spent(-1, QDate(2026, 9, 13)).used, 0);
    }
};

QTEST_APPLESS_MAIN(TstDailyAllowance)
#include "tst_daily_allowance.moc"
