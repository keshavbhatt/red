#pragma once

#include "AccountLicenseConfig.h"

#include <QCoreApplication>

namespace red::services {

/// Red's AccountLicenseConfig: the shared ktechpit "USS" backend, app code
/// RED (the code Red 9 licences were sold under), a 10-day evaluation.
inline AccountLicenseConfig redLicenseConfig()
{
    AccountLicenseConfig cfg;
    cfg.appCode = QStringLiteral("RED");
    cfg.appName = QStringLiteral("Red");
    cfg.settingsOrgName = QCoreApplication::organizationName();
    cfg.settingsAppName = QCoreApplication::applicationName();
    cfg.checkStatusEndpoint = QStringLiteral("https://ktechpit.com/USS/public/check/status.php");
    cfg.checkoutUrlTemplate =
        QStringLiteral("https://ktechpit.com/USS/public/checkout.php?slug=red&accountId=%1");
    cfg.selfServicePortalUrl = QStringLiteral("https://ktechpit.com/USS/public/user/");
    cfg.evaluationDurationSecs = 10LL * 24LL * 60LL * 60LL;
    return cfg;
}

} // namespace red::services
