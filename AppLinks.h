// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#pragma once

#include "AppCatalog.h"

#include <QUrl>

namespace ktechpit::AppLinks {

/// Where "Get" leads for this install type: the snap or Flathub store page in the desktop's
/// software centre (snap://, appstream://), else the product page on ktechpit.com.
[[nodiscard]] QUrl getUrl(const AppInfo& app, Package package, const QString& source,
                          const QString& place);
/// The product page, with ?ref=<source>-<place> when a source is set.
[[nodiscard]] QUrl pageUrl(const AppInfo& app, const QString& source, const QString& place);

/// Opens getUrl(); falls back to the product page when no handler takes the store link.
void openGet(const AppInfo& app, const QString& place);
/// Opens the product page.
void openPage(const AppInfo& app, const QString& place);

/// True when the app is visibly installed here as a snap or a Flatpak. Inside a sandbox the
/// host system is often out of sight, so this can miss an install; never the other way round.
[[nodiscard]] bool isInstalled(const AppInfo& app);

} // namespace ktechpit::AppLinks
