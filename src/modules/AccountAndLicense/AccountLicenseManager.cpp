// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#include "AccountLicenseManager.h"
#include "AccountLicenseStore.h"

#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

// ── private helpers (no external deps) ───────────────────────────────────────
namespace {

/// Generates a random hex string of the requested length.
/// Mirrors Utils::generateRandomId() so callers get the same format.
QString al_generateId(int length)
{
    QString id = QUuid::createUuid().toString(QUuid::Id128);
    while (id.length() < length)
        id += QUuid::createUuid().toString(QUuid::Id128);
    return id.left(length);
}

} // anonymous namespace

// ── AccountLicenseManager ─────────────────────────────────────────────────────

AccountLicenseManager::AccountLicenseManager(const AccountLicenseConfig &config,
                                             QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_store(new AccountLicenseStore(config))
{
}

AccountLicenseManager::~AccountLicenseManager()
{
    delete m_store;
}

// ── Core operations ───────────────────────────────────────────────────────────

QString AccountLicenseManager::serial() const
{
    return m_serial;
}

void AccountLicenseManager::ensureSerial(const QString &legacySerial)
{
    // 1. Try obfuscated file
    m_serial = loadSerialFromFile();

    // 2. Try QSettings (base64-encoded)
    if (m_serial.isEmpty())
        m_serial = m_store->serial();

    // Nothing stored yet: keep the account ID an earlier version of the app
    // created, so an existing licence keeps working.
    if (m_serial.trimmed().isEmpty() && !legacySerial.trimmed().isEmpty())
        m_serial = legacySerial.trimmed();

    // 3. Generate new
    if (m_serial.trimmed().isEmpty() || m_serial.length() < 8) {
        // Use the config-supplied prefix; fall back to platform auto-detection
        // so the API can distinguish Windows installs without an extra field.
        QString prefix = m_config.serialPlatformPrefix;
        if (prefix.isNull()) {   // null = "not set" → auto-detect
#ifdef Q_OS_WIN
            prefix = QStringLiteral("win");
#endif
        }
        m_serial = prefix + generateId(24);
    }

    // Persist in both locations
    saveSerialToFile(m_serial);
    m_store->setSerial(m_serial);

    const qint64 nowUtc = nowUtcSecs();
    if (m_store->value(QStringLiteral("license/eval_start_utc"), 0LL).toLongLong() <= 0)
        m_store->setValue(QStringLiteral("license/eval_start_utc"), nowUtc);

    detectLocalAnomaly(m_serial, nowUtc);

    const LicenseStatus cached = buildCachedStatus();
    m_hasPremiumAccess = cached.hasPaidAccess;
    m_evaluationActive = cached.evaluationActive;

    Q_EMIT serialReady(m_serial);
}

void AccountLicenseManager::checkPurchase(const QString &serial, bool force)
{
#ifdef QT_DEBUG
    if (m_debugLicenseState != DebugLicenseState::None) {
        const LicenseStatus s = buildDebugStatus(m_debugLicenseState);
        m_hasPremiumAccess    = s.hasPaidAccess;
        m_evaluationActive    = s.evaluationActive;
        Q_EMIT licenseStatusUpdated(s);
        Q_EMIT licenseCheckInProgressChanged(false);
        return;
    }
#endif

    const QString accountId = serial.trimmed();
    if (accountId.isEmpty()) {
        LicenseStatus status;
        status.message          = QStringLiteral("Account ID is not available");
        status.fromNetworkError = true;
        Q_EMIT licenseStatusUpdated(status);
        Q_EMIT licenseCheckInProgressChanged(false);
        return;
    }

    const LicenseStatus cached = buildCachedStatus();
    m_hasPremiumAccess = cached.hasPaidAccess;
    m_evaluationActive = cached.evaluationActive;

    if (!shouldPerformNetworkCheck(cached, force)) {
        Q_EMIT licenseStatusUpdated(cached);
        Q_EMIT licenseCheckInProgressChanged(false);
        return;
    }

    QUrl url(m_config.checkStatusEndpoint);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("account_id"), accountId);
    query.addQueryItem(QStringLiteral("app_code"),   m_config.appCode);
    query.addQueryItem(QStringLiteral("format"),     QStringLiteral("json"));
    url.setQuery(query);

    QNetworkRequest req(url);
    if (m_pendingChecks == 0)
        Q_EMIT licenseCheckInProgressChanged(true);
    ++m_pendingChecks;

    QNetworkReply *reply = m_nam.get(req);
    reply->setProperty("accountId", accountId);
    connect(reply, &QNetworkReply::finished,
            this,  &AccountLicenseManager::onCheckRequestFinished);
}

