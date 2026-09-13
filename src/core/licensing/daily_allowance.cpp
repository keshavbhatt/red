#include "core/licensing/daily_allowance.h"

#include <algorithm>

namespace red::core {

namespace {
int usedOn(const DailyAllowance& a, const QDate& today)
{
    return a.day == today.toString(Qt::ISODate) ? std::max(0, a.used) : 0;
}
} // namespace

int DailyAllowance::remaining(int limit, const QDate& today) const
{
    return std::max(0, limit - usedOn(*this, today));
}

DailyAllowance DailyAllowance::spent(int items, const QDate& today) const
{
    return {today.toString(Qt::ISODate), usedOn(*this, today) + std::max(0, items)};
}

} // namespace red::core
