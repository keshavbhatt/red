#pragma once

#include <QList>
#include <QNetworkCookie>
#include <QString>

#include <memory>

class QTemporaryFile;

// Netscape cookie-file export for yt-dlp (ADR-006). Pure formatting plus a
// helper that writes a private temp file.
namespace red::services {

/// True for the domains yt-dlp needs to act as the signed-in user.
[[nodiscard]] bool isYouTubeSessionCookie(const QNetworkCookie& cookie);

/// The Netscape/Mozilla cookies.txt text for the given cookies.
[[nodiscard]] QString netscapeCookieText(const QList<QNetworkCookie>& cookies);

/// Reads Chromium's "Cookies" SQLite database (QtWebEngine stores values in
/// clear). The file is copied first: Chromium holds it locked and flushes
/// lazily. Needed because QWebEngineCookieStore::loadAllCookies() delivers
/// nothing in Qt 6.11 (its GetAllCookies call carries no callback).
[[nodiscard]] QList<QNetworkCookie> readChromiumCookieDatabase(const QString& path);

/// Writes the text to a 0600 temp file that is deleted with the returned object.
[[nodiscard]] std::unique_ptr<QTemporaryFile> writeCookiesTempFile(const QList<QNetworkCookie>& cookies);

} // namespace red::services