// ── State queries ─────────────────────────────────────────────────────────────

bool AccountLicenseManager::hasPremiumAccess() const   { return m_hasPremiumAccess; }
bool AccountLicenseManager::isEvaluationActive() const { return m_evaluationActive; }

// ── URL builders ──────────────────────────────────────────────────────────────

QString AccountLicenseManager::checkoutUrl() const
{
    return m_config.checkoutUrlTemplate.arg(m_serial);
}

QString AccountLicenseManager::selfServicePortalUrl() const
{
    return m_config.selfServicePortalUrl;
}


// ── Policy accessors ──────────────────────────────────────────────────────────


const AccountLicenseConfig &AccountLicenseManager::config() const
{
    return m_config;
}

QVariant AccountLicenseManager::storeValue(const QString &key,
                                            const QVariant &defaultValue) const
{
    return m_store->value(key, defaultValue);
}

void AccountLicenseManager::setStoreValue(const QString &key,
                                           const QVariant &value)
{
    m_store->setValue(key, value);
}

// ── Network reply slot ────────────────────────────────────────────────────────

void AccountLicenseManager::onCheckRequestFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (!reply) return;
    reply->deleteLater();

    auto completeCheck = [this]() {
        if (m_pendingChecks <= 0) return;
        --m_pendingChecks;
        if (m_pendingChecks == 0)
            Q_EMIT licenseCheckInProgressChanged(false);
    };

    LicenseStatus status;
    const QString accountId = reply->property("accountId").toString();
    const qint64  nowUtc    = nowUtcSecs();
    detectLocalAnomaly(accountId, nowUtc);

    if (reply->error() != QNetworkReply::NoError) {
        status                  = buildCachedStatus();
        status.message          = QStringLiteral(
            "Could not verify account status (network error). Using cached access state.");
        status.fromNetworkError = true;
        m_hasPremiumAccess      = status.hasPaidAccess;
        m_evaluationActive      = status.evaluationActive;
        Q_EMIT licenseStatusUpdated(status);
        completeCheck();
        return;
    }

    const QByteArray payload = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        status                  = buildCachedStatus();
        status.message          = QStringLiteral(
            "Invalid server response. Using cached access state.");
        status.fromNetworkError = true;
        m_hasPremiumAccess      = status.hasPaidAccess;
        m_evaluationActive      = status.evaluationActive;
        Q_EMIT licenseStatusUpdated(status);
        completeCheck();
        return;
    }

    const QJsonObject obj = json.object();
    status.active      = obj.value(QStringLiteral("active")).toBool(false);
    status.message     = obj.value(QStringLiteral("message")).toString();
    status.licenseType = obj.value(QStringLiteral("license_type")).toString();

    const QJsonValue expiryDateVal = obj.value(QStringLiteral("expiry_date"));
    if (expiryDateVal.isString()) {
        status.expiryDate    = expiryDateVal.toString();
        status.hasExpiryDate = !status.expiryDate.trimmed().isEmpty();
    }

    const QJsonValue expiryTsVal = obj.value(QStringLiteral("expiry_timestamp"));
    if (expiryTsVal.isDouble()) {
        status.expiryTimestamp    = static_cast<qint64>(expiryTsVal.toDouble());
        status.hasExpiryTimestamp = true;
    } else if (expiryTsVal.isString()) {
        bool ok = false;
        const qint64 parsed = expiryTsVal.toString().toLongLong(&ok);
        if (ok) {
            status.expiryTimestamp    = parsed;
            status.hasExpiryTimestamp = true;
        }
    }

    const QJsonValue daysVal = obj.value(QStringLiteral("days_remaining"));
    if (daysVal.isDouble()) {
        status.daysRemaining    = daysVal.toInt(-1);
        status.hasDaysRemaining = true;
    } else if (daysVal.isString()) {
        bool ok = false;
        const int parsed = daysVal.toString().toInt(&ok);
        if (ok) {
            status.daysRemaining    = parsed;
            status.hasDaysRemaining = true;
        }
    }

    m_store->setActivated(status.active);
    m_store->setValue(QStringLiteral("license/last_check_utc"),      nowUtc);
    m_store->setValue(QStringLiteral("license/type"),                 status.licenseType);
    m_store->setValue(QStringLiteral("license/expiry_date"),          status.expiryDate);
    m_store->setValue(QStringLiteral("license/expiry_timestamp"),     status.expiryTimestamp);
    m_store->setValue(QStringLiteral("license/next_check_utc"),
        defaultNextCheckUtc(status.licenseType, nowUtc,
            m_store->value(QStringLiteral("license/anomaly_detected"), false).toBool()));

    LicenseStatus merged            = buildCachedStatus();
    merged.fromNetworkError         = false;
    merged.expiryDate               = status.expiryDate;
    merged.expiryTimestamp          = status.expiryTimestamp;
    merged.hasExpiryDate            = status.hasExpiryDate;
    merged.hasExpiryTimestamp       = status.hasExpiryTimestamp;
    merged.daysRemaining            = status.daysRemaining;
    merged.hasDaysRemaining         = status.hasDaysRemaining;
    if (!status.message.trimmed().isEmpty())
        merged.message = status.message;

    m_hasPremiumAccess = merged.hasPaidAccess;
    m_evaluationActive = merged.evaluationActive;
    Q_EMIT licenseStatusUpdated(merged);
    completeCheck();
}

