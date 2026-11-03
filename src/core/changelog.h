#pragma once

#include <QList>
#include <QString>

namespace red::core {

/// One `## [<version>]` heading of a Keep-a-Changelog document.
struct ChangelogRelease
{
    QString version; ///< without a `v` prefix
    QString date;    ///< the `YYYY-MM-DD` after the heading; empty while unreleased
};

/// Every release heading in document order (newest first in a Keep-a-Changelog file), so the
/// What's new dialog can offer older versions too. Pure, tested.
[[nodiscard]] QList<ChangelogRelease> changelogReleases(const QString& markdown);

/// The body of the `## [<version>]` section of a Keep-a-Changelog document:
/// everything after that heading up to the next `## ` heading, trimmed.
/// The heading may carry a suffix (`— unreleased`, `- 2026-09-20`) and a `v`
/// prefix on the version; `### ` subheadings stay in the body; Keep-a-Changelog
/// link-reference lines (`[1.0.0]: https://…`) are dropped. Empty when the
/// version has no section. Pure, tested.
[[nodiscard]] QString changelogSection(const QString& markdown, const QString& version);

} // namespace red::core
