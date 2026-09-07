#include "core/downloads/ytdlp_output.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestYtdlpOutput : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void progressLine()
    {
        const auto e = parseOutputLine(
            u"RED:{\"status\":\"downloading\",\"downloaded\":3072,\"total\":117526,\"estimate\":0,\"speed\":45434.4,\"eta\":2,\"filename\":\"out/x.m4a\",\"index\":3,\"id\":\"jNQXAC9IVRw\",\"count\":10}"_s);
        QVERIFY(e.has_value());
        const auto* p = std::get_if<ProgressEvent>(&*e);
        QVERIFY(p != nullptr);
        QCOMPARE(p->status, u"downloading"_s);
        QCOMPARE(p->downloaded, 3072);
        QCOMPARE(p->totalOrEstimate(), 117526);
        QCOMPARE(p->eta, 2);
        QCOMPARE(p->index, 3);
        QCOMPARE(p->count, 10);
        QCOMPARE(p->id, u"jNQXAC9IVRw"_s);
    }

    void otherLines()
    {
        const auto pp =
            parseOutputLine(u"REDPP:{\"status\":\"started\",\"postprocessor\":\"Merger\",\"id\":\"x\"}"_s);
        QVERIFY(std::holds_alternative<PostprocessEvent>(*pp));
        QCOMPARE(std::get<PostprocessEvent>(*pp).label(), u"Merging streams"_s);

        const auto item = parseOutputLine(
            u"REDITEM:{\"id\":\"v\",\"title\":\"T\",\"index\":2,\"count\":5,\"thumbnail\":\"t\",\"duration\":19,\"uploader\":\"U\",\"url\":\"w\"}"_s);
        QVERIFY(std::holds_alternative<ItemEvent>(*item));
        QCOMPARE(std::get<ItemEvent>(*item).count, 5);

        const auto file = parseOutputLine(u"REDFILE:/home/u/Videos/Red/Me at the zoo.mp4"_s);
        QCOMPARE(std::get<FileEvent>(*file).path, u"/home/u/Videos/Red/Me at the zoo.mp4"_s);

        const auto err = parseOutputLine(u"ERROR: [youtube] abc: Video unavailable"_s);
        QCOMPARE(std::get<MessageEvent>(*err).level, MessageEvent::Level::Error);
        const auto warn = parseOutputLine(u"WARNING: something"_s);
        QCOMPARE(std::get<MessageEvent>(*warn).level, MessageEvent::Level::Warning);
        const auto info = parseOutputLine(u"[youtube] Extracting URL"_s);
        QCOMPARE(std::get<MessageEvent>(*info).level, MessageEvent::Level::Info);
        QVERIFY(!parseOutputLine(u"   "_s).has_value());
        QVERIFY(!parseOutputLine(u"RED:not json"_s).has_value());
    }

    void friendlyErrors()
    {
        QVERIFY(friendlyError(u"ERROR: [youtube] x: Sign in to confirm you’re not a bot."_s, 1)
                    .contains(u"Sign in"_s));
        QVERIFY(friendlyError(u"ERROR: Private video"_s, 1).contains(u"private"_s));
        QVERIFY(
            friendlyError(u"ERROR: [youtube:tab] RDabc: YouTube said: This playlist type is unviewable."_s, 1)
                .contains(u"mix"_s));
        QCOMPARE(friendlyError(u"ERROR: [youtube:tab] PLx: Something odd happened (caused by <X>)"_s, 1),
                 u"Something odd happened"_s);
        QVERIFY(friendlyError(u"ERROR: Requested format is not available"_s, 1).contains(u"quality"_s));
        QVERIFY(friendlyError(u"WARNING: x\nERROR: [youtube] abc: Video unavailable"_s, 1)
                    .contains(u"unavailable"_s));
        QCOMPARE(friendlyError(QString(), 7), u"The download engine exited with code 7."_s);
        QVERIFY(friendlyError(u"ERROR: something odd happened"_s, 1).startsWith(u"something odd"_s));
    }
};

QTEST_GUILESS_MAIN(TestYtdlpOutput)
#include "tst_ytdlp_output.moc"
