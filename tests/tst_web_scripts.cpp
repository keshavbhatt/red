// The injected bundle: every resource exists, parses as JavaScript, and the
// profile-level installation works for both modes (ADR-006).

#include "web/script_bundle.h"

#include <QApplication>
#include <QJSEngine>
#include <QTest>
#include <QWebEngineProfile>
#include <QWebEngineScriptCollection>

using namespace Qt::StringLiterals;
using red::web::ScriptBundle;

class TestWebScripts : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void everyResourceExistsAndParses()
    {
        QStringList all = ScriptBundle::bootstrapResources(false);
        for (const QString& r : ScriptBundle::bootstrapResources(true)) {
            if (!all.contains(r)) {
                all << r;
            }
        }
        QVERIFY(all.size() >= 12);
        QJSEngine engine;
        for (const QString& resource : all) {
            const QString source = ScriptBundle::readResource(resource);
            QVERIFY2(!source.isEmpty(), qPrintable(resource));
            const QJSValue result =
                engine.evaluate(u"(function(){ 'use strict'; "_s + source + u"\n})"_s, resource);
            QVERIFY2(!result.isError(), qPrintable(resource + u": "_s + result.toString()));
        }
    }

    void headerBlocksArePresent()
    {
        for (const QString& resource : ScriptBundle::bootstrapResources(true)) {
            if (resource.contains(u"qwebchannel"_s)) {
                continue;
            }
            const QString source = ScriptBundle::readResource(resource);
            QVERIFY2(source.startsWith(u"// name:"_s), qPrintable(resource));
            QVERIFY2(source.contains(u"// on-fail:"_s), qPrintable(resource));
        }
    }

    void modeSelectsTvScripts()
    {
        const QStringList desktop = ScriptBundle::bootstrapResources(false);
        const QStringList tv = ScriptBundle::bootstrapResources(true);
        QVERIFY(desktop.contains(u":/scripts/theme-control.js"_s));
        QVERIFY(!desktop.contains(u":/scripts/tv/gamepad.js"_s));
        QVERIFY(tv.contains(u":/scripts/tv/gamepad.js"_s));
        QVERIFY(!tv.contains(u":/scripts/mini-player.js"_s));
        QVERIFY(tv.contains(u":/scripts/adblock.js"_s));
        QCOMPARE(desktop.first(), u":/qtwebchannel/qwebchannel.js"_s);
        QCOMPARE(desktop.at(1), u":/scripts/bootstrap.js"_s);
        QCOMPARE(desktop.at(2), u":/scripts/hooks.js"_s);
    }

    void installsOnProfile()
    {
        QWebEngineProfile profile; // off-the-record
        ScriptBundle bundle(profile);
        bundle.installBootstrap({{u"mode"_s, u"desktop"_s}}, false);
        QVERIFY(bundle.isInstalled(u"bootstrap"_s));
        QCOMPARE(profile.scripts()->count(), 1);
        const QString source = profile.scripts()->find(u"red:bootstrap"_s).first().sourceCode();
        QVERIFY(source.startsWith(u"window.__redConfig = {\"mode\":\"desktop\"}"_s));
        QVERIFY(source.contains(u"tv/h5vcc"_s) == false);
        bundle.installBootstrap({{u"mode"_s, u"tv"_s}}, true);
        QCOMPARE(profile.scripts()->count(), 1); // replaced, not duplicated
        QVERIFY(profile.scripts()
                    ->find(u"red:bootstrap"_s)
                    .first()
                    .sourceCode()
                    .contains(u"name:     tv/h5vcc"_s));
        bundle.remove(u"bootstrap"_s);
        QCOMPARE(profile.scripts()->count(), 0);
    }
};

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    TestWebScripts test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_web_scripts.moc"
