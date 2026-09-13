#pragma once

#include <QString>

/// Public web addresses the app points people at (the public repository:
/// guide, issues, changelog).
namespace red::ui::links {

inline const QString kGuide = QStringLiteral("https://github.com/keshavbhatt/red/blob/master/GUIDE.md");
inline const QString kIssues = QStringLiteral("https://github.com/keshavbhatt/red/issues");
inline const QString kChangelog =
    QStringLiteral("https://github.com/keshavbhatt/red/blob/master/CHANGELOG.md");

} // namespace red::ui::links
