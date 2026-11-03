// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#pragma once

#include "AccountLicenseConfig.h"

#include <QMap>
#include <QSettings>
#include <QString>
#include <QVariant>

/**
 * @brief Encrypted QSettings + file-backup persistence for license/serial data.
 *
 * Sensitive keys (activation flag, serial, all "license/" prefixed keys) are:
 *  - stored under SHA-256–hashed key names to prevent plain-text snooping
 *  - XOR-encrypted with a machine-bound key before being written
 *  - duplicated to a hardened binary backup file
 *
 * Non-sensitive keys are stored verbatim.
 *
 * @note This is an internal helper used by AccountLicenseManager.
 *       You do not need to use it directly.
 */
class AccountLicenseStore
{
public:
    explicit AccountLicenseStore(const AccountLicenseConfig &config);

    // --- Activation flag ---
    bool isActivated() const;
    void setActivated(bool activated);

    // --- Serial (base64-encoded in storage) ---
    QString serial() const;
    void    setSerial(const QString &serial);

    // --- Generic key/value (for "license/*" keys and other license data) ---
    QVariant value(const QString &key, const QVariant &defaultValue = {}) const;
    void     setValue(const QString &key, const QVariant &value);

    // --- Classification helpers ---
    bool isSensitiveKey(const QString &key) const;
    static QString licensePrefix();

private:
    // -- Key obfuscation --
    QString primaryStoreKey(const QString &key) const;

    // -- Encryption --
    QByteArray protectionKey() const;
    QVariant   encryptVariant(const QVariant &plain) const;
    bool       decryptVariant(const QVariant &stored, QVariant *out) const;

    // -- Binary backup store --
    QString backupStorePath() const;
    bool    backupRead(const QString &key, QVariant *out) const;
    void    backupWrite(const QString &key, const QVariant &storedValue);

    AccountLicenseConfig m_config;
    mutable QSettings    m_settings;
};

