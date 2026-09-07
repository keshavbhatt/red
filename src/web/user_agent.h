#pragma once

#include "core/settings/settings.h"

#include <QString>

namespace red::web {

/// Qt WebEngine's default user agent is a real Chrome UA with an extra
/// "QtWebEngine/x.y.z" token; removing that token is enough to be treated as
/// Chrome by YouTube and Google sign-in (ADR-008). Pure, unit-tested.
[[nodiscard]] QString sanitizeUserAgent(const QString& defaultUserAgent);

/// A current Firefox UA, sent on Google's sign-in hosts only: Google rejects
/// embedded Chrome ("This browser or app may not be secure") but accepts
/// Firefox (SignInUserAgent::Firefox, the default).
[[nodiscard]] QString firefoxUserAgent();
/// accounts.google.com, accounts.youtube.com and their subdomains.
[[nodiscard]] bool isGoogleSignInHost(const QString& host);
/// The User-Agent header for Google's sign-in hosts, empty for "no change".
[[nodiscard]] QString signInUserAgent(core::SignInUserAgent choice, const QString& userOverride);

/// TV mode (ADR-001, LESSONS V1): what `navigator.userAgent` reports inside
/// Leanback — an old Cobalt so YouTube does not assume Widevine.
[[nodiscard]] QString tvNavigatorUserAgent(const QString& appVersion);
/// TV mode: what www.youtube.com sees on the wire — a newer Cobalt for the
/// current UI. Set per request by the interceptor.
[[nodiscard]] QString tvWireUserAgent(const QString& appVersion);
/// TV mode: everything that is not www.youtube.com.
[[nodiscard]] QString tvGenericUserAgent(const QString& appVersion);

/// The profile UA for a mode: the override wins, then the TV navigator UA,
/// else the sanitized engine default.
[[nodiscard]] QString effectiveUserAgent(const QString& engineDefault, const QString& userOverride,
                                         core::AppMode mode, const QString& appVersion);

} // namespace red::web
