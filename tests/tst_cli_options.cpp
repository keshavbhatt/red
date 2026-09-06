#include "app/cli_options.h"

#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::app;

class TestCliOptions : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultsWhenNoArguments()
    {
        const CliParseResult r = parseCliOptions({u"red"_s});
        QVERIFY(r.ok());
        QVERIFY(!r.helpRequested);
        QVERIFY(!r.versionRequested);
        QVERIFY(r.options.profile.isEmpty());
        QVERIFY(!r.options.tvMode);
        QVERIFY(!r.options.logFile.has_value());
        QVERIFY(!r.options.hasCommands());
    }

    void parsesEverything()
    {
        const CliParseResult r =
            parseCliOptions({u"red"_s, u"--profile"_s, u"work"_s, u"--tv"_s, u"--log-file"_s, u"/tmp/x.log"_s,
                             u"--download"_s, u"https://youtu.be/abc"_s, u"--settings"_s,
                             u"https://www.youtube.com/watch?v=x"_s});
        QVERIFY(r.ok());
        QCOMPARE(r.options.profile, u"work"_s);
        QVERIFY(r.options.tvMode);
        QCOMPARE(r.options.logFile.value(), u"/tmp/x.log"_s);
        QCOMPARE(r.options.download.value(), u"https://youtu.be/abc"_s);
        QVERIFY(r.options.showSettings);
        QCOMPARE(r.options.urls, QStringList{u"https://www.youtube.com/watch?v=x"_s});
        QVERIFY(r.options.hasCommands());
    }

    void helpAndVersion()
    {
        const CliParseResult help = parseCliOptions({u"red"_s, u"--help"_s});
        QVERIFY(help.ok());
        QVERIFY(help.helpRequested);
        QVERIFY(help.helpText.contains(u"--profile"_s));

        const CliParseResult version = parseCliOptions({u"red"_s, u"-v"_s});
        QVERIFY(version.ok());
        QVERIFY(version.versionRequested);
    }

    void unknownOptionIsAnError()
    {
        const CliParseResult r = parseCliOptions({u"red"_s, u"--bogus"_s});
        QVERIFY(!r.ok());
        QVERIFY(r.errorText.contains(u"bogus"_s));
    }

    void commandsForwardInOrderAndEndWithRaise()
    {
        CliOptions o;
        o.tvMode = true;
        o.urls = {u"https://youtu.be/1"_s};
        o.download = u"https://youtu.be/2"_s;
        o.showSettings = true;
        const QList<QJsonObject> cmds = commandsFor(o);
        QCOMPARE(cmds.size(), 5);
        QCOMPARE(cmds[0].value(u"cmd"_s).toString(), u"tv"_s);
        QCOMPARE(cmds[1].value(u"cmd"_s).toString(), u"open"_s);
        QCOMPARE(cmds[1].value(u"url"_s).toString(), u"https://youtu.be/1"_s);
        QCOMPARE(cmds[2].value(u"cmd"_s).toString(), u"download"_s);
        QCOMPARE(cmds[2].value(u"url"_s).toString(), u"https://youtu.be/2"_s);
        QCOMPARE(cmds[3].value(u"cmd"_s).toString(), u"settings"_s);
        QCOMPARE(cmds[4].value(u"cmd"_s).toString(), u"raise"_s);
    }

    void quitIsExclusive()
    {
        CliOptions o;
        o.quit = true;
        o.showSettings = true;
        const QList<QJsonObject> cmds = commandsFor(o);
        QCOMPARE(cmds.size(), 1);
        QCOMPARE(cmds[0].value(u"cmd"_s).toString(), u"quit"_s);
    }

    void plainInvocationJustRaises()
    {
        const QList<QJsonObject> cmds = commandsFor(CliOptions{});
        QCOMPARE(cmds.size(), 1);
        QCOMPARE(cmds[0].value(u"cmd"_s).toString(), u"raise"_s);
    }
};

QTEST_GUILESS_MAIN(TestCliOptions)
#include "tst_cli_options.moc"
