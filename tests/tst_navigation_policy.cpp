#include "core/navigation_policy.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestNavigationPolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void externalLinks()
    {
        QVERIFY(!shouldOpenExternally(QUrl(u"https://www.youtube.com/watch?v=x"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"https://accounts.google.com/signin"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"https://consent.youtube.com/m"_s)));
        QVERIFY(shouldOpenExternally(QUrl(u"https://example.com/"_s)));
        QVERIFY(shouldOpenExternally(QUrl(u"mailto:someone@example.com"_s)));
        QVERIFY(!shouldOpenExternally(QUrl(u"about:blank"_s)));
        QVERIFY(!shouldOpenExternally(QUrl()));
    }

    void popups()
    {
        QVERIFY(isInAppPopupUrl(QUrl()));
        QVERIFY(isInAppPopupUrl(QUrl(u"https://accounts.google.com/o/oauth2"_s)));
        QVERIFY(!isInAppPopupUrl(QUrl(u"https://example.com"_s)));
    }

    void redirectUnwrap()
    {
        const QUrl wrapped(
            u"https://www.youtube.com/redirect?event=video_description&q=https%3A%2F%2Fexample.com%2Fpage"_s);
        QCOMPARE(unwrapRedirect(wrapped), QUrl(u"https://example.com/page"_s));
        QCOMPARE(unwrapRedirect(QUrl(u"https://example.com/"_s)), QUrl(u"https://example.com/"_s));
    }
};

QTEST_GUILESS_MAIN(TestNavigationPolicy)
#include "tst_navigation_policy.moc"
