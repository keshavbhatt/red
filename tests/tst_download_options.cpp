#include "core/downloads/download_options.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestDownloadOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void videoPreset()
    {
        DownloadOptions o;
        o.kind = DownloadOptions::Kind::Video;
        o.quality = VideoQuality::Q1080;
        o.container = Container::Mp4;
        o.outputDirectory = u"/tmp/out"_s;
        QCOMPARE(formatSelector(o), u"bv*+ba/b"_s);
        QCOMPARE(formatSort(o), u"res:1080,ext:mp4:m4a"_s);
        QCOMPARE(expectedExtension(o), u"mp4"_s);
        EnginePaths paths;
        paths.ytdlp = u"/x/yt-dlp"_s;
        paths.ffmpeg = u"/usr/bin/ffmpeg"_s;
        paths.jsRuntime = u"quickjs:/x/qjs"_s;
        paths.cookiesFile = u"/tmp/c.txt"_s;
        const QStringList args = downloadArguments(o, paths, u"https://www.youtube.com/watch?v=abc"_s);
        QVERIFY(args.contains(u"--no-update"_s));
        QVERIFY(args.contains(u"--ignore-config"_s));
        QCOMPARE(args.at(args.indexOf(u"--ffmpeg-location"_s) + 1), u"/usr/bin/ffmpeg"_s);
        QCOMPARE(args.at(args.indexOf(u"--js-runtimes"_s) + 1), u"quickjs:/x/qjs"_s);
        QCOMPARE(args.at(args.indexOf(u"--cookies"_s) + 1), u"/tmp/c.txt"_s);
        QCOMPARE(args.at(args.indexOf(u"-S"_s) + 1), u"res:1080,ext:mp4:m4a"_s);
        QCOMPARE(args.at(args.indexOf(u"--merge-output-format"_s) + 1), u"mp4"_s);
        QVERIFY(args.contains(u"--no-playlist"_s));
        QVERIFY(args.contains(u"--embed-thumbnail"_s));
        QVERIFY(args.contains(u"--embed-metadata"_s));
        QCOMPARE(args.at(args.indexOf(u"-o"_s) + 1), u"/tmp/out/%(title)s.%(ext)s"_s);
        QCOMPARE(args.last(), u"https://www.youtube.com/watch?v=abc"_s);
        QCOMPARE(args.at(args.size() - 2), u"--"_s);
        const QString joined = args.join(u'\n');
        QVERIFY(joined.contains(u"download:RED:{"_s));
        QVERIFY(joined.contains(u"postprocess:REDPP:{"_s));
        QVERIFY(joined.contains(u"after_move:REDFILE:%(filepath)s"_s));
    }

    void audioAndCustom()
    {
        DownloadOptions o;
        o.kind = DownloadOptions::Kind::Audio;
        o.audioFormat = AudioFormat::Mp3;
        o.audioBitrateKbps = 192;
        o.embedThumbnail = false;
        o.embedMetadata = false;
        QCOMPARE(formatSelector(o), u"ba/b"_s);
        QVERIFY(formatSort(o).isEmpty());
        QCOMPARE(expectedExtension(o), u"mp3"_s);
        const QStringList args = downloadArguments(o, {}, u"u"_s);
        QVERIFY(args.contains(u"--extract-audio"_s));
        QCOMPARE(args.at(args.indexOf(u"--audio-format"_s) + 1), u"mp3"_s);
        QCOMPARE(args.at(args.indexOf(u"--audio-quality"_s) + 1), u"192K"_s);
        QVERIFY(!args.contains(u"--embed-thumbnail"_s));
        QVERIFY(!args.contains(u"--merge-output-format"_s));

        DownloadOptions c;
        c.kind = DownloadOptions::Kind::Custom;
        c.customVideoFormat = u"137"_s;
        c.customAudioFormat = u"140"_s;
        c.container = Container::Mkv;
        QCOMPARE(formatSelector(c), u"137+140"_s);
        QCOMPARE(expectedExtension(c), u"mkv"_s);
        c.customAudioFormat.clear();
        QCOMPARE(formatSelector(c), u"137"_s);
        c.customVideoFormat.clear();
        c.customAudioFormat = u"140"_s;
        QCOMPARE(formatSelector(c), u"140"_s);
        QCOMPARE(expectedExtension(c), u"opus"_s);
    }

    void playlistSubtitlesSponsorsAndLimits()
    {
        DownloadOptions o;
        o.isPlaylist = true;
        o.playlistItems = u"1,3-5"_s;
        o.subtitleLanguages = {u"en"_s};
        o.includeAutoSubtitles = true;
        o.embedSubtitles = false;
        o.removeSponsors = true;
        o.sponsorCategories = {u"sponsor"_s, u"selfpromo"_s};
        o.speedLimitKbps = 500;
        o.filenamePattern = FilenamePattern::TitleId;
        o.outputDirectory = u"/d"_s;
        const QStringList args = downloadArguments(o, {}, u"u"_s);
        QVERIFY(args.contains(u"--yes-playlist"_s));
        QCOMPARE(args.at(args.indexOf(u"--playlist-items"_s) + 1), u"1,3-5"_s);
        QCOMPARE(args.at(args.indexOf(u"--sub-langs"_s) + 1), u"en"_s);
        QVERIFY(args.contains(u"--write-auto-subs"_s));
        QVERIFY(args.contains(u"--convert-subs"_s));
        QVERIFY(!args.contains(u"--embed-subs"_s));
        QCOMPARE(args.at(args.indexOf(u"--sponsorblock-remove"_s) + 1), u"sponsor,selfpromo"_s);
        QCOMPARE(args.at(args.indexOf(u"--limit-rate"_s) + 1), u"500K"_s);
        QVERIFY(args.at(args.indexOf(u"-o"_s) + 1)
                    .contains(u"%(playlist_index|0)03d - %(title)s [%(id)s].%(ext)s"_s));
        QCOMPARE(previewFileName(o, u"A/B: C"_s, u"id1"_s, QString()), u"A_B_ C [id1].mp4"_s);
        o.filenamePattern = FilenamePattern::ChannelTitle;
        QCOMPARE(previewFileName(o, u"T"_s, u"id"_s, u"Ch"_s), u"Ch - T.mp4"_s);
    }

    void probeArgumentsAndJson()
    {
        const QStringList flat = probeArguments({}, u"https://www.youtube.com/playlist?list=x"_s, true);
        QVERIFY(flat.contains(u"--dump-single-json"_s));
        QVERIFY(flat.contains(u"--flat-playlist"_s));
        const QStringList single = probeArguments({}, u"u"_s, false);
        QVERIFY(single.contains(u"--no-playlist"_s));

        DownloadOptions o;
        o.kind = DownloadOptions::Kind::Audio;
        o.audioFormat = AudioFormat::Flac;
        o.subtitleLanguages = {u"de"_s, u"fr"_s};
        o.playlistItems = u"2"_s;
        const DownloadOptions back = DownloadOptions::fromJson(o.toJson());
        QCOMPARE(back.kind, DownloadOptions::Kind::Audio);
        QCOMPARE(back.audioFormat, AudioFormat::Flac);
        QCOMPARE(back.subtitleLanguages, o.subtitleLanguages);
        QCOMPARE(back.playlistItems, u"2"_s);
    }
};

QTEST_GUILESS_MAIN(TestDownloadOptions)
#include "tst_download_options.moc"
