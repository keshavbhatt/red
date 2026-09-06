#pragma once

#include <QUrl>

// Pure navigation rules (FEATURES S11): what stays in the app and what goes
// to the system browser.
namespace red::core {

/// True for links that should open in the system browser: anything that is
/// neither YouTube nor a Google service host (sign-in, consent, CDN).
[[nodiscard]] bool shouldOpenExternally(const QUrl& url);

/// True for pop-ups that must stay in-app (Google sign-in windows).
[[nodiscard]] bool isInAppPopupUrl(const QUrl& url);

/// Rewrites YouTube's redirect wrapper (youtube.com/redirect?q=…) to its target.
[[nodiscard]] QUrl unwrapRedirect(const QUrl& url);

} // namespace red::core
