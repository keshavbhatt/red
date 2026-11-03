#pragma once

#include <QString>

/// Public web addresses the app points people at (the public repository:
/// guide, issues, changelog, licences).
namespace red::ui::links {

inline const QString kGuide = QStringLiteral("https://github.com/keshavbhatt/red/blob/master/GUIDE.md");
inline const QString kMoreApps = QStringLiteral("https://ktechpit.com/USS/public/products.php");
inline const QString kContact = QStringLiteral("mailto:connect@ktechpit.com");
inline const QString kIssues = QStringLiteral("https://github.com/keshavbhatt/red/issues");
inline const QString kLicences = QStringLiteral("https://github.com/keshavbhatt/red/blob/master/LICENSING.md");
inline const QString kChangelog =
    QStringLiteral("https://github.com/keshavbhatt/red/blob/master/CHANGELOG.md");

} // namespace red::ui::links
