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

/// Writes the text to a 0600 temp file that is deleted with the returned object.
[[nodiscard]] std::unique_ptr<QTemporaryFile> writeCookiesTempFile(const QList<QNetworkCookie>& cookies);

} // namespace red::services
