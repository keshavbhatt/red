#include "core/youtube_url.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestYouTubeUrl : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void classifiesShapes_data()
    {
        QTest::addColumn<QString>("url");
        QTest::addColumn<int>("kind");
        QTest::addColumn<QString>("videoId");
        QTest::addColumn<QString>("playlistId");
        QTest::newRow("watch") << u"https://www.youtube.com/watch?v=aqz-KE-bpKQ"_s
                               << static_cast<int>(YouTubeUrlKind::Video) << u"aqz-KE-bpKQ"_s << QString();
        QTest::newRow("watch+list") << u"https://www.youtube.com/watch?v=aqz-KE-bpKQ&list=PL123"_s
                                    << static_cast<int>(YouTubeUrlKind::Video) << u"aqz-KE-bpKQ"_s
                                    << u"PL123"_s;
        QTest::newRow("youtu.be") << u"https://youtu.be/aqz-KE-bpKQ?t=10"_s
                                  << static_cast<int>(YouTubeUrlKind::Video) << u"aqz-KE-bpKQ"_s << QString();
        QTest::newRow("shorts") << u"https://www.youtube.com/shorts/aqz-KE-bpKQ"_s
                                << static_cast<int>(YouTubeUrlKind::Video) << u"aqz-KE-bpKQ"_s << QString();
        QTest::newRow("live") << u"https://www.youtube.com/live/aqz-KE-bpKQ"_s
                              << static_cast<int>(YouTubeUrlKind::Video) << u"aqz-KE-bpKQ"_s << QString();
        QTest::newRow("embed") << u"https://www.youtube-nocookie.com/embed/aqz-KE-bpKQ"_s
                               << static_cast<int>(YouTubeUrlKind::Video) << u"aqz-KE-bpKQ"_s << QString();
        QTest::newRow("playlist") << u"https://www.youtube.com/playlist?list=PLabc"_s
                                  << static_cast<int>(YouTubeUrlKind::Playlist) << QString() << u"PLabc"_s;
        QTest::newRow("handle") << u"https://www.youtube.com/@Blender/videos"_s
                                << static_cast<int>(YouTubeUrlKind::Channel) << QString() << QString();
        QTest::newRow("channel") << u"https://www.youtube.com/channel/UC123"_s
                                 << static_cast<int>(YouTubeUrlKind::Channel) << QString() << QString();
        QTest::newRow("home") << u"https://www.youtube.com/"_s << static_cast<int>(YouTubeUrlKind::Other)
                              << QString() << QString();
        QTest::newRow("feed") << u"https://www.youtube.com/feed/subscriptions"_s
                              << static_cast<int>(YouTubeUrlKind::Other) << QString() << QString();
        QTest::newRow("tv watch") << u"https://www.youtube.com/tv#/watch?v=aqz-KE-bpKQ&list=x"_s
                                  << static_cast<int>(YouTubeUrlKind::Video) << u"aqz-KE-bpKQ"_s << QString();
        QTest::newRow("tv home") << u"https://www.youtube.com/tv#/"_s
                                 << static_cast<int>(YouTubeUrlKind::Other) << QString() << QString();
        QTest::newRow("other site") << u"https://example.com/watch?v=aqz-KE-bpKQ"_s
                                    << static_cast<int>(YouTubeUrlKind::NotYouTube) << QString() << QString();
        QTest::newRow("bad id") << u"https://www.youtube.com/watch?v=short"_s
                                << static_cast<int>(YouTubeUrlKind::Other) << QString() << QString();
    }

    void classifiesShapes()
    {
        QFETCH(QString, url);
        QFETCH(int, kind);
        QFETCH(QString, videoId);
        QFETCH(QString, playlistId);
        const YouTubeUrlInfo info = classifyYouTubeUrl(QUrl(url));
        QCOMPARE(static_cast<int>(info.kind), kind);
        QCOMPARE(info.videoId, videoId);
        QCOMPARE(info.playlistId, playlistId);
    }

    void flagsAndHelpers()
    {
        QVERIFY(classifyYouTubeUrl(QUrl(u"https://www.youtube.com/shorts/aqz-KE-bpKQ"_s)).isShorts);
        QVERIFY(classifyYouTubeUrl(QUrl(u"https://www.youtube.com/tv"_s)).isTv);
        QCOMPARE(canonicalVideoUrl(QUrl(u"https://youtu.be/aqz-KE-bpKQ"_s)).value(),
                 QUrl(u"https://www.youtube.com/watch?v=aqz-KE-bpKQ"_s));
        QVERIFY(!canonicalVideoUrl(QUrl(u"https://www.youtube.com/"_s)));
        QVERIFY(isDownloadable(QUrl(u"https://www.youtube.com/playlist?list=PL1"_s)));
        QVERIFY(!isDownloadable(QUrl(u"https://www.youtube.com/feed/history"_s)));
        QCOMPARE(thumbnailUrl(u"abc"_s).toString(), u"https://i.ytimg.com/vi/abc/mqdefault.jpg"_s);
        QCOMPARE(videoIdFromTvHash(u"/watch?v=aqz-KE-bpKQ"_s), u"aqz-KE-bpKQ"_s);
        QCOMPARE(videoIdFromTvHash(u"/browse"_s), QString());
    }

    void hosts()
    {
        QVERIFY(isYouTubeHost(u"www.youtube.com"_s));
        QVERIFY(isYouTubeHost(u"youtu.be"_s));
        QVERIFY(isYouTubeHost(u"rr1---sn-abc.googlevideo.com"_s));
        QVERIFY(!isYouTubeHost(u"notyoutube.com"_s));
        QVERIFY(isGoogleServiceHost(u"accounts.google.com"_s));
        QVERIFY(isGoogleServiceHost(u"lh3.googleusercontent.com"_s));
        QVERIFY(!isGoogleServiceHost(u"example.com"_s));
    }
};

QTEST_GUILESS_MAIN(TestYouTubeUrl)
#include "tst_youtube_url.moc"
