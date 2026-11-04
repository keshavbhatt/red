// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;
class QRandomGenerator;

namespace ktechpit {

/// How the host app was installed; decides where a "Get" button leads.
enum class Package {
    Native,   ///< distro package or a build from source
    Snap,
    Flatpak,
    AppImage,
    Windows,
};

/// One app of the catalog (api/apps.php, schema 1).
struct AppInfo {
    QString id;      ///< product slug, stable ("glate")
    QString name;    ///< short name for a card ("Glate")
    QString title;   ///< full title
    QString summary; ///< one line
    QString type;    ///< freeware | freemium | premium
    QStringList platforms;
    int trending = 0; ///< the site's trending rank, 1..5; 0 when not trending
    QUrl icon;
    QUrl page;
    QString snap;    ///< snap name, empty when not on the Snap Store
    QString flatpak; ///< Flathub app id, empty when not on Flathub
};

struct CatalogConfig {
    /// The host app's catalog id (product slug, e.g. "red"); never suggested to itself.
    QString hostAppId;
    Package package = Package::Native;
    /// Added to web links as ?ref=<source>-<place>, so the site can count where clicks come from.
    QString source;
    QUrl catalogUrl = QUrl(QStringLiteral("https://ktechpit.com/USS/public/api/apps.php"));
    /// The catalog is fetched again when the cached copy is older than this.
    int refreshDays = 7;
    /// Delay before the first network request, so it never competes with the app's start.
    int refreshDelayMs = 20000;
};

/// The list of Ktechpit apps, shared by every widget of the module.
///
/// Starts from the copy bundled in the resources, replaced by a cached download when there is
/// one, and refreshed from the web at most every `refreshDays`. The only request it makes is
/// a GET of the catalog and of app icons; it sends nothing about the user.
class AppCatalog : public QObject
{
    Q_OBJECT

public:
    /// Sets up the catalog. Call once, after QApplication exists.
    static void init(const CatalogConfig& config);
    static AppCatalog* instance();

    [[nodiscard]] const CatalogConfig& config() const { return m_config; }

    /// Every app that runs here, in the site's order; the host app and installed apps left out.
    [[nodiscard]] QList<AppInfo> apps() const;
    /// Up to `count` of apps(), drawn at random, trending apps more often; different on every call.
    [[nodiscard]] QList<AppInfo> randomApps(int count) const;

    /// Off: every widget of the module hides. The host keeps the choice in its own settings.
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    /// The app's icon at `size` px: bundled, cached or downloaded; a lettered tile meanwhile.
    [[nodiscard]] QPixmap icon(const AppInfo& app, int size) const;

    /// Fetches the catalog now.
    void refresh();

    // Pure helpers, public for the tests.
    [[nodiscard]] static QList<AppInfo> parse(const QByteArray& json, bool* ok = nullptr);
    [[nodiscard]] static QList<AppInfo> randomApps(QList<AppInfo> pool, int count, QRandomGenerator& random);
    /// How much more often an app comes up than one that is not trending: #1 counts 6, #5
    /// counts 2, every other app 1.
    [[nodiscard]] static int weight(const AppInfo& app);
    [[nodiscard]] static QString currentPlatform();
    [[nodiscard]] static QPixmap placeholderIcon(const QString& name, int size);

Q_SIGNALS:
    void changed();
    void iconReady(const QString& appId);
    void enabledChanged(bool enabled);

private:
    explicit AppCatalog(const CatalogConfig& config, QObject* parent = nullptr);
    void load();
    bool adopt(const QByteArray& json);
    void fetchIcon(const AppInfo& app) const;
    [[nodiscard]] QString cacheDir() const;

    CatalogConfig m_config;
    QList<AppInfo> m_apps;
    bool m_enabled = true;
    QNetworkAccessManager* m_network = nullptr;
    mutable QSet<QString> m_iconRequests;
};

} // namespace ktechpit