// ── Internal helpers ──────────────────────────────────────────────────────────

/*static*/ qint64 AccountLicenseManager::nowUtcSecs()
{
    return QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
}

/*static*/ QString AccountLicenseManager::normalizeLicenseType(const QString &type)
{
    return type.trimmed().toLower();
}

qint64 AccountLicenseManager::defaultNextCheckUtc(const QString &licenseType,
                                                   qint64 nowUtc,
                                                   bool anomaly) const
{
    if (!anomaly
        && normalizeLicenseType(licenseType).contains(QStringLiteral("lifetime")))
        return nowUtc + m_config.lifetimeRecheckSecs;
    return nowUtc + m_config.weeklyCheckSecs;
}

bool AccountLicenseManager::detectLocalAnomaly(const QString &accountId,
                                               qint64 nowUtc)
{
    bool anomaly =
        m_store->value(QStringLiteral("license/anomaly_detected"), false).toBool();

    const qint64 lastSeen =
        m_store->value(QStringLiteral("license/last_seen_utc"), 0LL).toLongLong();
    if (lastSeen > 0
        && (nowUtc + m_config.clockSkewToleranceSecs) < lastSeen)
        anomaly = true;

    const QString lastAccount =
        m_store->value(QStringLiteral("license/account_id")).toString().trimmed();
    if (!lastAccount.isEmpty() && !accountId.isEmpty()
        && lastAccount != accountId)
        anomaly = true;

    m_store->setValue(QStringLiteral("license/last_seen_utc"), nowUtc);
    if (!accountId.isEmpty())
        m_store->setValue(QStringLiteral("license/account_id"), accountId);
    m_store->setValue(QStringLiteral("license/anomaly_detected"), anomaly);
    return anomaly;
}

