// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include <QString>
#include <QStringList>

namespace red::core {

/// Red 9 kept its account ID in two places: QSettings("org.keshavnrj.ubuntu",
/// "Red") key `accountId`, i.e. `<home>/.config/org.keshavnrj.ubuntu/Red.conf`,
/// and the first line of `<home>/Downloads/.red.id`. Under snap confinement
/// `<home>` was the snap's own home. These helpers read that ID back so an
/// existing licence keeps working (pure, tested).

/// Homes to search, most likely first: the process home, the real home
/// (SNAP_REAL_HOME), and the snap data directories of the red-app snap.
[[nodiscard]] QStringList legacyAccountSearchHomes();

/// The Red 9 account ID found under any of `homes`, or empty.
[[nodiscard]] QString legacyRedAccountId(const QStringList& homes);

/// A plausible account ID: 8+ characters of [A-Za-z0-9].
[[nodiscard]] bool looksLikeAccountId(const QString& id);

} // namespace red::core
