#include "web/script_bundle.h"

#include "web/logging.h"

#include <QFile>
#include <QJsonDocument>
#include <QtEnvironmentVariables>
#include <QWebEngineProfile>
#include <QWebEngineScriptCollection>

using namespace Qt::StringLiterals;

namespace red::web {

namespace {
const QString kBootstrapName = u"bootstrap"_s;
}

ScriptBundle::ScriptBundle(QWebEngineProfile& profile)
    : m_profile(profile)
{}

QString ScriptBundle::scriptName(const QString& name)
{
    return u"red:"_s + name;
}

QString ScriptBundle::readResource(const QString& resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(lcWeb) << "missing script resource" << resourcePath;
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QStringList ScriptBundle::bootstrapResources(bool tvMode)
{
    // Order matters: hooks.js provides the JSON/fetch/XHR modifier registry
    // every other script registers into (LESSONS V3).
    static const QStringList kShared{
        u":/scripts/hooks.js"_s,
        u":/scripts/adblock.js"_s,
        u":/scripts/sponsorblock.js"_s,
        u":/scripts/media-session.js"_s,
        u":/scripts/hide-shorts.js"_s,
        u":/scripts/codec-preference.js"_s,
        u":/scripts/background-play.js"_s,
    };
    static const QStringList kDesktop{
        u":/scripts/theme-control.js"_s,
        u":/scripts/mini-player.js"_s,
    };
    static const QStringList kTv{
        u":/scripts/tv/h5vcc.js"_s,
        u":/scripts/tv/identification.js"_s,
        u":/scripts/tv/leanback-fixes.js"_s,
        u":/scripts/tv/input.js"_s,
        u":/scripts/tv/gamepad.js"_s,
    };
    const QStringList features = kShared + (tvMode ? kTv : kDesktop);
    QStringList resources{u":/qtwebchannel/qwebchannel.js"_s, u":/scripts/bootstrap.js"_s};

    // Diagnostic escape hatch: RED_DISABLE_WEB_SCRIPTS turns page tweaks off to
    // isolate an interfering feature. "1"/"all" drops every tweak; otherwise it is
    // a comma-separated list of script names to drop (e.g. "adblock").
    const QString disable = qEnvironmentVariable("RED_DISABLE_WEB_SCRIPTS").trimmed();
    if (disable == u"1"_s || disable.compare(u"all"_s, Qt::CaseInsensitive) == 0) {
        return resources;
    }
    const QStringList drop = disable.isEmpty() ? QStringList() : disable.split(u',', Qt::SkipEmptyParts);
    for (const QString& res : features) {
        const QString name = res.mid(res.lastIndexOf(u'/') + 1).chopped(3); // basename, no ".js"
        if (!drop.contains(name.trimmed())) {
            resources << res;
        }
    }
    return resources;
}

void ScriptBundle::installBootstrap(const QJsonObject& config, bool tvMode)
{
    // Config travels as JSON — never string-concatenated user text.
    QString source = u"window.__redConfig = "_s +
                     QString::fromUtf8(QJsonDocument(config).toJson(QJsonDocument::Compact)) + u";\n"_s;
    for (const QString& resource : bootstrapResources(tvMode)) {
        source += readResource(resource);
        source += u"\n"_s;
    }
    installSource(kBootstrapName, source, QWebEngineScript::DocumentCreation);
}

void ScriptBundle::installResource(const QString& name, const QString& resourcePath,
                                   QWebEngineScript::InjectionPoint point)
{
    installSource(name, readResource(resourcePath), point);
}

void ScriptBundle::installSource(const QString& name, const QString& source,
                                 QWebEngineScript::InjectionPoint point)
{
    remove(name);
    if (source.isEmpty()) {
        return;
    }
    QWebEngineScript script;
    script.setName(scriptName(name));
    script.setSourceCode(source);
    script.setInjectionPoint(point);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setRunsOnSubFrames(false);
    m_profile.scripts()->insert(script);
    qCDebug(lcWeb) << "installed script" << script.name() << "at" << point;
}

void ScriptBundle::remove(const QString& name)
{
    const QList<QWebEngineScript> existing = m_profile.scripts()->find(scriptName(name));
    for (const QWebEngineScript& s : existing) {
        m_profile.scripts()->remove(s);
    }
}

bool ScriptBundle::isInstalled(const QString& name) const
{
    return !m_profile.scripts()->find(scriptName(name)).isEmpty();
}

} // namespace red::web
