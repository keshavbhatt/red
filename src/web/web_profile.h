#pragma once

#include "core/settings/settings.h"

#include <QJsonObject>
#include <QString>
#include <QWebEngineProfile>

#include <memory>

namespace red::web {

class Bridge;
class CookieExporter;
class RequestInterceptor;
class ScriptBundle;

/// The one persistent profile of the application. Storage paths, cookie
/// policy, user agent, engine attributes, the request interceptor and the
/// injected script bundle are configured here and nowhere else (ADR-006).
/// The active AppMode (ADR-001) changes the UA and the script set.
class WebProfile : public QWebEngineProfile
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebProfile)

public:
    WebProfile(core::Settings& settings, const QString& appVersion, QObject* parent = nullptr);
    ~WebProfile() override;

    [[nodiscard]] core::Settings& appSettings() { return m_settings; }
    [[nodiscard]] Bridge& bridge() { return *m_bridge; }
    [[nodiscard]] ScriptBundle& scripts() { return *m_scripts; }
    [[nodiscard]] RequestInterceptor& interceptor() { return *m_interceptor; }
    [[nodiscard]] CookieExporter& cookies() { return *m_cookies; }

    [[nodiscard]] core::AppMode mode() const { return m_mode; }
    /// Switches UA + scripts; the caller reloads the page afterwards.
    void setMode(core::AppMode mode);
    /// Whether the page is in mini-player state (part of the script config).
    void setMiniPlayer(bool on);

    /// The config object injected as window.__red.config (pure over settings, tested).
    [[nodiscard]] QJsonObject scriptConfig() const;
    /// Directory roots that storage clean-up is allowed to touch.
    [[nodiscard]] QStringList storageRoots() const;

private:
    void configureStorage();
    void configureUserAgent();
    void configureAttributes();
    void configureBlocking();
    void installBootstrap();
    void pushConfig();

    core::Settings& m_settings;
    QString m_appVersion;
    core::AppMode m_mode = core::AppMode::Desktop;
    bool m_miniPlayer = false;
    std::unique_ptr<Bridge> m_bridge;
    std::unique_ptr<ScriptBundle> m_scripts;
    std::unique_ptr<RequestInterceptor> m_interceptor;
    std::unique_ptr<CookieExporter> m_cookies;
};

} // namespace red::web
