// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include <QDate>
#include <QString>

namespace red::core {

/// A per-day counter that resets when the calendar day changes: the free
/// tier's download allowance. Stored as the day it was counted on plus the
/// count, so nothing needs a timer.
struct DailyAllowance
{
    QString day; ///< ISO date the count belongs to; empty = never used
    int used = 0;

    /// How many of `limit` are left on `today`.
    [[nodiscard]] int remaining(int limit, const QDate& today) const;
    /// The allowance after spending `items` on `today` (a new day starts at 0).
    [[nodiscard]] DailyAllowance spent(int items, const QDate& today) const;
};

} // namespace red::core
