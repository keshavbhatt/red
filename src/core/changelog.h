#pragma once

#include <QString>

namespace red::core {

/// The body of the `## [<version>]` section of a Keep-a-Changelog document:
/// everything after that heading up to the next `## ` heading, trimmed.
/// The heading may carry a suffix (`— unreleased`, `- 2026-09-20`) and a `v`
/// prefix on the version; `### ` subheadings stay in the body; Keep-a-Changelog
/// link-reference lines (`[1.0.0]: https://…`) are dropped. Empty when the
/// version has no section. Pure, tested.
[[nodiscard]] QString changelogSection(const QString& markdown, const QString& version);

} // namespace red::core
