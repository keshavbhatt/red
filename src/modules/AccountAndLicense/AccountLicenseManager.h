#pragma once

#include "AccountLicenseConfig.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QVariant>

class AccountLicenseStore;

/**
 * @brief App-agnostic manager for serial IDs and remote licence verification.
 *
 * Handles:
 *  - Serial / account-ID generation and obfuscated persistence
 *  - Asynchronous remote licence-status checks via a configurable API endpoint
 *  - Local anomaly detection (clock skew, account-ID drift)
 *  - Evaluation period tracking
 *  - Weekly / lifetime re-check scheduling
 *
 * Configure it once via AccountLicenseConfig and reuse across projects.
 *
 * @par Typical usage
 * @code
 * AccountLicenseConfig cfg = makeMyAppConfig();
 * auto *mgr = new AccountLicenseManager(cfg, this);
 * connect(mgr, &AccountLicenseManager::licenseStatusUpdated,
 *         this, &MyWindow::onLicenseStatus);
 * mgr->ensureSerial();
 * mgr->checkPurchase(mgr->serial());
 * @endcode
 */
class AccountLicenseManager : public QObject
{
    Q_OBJECT
public:
    /// Full description of the current licence state.
    struct LicenseStatus {
        bool    active                  = false;
        bool    evaluationActive        = false;
        bool    hasPaidAccess           = false;
        QString message;
        QString expiryDate;
        qint64  expiryTimestamp         = 0;
        qint64  evaluationEndTimestamp  = 0;
        qint64  nextCheckTimestamp      = 0;
        QString licenseType;
        int     daysRemaining           = -1;
        int     evaluationDaysRemaining = -1;
        bool    hasExpiryDate           = false;
        bool    hasExpiryTimestamp      = false;
        bool    hasDaysRemaining        = false;
        bool    fromNetworkError        = false;
        bool    anomalyDetected         = false;
    };

    explicit AccountLicenseManager(const AccountLicenseConfig &config,
                                   QObject *parent = nullptr);
    ~AccountLicenseManager() override;

    // ── Core operations ───────────────────────────────────────────────────────

    /// The current serial / account ID (empty until ensureSerial() is called).
    QString serial() const;

    /// Load serial from persistent storage or generate a new one, then emit
    /// serialReady() and initialise the local licence state.
    /// @p legacySerial, when set, is adopted if no serial is stored yet
    /// (migration of an existing installation's account ID).
    void ensureSerial(const QString &legacySerial = {});

    /// Asynchronously verify @p serial against the remote licence server.
    /// @p force bypasses weekly re-check scheduling.
    void checkPurchase(const QString &serial, bool force = false);

    // ── State queries ─────────────────────────────────────────────────────────
    bool hasPremiumAccess()   const;
    bool isEvaluationActive() const;

    // ── Config-driven URL builders ────────────────────────────────────────────
    QString checkoutUrl()          const;
    QString selfServicePortalUrl() const;

    // ── Policy accessors ──────────────────────────────────────────────────────

    const AccountLicenseConfig &config() const;

    // ── Encrypted app-level key/value storage ─────────────────────────────────
    /// Read a value from the encrypted store.
    /// Keys prefixed with "license/" are stored obfuscated and XOR-encrypted.
    QVariant storeValue(const QString &key,
                        const QVariant &defaultValue = {}) const;
    /// Write a value to the encrypted store.
    void setStoreValue(const QString &key, const QVariant &value);

Q_SIGNALS:
    void serialReady(const QString &serial);
    void licenseCheckInProgressChanged(bool inProgress);
    void licenseStatusUpdated(const AccountLicenseManager::LicenseStatus &status);

private Q_SLOTS:
    void onCheckRequestFinished();

private:
    // Internal helpers
    static qint64  nowUtcSecs();
    static QString normalizeLicenseType(const QString &type);
    qint64  defaultNextCheckUtc(const QString &licenseType,
                                qint64 nowUtc, bool anomaly) const;
    bool    detectLocalAnomaly(const QString &accountId, qint64 nowUtc);
    LicenseStatus buildCachedStatus() const;
    bool shouldPerformNetworkCheck(const LicenseStatus &cached,
                                   bool forceCheck) const;

    // Serial file storage
    QString serialFilePath()                       const;
    void    saveSerialToFile(const QString &serial) const;
    QString loadSerialFromFile()                    const;

    // ID generation
    static QString generateId(int length = 24);

    AccountLicenseConfig  m_config;
    AccountLicenseStore  *m_store           = nullptr;
    bool                  m_hasPremiumAccess = false;
    bool                  m_evaluationActive = false;
    QNetworkAccessManager m_nam;

#ifdef QT_DEBUG
// ── Debug simulation (debug builds only) ─────────────────────────────────────
public:
    /**
     * @brief Simulated licence states available in debug builds.
     *
     * Set via setDebugLicenseState() before calling checkPurchase() to have
     * the manager emit a crafted LicenseStatus immediately without any
     * network call or settings read.  The full signal chain
     * (AccountDialog, MainWindow, Share gating) is exercised just as in
     * production.
     *
     * Set to DebugLicenseState::None (the default) to restore normal behaviour.
     */
    enum class DebugLicenseState {
        None,              ///< Real licence check (default)
        Pro,               ///< Active Pro licence, 365 days remaining
        EvaluationActive,  ///< Evaluation period, 7 days remaining
        EvaluationExpired, ///< Evaluation ended — Free tier
        NetworkError,      ///< Simulated network error (cached state)
    };
    Q_ENUM(DebugLicenseState)

    void             setDebugLicenseState(DebugLicenseState state);
    DebugLicenseState debugLicenseState() const;

private:
    static LicenseStatus buildDebugStatus(DebugLicenseState state);
    DebugLicenseState m_debugLicenseState = DebugLicenseState::None;
#endif
    QString               m_serial;
    int                   m_pendingChecks    = 0;
};

