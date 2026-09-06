#include "core/downloads/media_info.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestMediaInfo : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesVideo()
    {
        const QByteArray json =
            "{\"_type\":\"video\",\"id\":\"abc\",\"title\":\"T\",\"channel\":\"C\",\"duration\":95,\"view_"
            "count\":1234567,"
            "\"upload_date\":\"20240102\",\"thumbnail\":\"https://i/x.jpg\",\"chapters\":[{\"title\":\"a\"}],"
            "\"subtitles\":{\"en\":[{\"ext\":\"vtt\",\"name\":\"English\"}]},\"automatic_captions\":{\"de\":["
            "{\"ext\":\"vtt\"}]},"
            "\"formats\":["
            "{\"format_id\":\"sb0\",\"ext\":\"mhtml\",\"vcodec\":\"none\",\"acodec\":\"none\"},"
            "{\"format_id\":\"140\",\"ext\":\"m4a\",\"vcodec\":\"none\",\"acodec\":\"mp4a.40.2\",\"abr\":129."
            "5,\"filesize\":100},"
            "{\"format_id\":\"137\",\"ext\":\"mp4\",\"vcodec\":\"avc1.640028\",\"acodec\":\"none\",\"width\":"
            "1920,\"height\":1080,\"fps\":30,\"filesize_approx\":5000,\"dynamic_range\":\"SDR\"},"
            "{\"format_id\":\"18\",\"ext\":\"mp4\",\"vcodec\":\"avc1.42001E\",\"acodec\":\"mp4a.40.2\","
            "\"width\":640,\"height\":360,\"fps\":30}"
            "]}";
        QString error;
        const MediaInfo info = parseMediaInfo(json, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(info.type, MediaInfo::Type::Video);
        QCOMPARE(info.id, u"abc"_s);
        QCOMPARE(info.uploader, u"C"_s);
        QCOMPARE(info.durationText, u"1:35"_s);
        QCOMPARE(info.viewCount, 1234567);
        QVERIFY(info.hasChapters);
        QCOMPARE(info.formats.size(), 3); // storyboard dropped
        QCOMPARE(info.availableHeights(), (QList<int>{1080, 360}));
        QCOMPARE(info.subtitleLanguages(false), QStringList{u"en"_s});
        QCOMPARE(info.subtitleLanguages(true), (QStringList{u"en"_s, u"de"_s}));
        const MediaFormat& audio = info.formats.at(0);
        QVERIFY(audio.isAudioOnly());
        QCOMPARE(audio.codecLabel(), u"AAC"_s);
        const MediaFormat& video = info.formats.at(1);
        QVERIFY(video.isVideoOnly());
        QCOMPARE(video.filesize, 5000);
        QCOMPARE(video.codecLabel(), u"H.264"_s);
        QCOMPARE(info.formats.at(2).codecLabel(), u"H.264 + AAC"_s);
    }

    void parsesFlatPlaylist()
    {
        const QByteArray json =
            "{\"_type\":\"playlist\",\"id\":\"PL1\",\"title\":\"List\",\"playlist_count\":2,"
            "\"entries\":[{\"id\":\"v1\",\"title\":\"One\",\"duration\":10,\"url\":\"https://www.youtube.com/"
            "watch?v=v1\"},"
            "{\"id\":\"v2\",\"title\":\"Two\",\"thumbnails\":[{\"url\":\"t\"}]}]}";
        const MediaInfo info = parseMediaInfo(json);
        QVERIFY(info.isPlaylist());
        QCOMPARE(info.entryCount, 2);
        QCOMPARE(info.entries.size(), 2);
        QCOMPARE(info.entries.at(1).url, u"https://www.youtube.com/watch?v=v2"_s);
        QCOMPARE(info.entries.at(1).thumbnail, u"t"_s);
    }

    void rejectsGarbage()
    {
        QString error;
        const MediaInfo info = parseMediaInfo("[1,2]", &error);
        QVERIFY(info.id.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void formatting()
    {
        QCOMPARE(formatDuration(0), QString());
        QCOMPARE(formatDuration(19), u"0:19"_s);
        QCOMPARE(formatDuration(3725), u"1:02:05"_s);
        QCOMPARE(formatBytes(-1), QString());
        QCOMPARE(formatBytes(999), u"999 B"_s);
        QCOMPARE(formatBytes(118000000), u"118 MB"_s);
        QCOMPARE(formatBytes(3200000000LL), u"3.20 GB"_s);
        QCOMPARE(formatCount(999), u"999"_s);
        QCOMPARE(formatCount(1500), u"1.5K"_s);
        QCOMPARE(formatCount(23000000), u"23M"_s);
    }
};

QTEST_GUILESS_MAIN(TestMediaInfo)
#include "tst_media_info.moc"