AccountLicenseManager::LicenseStatus AccountLicenseManager::buildCachedStatus() const
{
    LicenseStatus status;
    const qint64 nowUtc = nowUtcSecs();

    const qint64 evalStartUtc =
        m_store->value(QStringLiteral("license/eval_start_utc"), 0LL).toLongLong();
    const qint64 evalEndUtc = evalStartUtc > 0
        ? (evalStartUtc + m_config.evaluationDurationSecs) : 0LL;

    status.active = m_store->isActivated();
    status.licenseType =
        m_store->value(QStringLiteral("license/type")).toString();
    status.expiryDate =
        m_store->value(QStringLiteral("license/expiry_date")).toString();
    status.expiryTimestamp =
        m_store->value(QStringLiteral("license/expiry_timestamp"), 0LL).toLongLong();
    status.nextCheckTimestamp =
        m_store->value(QStringLiteral("license/next_check_utc"), 0LL).toLongLong();
    status.anomalyDetected =
        m_store->value(QStringLiteral("license/anomaly_detected"), false).toBool();

    status.evaluationEndTimestamp = evalEndUtc;
    status.evaluationActive       = !status.active && (evalEndUtc > nowUtc);
    status.hasPaidAccess          = status.active || status.evaluationActive;
    status.hasExpiryDate          = !status.expiryDate.trimmed().isEmpty();
    status.hasExpiryTimestamp     = status.expiryTimestamp > 0;

    if (status.active && status.hasExpiryTimestamp
        && !normalizeLicenseType(status.licenseType)
                .contains(QStringLiteral("lifetime"))) {
        const qint64 rem = qMax<qint64>(0, status.expiryTimestamp - nowUtc);
        status.daysRemaining =
            static_cast<int>((rem + (24 * 60 * 60 - 1)) / (24 * 60 * 60));
        status.hasDaysRemaining = true;
    }

    if (status.evaluationActive) {
        const qint64 rem = qMax<qint64>(0, evalEndUtc - nowUtc);
        status.evaluationDaysRemaining =
            static_cast<int>((rem + (24 * 60 * 60 - 1)) / (24 * 60 * 60));
    }

    if (status.active) {
        status.message = QStringLiteral("License is active and valid");
    } else if (status.evaluationActive) {
        status.message = QStringLiteral("Evaluation active");
    } else {
        status.message = QStringLiteral("Evaluation period ended");
    }

    return status;
}

bool AccountLicenseManager::shouldPerformNetworkCheck(const LicenseStatus &cached,
                                                       bool forceCheck) const
{
    if (forceCheck)                            return true;
    if (cached.nextCheckTimestamp <= 0)        return true;
    if (cached.anomalyDetected)                return true;

    const qint64 nowUtc = nowUtcSecs();
    if (cached.active
        && normalizeLicenseType(cached.licenseType)
               .contains(QStringLiteral("lifetime")))
        return false;

    return nowUtc >= cached.nextCheckTimestamp;
}

// ── Serial file helpers ───────────────────────────────────────────────────────

// The obfuscated-path helper lives in AccountLicenseStore.cpp's anonymous
// namespace.  We replicate just the path-only logic here using the same
// leaf names from the config so both storage layers stay in sync.
// Internally we use QStandardPaths and the same directory structure that
// Utils::obfuscatedSubPath() produces — but we rely on the Store to own
// the full obfuscation logic.  Here we simply mirror the path calculation.

#include <QCryptographicHash>

namespace {
static QString al_serialShortHash(const QString &input)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha1)
            .toHex()
            .left(8));
}

