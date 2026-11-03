// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#include "services/licensing/license_service.h"

#include "core/licensing/legacy_account.h"
#include "services/licensing/license_config.h"
#include "services/logging.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;
#include <QDate>

namespace red::services {

LicenseService::LicenseService(core::Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_manager(redLicenseConfig())
{
    connect(&m_manager, &AccountLicenseManager::licenseCheckInProgressChanged, this, [this](bool inProgress) {
        m_checking = inProgress;
        Q_EMIT checkingChanged(inProgress);
    });
    connect(&m_manager, &AccountLicenseManager::licenseStatusUpdated, this,
            [this](const AccountLicenseManager::LicenseStatus& status) {
                m_status = status;
                qCInfo(lcServices) << "licence:"
                                   << (status.active             ? "pro"
                                       : status.evaluationActive ? "evaluation"
                                                                 : "free")
                                   << status.message;
                Q_EMIT statusChanged();
            });
    connect(&m_manager, &AccountLicenseManager::serialReady, this, [this](const QString& serial) {
        m_accountId = serial;
        Q_EMIT statusChanged();
    });
}

LicenseService::~LicenseService() = default;

void LicenseService::start()
{
#ifdef QT_DEBUG
    // Debug builds only (ADR-010): RED_DEBUG_LICENSE=pro|trial|free|neterr
    // drives the module's simulated states so the gate can be exercised.
    if (const QString sim = qEnvironmentVariable("RED_DEBUG_LICENSE"); !sim.isEmpty()) {
        using S = AccountLicenseManager::DebugLicenseState;
        m_manager.setDebugLicenseState(sim == u"pro"_s     ? S::Pro
                                       : sim == u"trial"_s ? S::EvaluationActive
                                       : sim == u"free"_s  ? S::EvaluationExpired
                                                           : S::NetworkError);
    }
#endif
    // Red 9 users keep their account ID (and with it their licence).
    const QString legacy = core::legacyRedAccountId(core::legacyAccountSearchHomes());
    m_manager.ensureSerial(legacy);
    m_accountId = m_manager.serial();
    m_migrated = !legacy.isEmpty() && legacy == m_accountId;
    if (m_migrated) {
        qCInfo(lcServices) << "account: using the Red 9 account id";
    }
    // Red 9 also kept the ID in ~/Downloads/.red.id; keep that file current so
    // the two versions agree and a wiped config can find the ID again.
    const QString idFile = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + u"/.red.id"_s;
    if (!m_accountId.isEmpty() && !QFile::exists(idFile)) {
        QFile file(idFile);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(m_accountId.toUtf8() + '\n');
        }
    }
    m_status.hasPaidAccess = m_manager.hasPremiumAccess();
    m_status.evaluationActive = m_manager.isEvaluationActive();
    m_status.active = m_status.hasPaidAccess && !m_status.evaluationActive;
    Q_EMIT statusChanged();
    m_manager.checkPurchase(m_accountId);
}

LicenseService::Tier LicenseService::tier() const
{
    if (m_status.active) {
        return Tier::Pro;
    }
    return m_status.evaluationActive ? Tier::Evaluation : Tier::Free;
}

bool LicenseService::evaluationEnded() const
{
    return !m_status.active && !m_status.evaluationActive && m_status.evaluationEndTimestamp > 0;
}

int LicenseService::evaluationDays() const
{
    return static_cast<int>(m_manager.config().evaluationDurationSecs / 86400);
}

bool LicenseService::checkAccess(const QString& feature)
{
    if (m_status.hasPaidAccess || m_status.evaluationActive) {
        return true;
    }
    Q_EMIT upgradeRequested(feature);
    return false;
}

int LicenseService::downloadsRemainingToday() const
{
    if (m_status.hasPaidAccess || m_status.evaluationActive) {
        return -1;
    }
    return m_settings.freeDownloads().remaining(kFreeDownloadsPerDay, QDate::currentDate());
}

bool LicenseService::canDownload(int items)
{
    const int remaining = downloadsRemainingToday();
    if (remaining < 0 || items <= remaining) {
        return true;
    }
    qCInfo(lcServices) << "free download allowance: asked" << items << "left" << remaining;
    Q_EMIT downloadLimitReached(items, remaining);
    return false;
}

void LicenseService::spendDownloads(int items)
{
    if (downloadsRemainingToday() < 0) {
        return; // unlimited: nothing to count
    }
    m_settings.setFreeDownloads(m_settings.freeDownloads().spent(items, QDate::currentDate()));
}

QString LicenseService::checkoutUrl() const
{
    return m_manager.checkoutUrl();
}

QString LicenseService::portalUrl() const
{
    return m_manager.selfServicePortalUrl();
}

void LicenseService::refresh()
{
    m_manager.checkPurchase(m_accountId, /*force=*/true);
}

} // namespace red::services
