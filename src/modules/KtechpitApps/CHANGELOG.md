# Changelog

Versions follow [Semantic Versioning](https://semver.org/): a new catalog field or widget option
is a minor version, a change that needs the host app to change its code is a major version.

## [1.0.0] - 2026-11-04

First release.

- `AppCatalog`: the list of Ktechpit apps from `https://ktechpit.com/USS/public/api/apps.php`
  (schema 1), with a bundled copy for offline and first starts, refreshed in the background at
  most once a week and on request (`refresh()`, `refreshFinished`). Leaves out the host app and
  apps for other platforms.
- `AppStrip`: a heading, cards drawn at random each time (trending apps more often, installed
  apps skipped) and a "See all" link.
- `MoreAppsDialog`: every app in the site's order, installed ones marked, with search and a
  Refresh button.
- `AppCard`: icon, name, one line and a Get button that opens the software centre for the way
  the host app was installed (`snap://`, `appstream://`), else the product page with
  `?ref=<source>-<place>`.
- A setting-friendly switch (`setEnabled`) that hides every card.
