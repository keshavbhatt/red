#include "web/user_agent.h"

#include <QTest>

using namespace Qt::StringLiterals;
using red::web::effectiveUserAgent;
using red::web::sanitizeUserAgent;

class TestUserAgent : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stripsQtWebEngineToken()
    {
        const QString in = u"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                           "QtWebEngine/6.11.1 Chrome/130.0.6723.192 Safari/537.36"_s;
        const QString out = sanitizeUserAgent(in);
        QVERIFY(!out.contains(u"QtWebEngine"_s));
        QVERIFY(out.contains(u"Chrome/130.0.6723.192"_s));
        QVERIFY(!out.contains(u"  "_s));
    }

    void leavesPlainChromeUntouched()
    {
        const QString in = u"Mozilla/5.0 (X11; Linux x86_64) Chrome/130.0.0.0 Safari/537.36"_s;
        QCOMPARE(sanitizeUserAgent(in), in);
    }

    void overrideWins()
    {
        using red::core::AppMode;
        using red::core::SignInUserAgent;
        QCOMPARE(effectiveUserAgent(u"engine QtWebEngine/6.11.1"_s, u"  custom  "_s, AppMode::Desktop,
                                    SignInUserAgent::Chrome, u"10"_s),
                 u"custom"_s);
        QVERIFY(effectiveUserAgent(u"engine"_s, QString(), AppMode::Tv, SignInUserAgent::Chrome, u"10"_s)
                    .contains(u"Cobalt/19"_s));
        QVERIFY(
            effectiveUserAgent(u"engine"_s, QString(), AppMode::Desktop, SignInUserAgent::Firefox, u"10"_s)
                .contains(u"Firefox/"_s));
        QVERIFY(red::web::tvWireUserAgent(u"10"_s).contains(u"Cobalt/25"_s));
        QCOMPARE(red::web::tvGenericUserAgent(u"10.0.0"_s), u"Red/10.0.0"_s);
    }

    void emptyOverrideFallsBackToSanitizedDefault()
    {
        QCOMPARE(effectiveUserAgent(u"engine QtWebEngine/6.11.1"_s, u"   "_s, red::core::AppMode::Desktop,
                                    red::core::SignInUserAgent::Chrome, u"10"_s),
                 u"engine"_s);
    }
};

QTEST_GUILESS_MAIN(TestUserAgent)
#include "tst_user_agent.moc"