static QString al_buildSerialFilePath(const QString &dataRoot,
                                      const QString &dirLeaf,
                                      const QString &fileName)
{
    // Mirror the first layers of obfuscatedSubPath() deterministically.
    static const QStringList layers = {
        QStringLiteral("._w_ww"),
        QStringLiteral("._meta"),
        QStringLiteral(".r9"),
        QStringLiteral("._sys"),
        QStringLiteral(".n3"),
        QStringLiteral("._rt"),
        QStringLiteral(".v1"),
    };
    const QString base     = QDir::cleanPath(dataRoot);
    const QString seedBase = base + QStringLiteral("|") + dirLeaf;

    QString out = base;
    const QChar sep = QDir::separator();
    for (int i = 0; i < layers.size(); ++i) {
        out += sep + layers.at(i);
        out += sep + QStringLiteral("._x_%1")
                         .arg(al_serialShortHash(
                             seedBase + QStringLiteral("|noise|") + QString::number(i)));
    }
    out += sep + dirLeaf;
    QDir().mkpath(out);
    return QDir(out).filePath(fileName);
}
} // anonymous namespace

QString AccountLicenseManager::serialFilePath() const
{
    const QString dataRoot =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return al_buildSerialFilePath(dataRoot,
                                  m_config.serialStoreDirLeaf,
                                  m_config.serialStoreFileName);
}

void AccountLicenseManager::saveSerialToFile(const QString &serial) const
{
    QFile f(serialFilePath());
    if (f.open(QIODevice::WriteOnly)) {
        QDataStream out(&f);
        out << serial;
    }
}

QString AccountLicenseManager::loadSerialFromFile() const
{
    QFile f(serialFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QString s;
    QDataStream in(&f);
    in >> s;
    return s;
}

/*static*/ QString AccountLicenseManager::generateId(int length)
{
    return al_generateId(length);
}

// ── Debug simulation (QT_DEBUG only) ─────────────────────────────────────────
#ifdef QT_DEBUG

void AccountLicenseManager::setDebugLicenseState(DebugLicenseState state)
{
    m_debugLicenseState = state;
}

AccountLicenseManager::DebugLicenseState
AccountLicenseManager::debugLicenseState() const
{
    return m_debugLicenseState;
}

/*static*/ AccountLicenseManager::LicenseStatus
AccountLicenseManager::buildDebugStatus(DebugLicenseState state)
{
    LicenseStatus s;
    const qint64 now = QDateTime::currentDateTimeUtc().toSecsSinceEpoch();

    switch (state) {
    case DebugLicenseState::Pro:
        s.active             = true;
        s.hasPaidAccess      = true;
        s.licenseType        = QStringLiteral("pro");
        s.message            = QStringLiteral("[DEBUG] Pro licence active");
        s.expiryDate         = QStringLiteral("2027-01-01");
        s.hasExpiryDate      = true;
        s.expiryTimestamp    = now + 365LL * 24LL * 3600LL;
        s.hasExpiryTimestamp = true;
        s.daysRemaining      = 365;
        s.hasDaysRemaining   = true;
        break;

    case DebugLicenseState::EvaluationActive:
        s.evaluationActive        = true;
        s.hasPaidAccess           = true;
        s.licenseType             = QStringLiteral("evaluation");
        s.message                 = QStringLiteral("[DEBUG] Evaluation active");
        s.evaluationEndTimestamp  = now + 7LL * 24LL * 3600LL;
        s.evaluationDaysRemaining = 7;
        break;

    case DebugLicenseState::EvaluationExpired:
        // evaluationEndTimestamp must be > 0 so callers can distinguish
        // "expired" from "never started" (evaluationEndTimestamp == 0).
        // Use a timestamp 10 days in the past (one day past the default trial).
        s.evaluationEndTimestamp = now - 10LL * 24LL * 3600LL;
        s.message = QStringLiteral("[DEBUG] Evaluation expired — Free tier");
        break;

    case DebugLicenseState::NetworkError:
        s.fromNetworkError = true;
        s.message          = QStringLiteral("[DEBUG] Simulated network error");
        break;

    default:
        break;
    }
    return s;
}

#endif // QT_DEBUG

