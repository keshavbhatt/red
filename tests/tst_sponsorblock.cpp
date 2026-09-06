#include "core/blocking/sponsorblock.h"

#include <QJsonObject>
#include <QTest>
#include <QUrlQuery>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestSponsorBlock : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void hashPrefixIsStable()
    {
        QCOMPARE(sponsorHashPrefix(u"dQw4w9WgXcQ"_s).size(), 4);
        QCOMPARE(sponsorHashPrefix(u"dQw4w9WgXcQ"_s), sponsorHashPrefix(u"dQw4w9WgXcQ"_s));
        QVERIFY(sponsorHashPrefix(u"a"_s) != sponsorHashPrefix(u"b"_s));
    }

    void urlCarriesCategories()
    {
        const QHash<QString, SponsorAction> actions{{u"sponsor"_s, SponsorAction::Skip},
                                                    {u"intro"_s, SponsorAction::Off}};
        const QStringList cats = enabledSponsorCategories(actions);
        QVERIFY(cats.contains(u"sponsor"_s));
        QVERIFY(!cats.contains(u"intro"_s));
        const QUrl url = sponsorSegmentsUrl(u"dQw4w9WgXcQ"_s, cats);
        QVERIFY(url.path().startsWith(u"/api/skipSegments/"_s));
        QVERIFY(QUrlQuery(url).queryItemValue(u"categories"_s, QUrl::FullyDecoded).contains(u"sponsor"_s));
    }

    void parsesAndFiltersByVideo()
    {
        const QByteArray body =
            "[{\"videoID\":\"other\",\"segments\":[{\"UUID\":\"x\",\"category\":\"sponsor\",\"actionType\":"
            "\"skip\",\"segment\":[1,2]}]},"
            "{\"videoID\":\"mine\",\"segments\":[{\"UUID\":\"b\",\"category\":\"intro\",\"actionType\":"
            "\"skip\",\"segment\":[30,40],\"locked\":1,\"votes\":3},"
            "{\"UUID\":\"a\",\"category\":\"sponsor\",\"actionType\":\"skip\",\"segment\":[5,15]},"
            "{\"UUID\":\"bad\",\"category\":\"sponsor\",\"actionType\":\"skip\",\"segment\":[20,10]}]}]";
        const QList<SponsorSegment> segs = parseSponsorSegments(body, u"mine"_s);
        QCOMPARE(segs.size(), 2);
        QCOMPARE(segs.first().uuid, u"a"_s);
        QCOMPARE(segs.last().category, u"intro"_s);
        QVERIFY(segs.last().locked);
        QVERIFY(parseSponsorSegments("not json", u"mine"_s).isEmpty());
    }

    void pageArrayHonoursActions()
    {
        SponsorSegment sponsor;
        sponsor.category = u"sponsor"_s;
        sponsor.start = 1;
        sponsor.end = 2;
        SponsorSegment intro = sponsor;
        intro.category = u"intro"_s;
        SponsorSegment full = sponsor;
        full.actionType = u"full"_s;
        const QHash<QString, SponsorAction> actions{{u"sponsor"_s, SponsorAction::Skip},
                                                    {u"intro"_s, SponsorAction::Off}};
        const QJsonArray out = sponsorSegmentsForPage({sponsor, intro, full}, actions);
        QCOMPARE(out.size(), 2);
        QCOMPARE(out.at(0).toObject().value(u"action"_s).toString(), u"skip"_s);
        QCOMPARE(out.at(1).toObject().value(u"action"_s).toString(), u"show"_s);
        QCOMPARE(sponsorRemoveCategories(actions),
                 (QStringList{u"sponsor"_s, u"selfpromo"_s, u"interaction"_s}));
    }

    void defaultsAreSane()
    {
        QCOMPARE(defaultSponsorAction(u"sponsor"_s), SponsorAction::Skip);
        QCOMPARE(defaultSponsorAction(u"intro"_s), SponsorAction::Show);
        QCOMPARE(defaultSponsorAction(u"filler"_s), SponsorAction::Off);
        QCOMPARE(sponsorCategories().size(), 8);
    }
};

QTEST_GUILESS_MAIN(TestSponsorBlock)
#include "tst_sponsorblock.moc"
