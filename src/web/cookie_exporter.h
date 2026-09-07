#pragma once

#include <QHash>
#include <QList>
#include <QNetworkCookie>
#include <QObject>

#include <memory>

class QTemporaryFile;
class QWebEngineCookieStore;

namespace red::web {

/// Mirrors the profile's cookie store for the YouTube/Google domains so a
/// Netscape cookie file can be produced for yt-dlp at any moment (ADR-006).
/// The mirror is kept live through cookieAdded/cookieRemoved.
class CookieExporter : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CookieExporter)

public:
    /// `databasePath` is the profile's "Cookies" SQLite file (may not exist yet).
    CookieExporter(QWebEngineCookieStore* store, QString databasePath, QObject* parent = nullptr);
    ~CookieExporter() override = default;

    [[nodiscard]] QList<QNetworkCookie> sessionCookies() const;
    /// The current value of a cookie by name for a domain suffix, or empty.
    [[nodiscard]] QString cookieValue(const QString& domainSuffix, const QByteArray& name) const;
    [[nodiscard]] bool looksSignedIn() const; ///< a SAPISID/SID cookie for youtube.com exists
    /// A fresh 0600 temp file, or nullptr when there is nothing to export.
    [[nodiscard]] std::unique_ptr<QTemporaryFile> writeTempFile();
    /// Deletes every cookie whose domain matches the suffix (e.g. "google.com").
    /// Returns how many were removed.
    int deleteCookies(const QString& domainSuffix);
    /// Re-reads the persisted cookies from the database (Chromium flushes them
    /// lazily, and QWebEngineCookieStore::loadAllCookies() is broken in Qt
    /// 6.11); live changes seen through the store stay on top.
    void reload();
    [[nodiscard]] int count() const { return static_cast<int>(sessionCookies().size()); }

Q_SIGNALS:
    void signedInChanged(bool signedIn);
    /// A YouTube/Google cookie was added or changed.
    void cookieChanged(const QNetworkCookie& cookie);

private:
    static QString keyOf(const QNetworkCookie& cookie);
    void handleAdded(const QNetworkCookie& cookie);
    void handleRemoved(const QNetworkCookie& cookie);

    void updateSignedIn();

    QWebEngineCookieStore* m_store = nullptr;
    QString m_databasePath;
    QHash<QString, QNetworkCookie> m_persisted; ///< from the database, refreshed by reload()
    QHash<QString, QNetworkCookie> m_cookies;   ///< live changes from the store
    bool m_signedIn = false;
};

} // namespace red::web
