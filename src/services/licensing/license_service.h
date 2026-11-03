// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include "AccountLicenseManager.h"
#include "core/settings/settings.h"

#include <QObject>
#include <QString>

namespace red::services {

/// Account and licence state for the app (one instance, owned by the window):
/// wraps AccountLicenseManager, migrates a Red 9 account ID on first run, and
/// is the single gate every Pro feature asks (`checkAccess`). Downloads are
/// not gated outright: the free tier gets kFreeDownloadsPerDay a day
/// (`canDownload` / `spendDownloads`), paid and evaluation are unlimited.
class LicenseService : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(LicenseService)

public:
    enum class Tier
    {
        Pro,        ///< a paid licence is active
        Evaluation, ///< inside the 10-day trial
        Free,       ///< trial over, nothing purchased
    };
    Q_ENUM(Tier)

    static constexpr int kFreeDownloadsPerDay = 5;

    LicenseService(core::Settings& settings, QObject* parent = nullptr);
    ~LicenseService() override;

    /// Loads or migrates the account ID and starts the first status check.
    void start();

    [[nodiscard]] QString accountId() const { return m_accountId; }
    [[nodiscard]] Tier tier() const;
    /// A paid licence is active (the module's hasPaidAccess also counts the evaluation).
    [[nodiscard]] bool isPro() const { return m_status.active; }
    /// Pro features may run: paid, or inside the evaluation.
    [[nodiscard]] bool hasAccess() const { return m_status.hasPaidAccess; }
    [[nodiscard]] bool evaluationActive() const { return m_status.evaluationActive; }
    [[nodiscard]] int evaluationDaysRemaining() const { return m_status.evaluationDaysRemaining; }
    [[nodiscard]] bool evaluationEnded() const;
    [[nodiscard]] int evaluationDays() const;
    [[nodiscard]] bool checking() const { return m_checking; }
    [[nodiscard]] const AccountLicenseManager::LicenseStatus& status() const { return m_status; }
    [[nodiscard]] bool migratedFromRed9() const { return m_migrated; }

    /// True when `feature` may run; otherwise emits upgradeRequested(feature).
    bool checkAccess(const QString& feature);

    /// Downloads still allowed today; -1 = unlimited (paid or evaluation).
    [[nodiscard]] int downloadsRemainingToday() const;
    /// True when `items` more videos may be queued today; otherwise emits
    /// downloadLimitReached(items, remaining). Does not count them.
    bool canDownload(int items);
    /// Counts `items` queued videos against today's free allowance.
    void spendDownloads(int items);

    [[nodiscard]] QString checkoutUrl() const;
    [[nodiscard]] QString portalUrl() const;
    /// Re-verifies with the server now (the "Refresh" button).
    void refresh();

Q_SIGNALS:
    void statusChanged();
    void checkingChanged(bool checking);
    void upgradeRequested(const QString& feature);
    /// The free tier asked for more than today allows.
    void downloadLimitReached(int requested, int remaining);

private:
    core::Settings& m_settings;
    AccountLicenseManager m_manager;
    AccountLicenseManager::LicenseStatus m_status;
    QString m_accountId;
    bool m_checking = false;
    bool m_migrated = false;
};

} // namespace red::services
