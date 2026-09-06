#include "core/blocking/block_list.h"

#include <QElapsedTimer>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestBlockList : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void blocksAdHostsAndSubdomains()
    {
        const BlockList list = BlockList::builtin({true, true});
        QVERIFY(list.matches(QUrl(u"https://doubleclick.net/x"_s)));
        QVERIFY(list.matches(QUrl(u"https://ad.doubleclick.net/ddm/x"_s)));
        QVERIFY(list.matches(QUrl(u"https://pagead2.googlesyndication.com/pagead/js"_s)));
        QVERIFY(list.matches(QUrl(u"https://www.google-analytics.com/collect"_s)));
        QVERIFY(list.matches(QUrl(u"https://csp.withgoogle.com/csp/report"_s)));
    }

    void neverBlocksPlayback()
    {
        const BlockList list = BlockList::builtin({true, true});
        QVERIFY(!list.matches(QUrl(u"https://rr3---sn-abcd.googlevideo.com/videoplayback?x=1"_s)));
        QVERIFY(!list.matches(QUrl(u"https://i.ytimg.com/vi/abc/mqdefault.jpg"_s)));
        QVERIFY(!list.matches(QUrl(u"https://www.youtube.com/youtubei/v1/player"_s)));
        QVERIFY(!list.matches(QUrl(u"https://www.youtube.com/watch?v=abc"_s)));
        QVERIFY(!list.matches(QUrl(u"https://www.gstatic.com/x.js"_s)));
        QVERIFY(!list.matches(QUrl(u"https://notdoubleclick.net/"_s)));
    }

    void youtubePathsAndToggles()
    {
        const BlockList all = BlockList::builtin({true, true});
        QVERIFY(all.matches(QUrl(u"https://www.youtube.com/pagead/interaction/?ai=x"_s)));
        QVERIFY(all.matches(QUrl(u"https://www.youtube.com/api/stats/ads?x"_s)));
        QVERIFY(all.matches(QUrl(u"https://www.youtube.com/youtubei/v1/log_event?alt=json"_s)));
        const BlockList adsOnly = BlockList::builtin({true, false});
        QVERIFY(adsOnly.matches(QUrl(u"https://www.youtube.com/pagead/x"_s)));
        QVERIFY(!adsOnly.matches(QUrl(u"https://www.youtube.com/youtubei/v1/log_event"_s)));
        QVERIFY(!adsOnly.matches(QUrl(u"https://www.google-analytics.com/collect"_s)));
        const BlockList none = BlockList::none();
        QVERIFY(none.isEmpty());
        QVERIFY(!none.matches(QUrl(u"https://doubleclick.net/"_s)));
        QVERIFY(BlockList::adHosts().size() > 10);
        QVERIFY(!BlockList::adHosts().contains(u"googlevideo.com"_s));
    }

    void isCheap()
    {
        const BlockList list = BlockList::builtin({true, true});
        const QString host = u"rr3---sn-abcd-efgh.googlevideo.com"_s;
        const QString path = u"/videoplayback"_s;
        QElapsedTimer t;
        t.start();
        int hits = 0;
        for (int i = 0; i < 100000; ++i) {
            hits += list.matches(host, path) ? 1 : 0;
        }
        QCOMPARE(hits, 0);
        QVERIFY2(t.elapsed() < 2000, "100k lookups must take well under 2 s");
    }
};

QTEST_GUILESS_MAIN(TestBlockList)
#include "tst_block_list.moc"
