#include "AccountLicenseStore.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QMap>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>

// ── internal helpers ──────────────────────────────────────────────────────────
// Self-contained copies of Utils::obfuscatedSubPath() and its support
// functions.  These are private to this translation unit so the module
// stays independent of ww_core.
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// ---- obfuscated-path helpers (ported from Utils.cpp) -----------------------

QString al_shortHash(const QString &input)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha1)
            .toHex()
            .left(8));
}

void al_ensureFileIfMissing(const QString &dirPath,
                            const QString &fileName,
                            const QByteArray &content)
{
    const QString fp = QDir(dirPath).filePath(fileName);
    if (QFile::exists(fp))
        return;
    QFile f(fp);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(content);
}

void al_createDecoyArtifacts(const QString &dirPath, const QString &seed)
{
    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    const QString dA = QStringLiteral("._n_%1").arg(al_shortHash(seed + QStringLiteral("|dA")));
    const QString dB = QStringLiteral("._n_%1").arg(al_shortHash(seed + QStringLiteral("|dB")));
    dir.mkpath(dA);
    dir.mkpath(dB);

    al_ensureFileIfMissing(dirPath,
        QStringLiteral("._meta_%1.dat").arg(al_shortHash(seed + QStringLiteral("|f1"))),
        QByteArrayLiteral("cache=0\n"));
    al_ensureFileIfMissing(dir.filePath(dA), QStringLiteral("._index"),
        QByteArrayLiteral("0\n"));
    al_ensureFileIfMissing(dir.filePath(dB), QStringLiteral("._state"),
        QByteArrayLiteral("ok\n"));
}

void al_createBaseSiblingDecoys(const QString &basePath, const QString &seed)
{
    QDir baseDir(basePath);
    if (!baseDir.exists())
        return;

    const QStringList rootNames = {
        QStringLiteral("._ww"),
        QStringLiteral(".wwx"),
        QStringLiteral("._cache"),
        QStringLiteral(".meta"),
    };

    for (int i = 0; i < rootNames.size(); ++i) {
        const QString folder = rootNames.at(i)
            + QStringLiteral("_")
            + al_shortHash(seed + QStringLiteral("|sib|") + QString::number(i)).left(4);

        baseDir.mkpath(folder);
        const QString full = baseDir.filePath(folder);
        al_createDecoyArtifacts(full, seed + QStringLiteral("|sib-node|") + QString::number(i));

        const QString nested = QStringLiteral("._node_")
            + al_shortHash(seed + QStringLiteral("|sib-nested|") + QString::number(i)).left(5);
        QDir(full).mkpath(nested);
        al_createDecoyArtifacts(QDir(full).filePath(nested),
            seed + QStringLiteral("|sib-nested-node|") + QString::number(i));
    }

    al_ensureFileIfMissing(basePath,
        QStringLiteral("._manifest_%1.log")
            .arg(al_shortHash(seed + QStringLiteral("|manifest")).left(6)),
        QByteArrayLiteral("version=1\n"));
}

/// Deterministic obfuscated path under basePath with decoy siblings.
/// Mirrors Utils::obfuscatedSubPath() so persisted data stays compatible.
QString al_obfuscatedSubPath(const QString &basePath, const QString &leafName)
{
    static const QStringList layers = {
        QStringLiteral("._w_ww"),
        QStringLiteral("._meta"),
        QStringLiteral(".r9"),
        QStringLiteral("._sys"),
        QStringLiteral(".n3"),
        QStringLiteral("._rt"),
        QStringLiteral(".v1"),
    };

    const QString base     = QDir::cleanPath(basePath);
    const QString seedBase = base + QStringLiteral("|") + leafName;

    al_createBaseSiblingDecoys(base, seedBase);

    QString out = base;
    const QChar sep = QDir::separator();

    for (int i = 0; i < layers.size(); ++i) {
        out += sep + layers.at(i);
        out += sep + QStringLiteral("._x_%1")
                         .arg(al_shortHash(seedBase + QStringLiteral("|noise|") + QString::number(i)));
    }
    if (!leafName.isEmpty())
        out += sep + leafName;

    QDir().mkpath(out);

    QString cursor = base;
    for (int i = 0; i < layers.size(); ++i) {
        cursor += sep + layers.at(i);
        QDir().mkpath(cursor);
        al_createDecoyArtifacts(cursor, seedBase + QStringLiteral("|real|") + QString::number(i));

        cursor += sep + QStringLiteral("._x_%1")
                            .arg(al_shortHash(seedBase + QStringLiteral("|noise|") + QString::number(i)));
        QDir().mkpath(cursor);
        al_createDecoyArtifacts(cursor,
            seedBase + QStringLiteral("|noise-node|") + QString::number(i));
    }
    al_createDecoyArtifacts(out, seedBase + QStringLiteral("|leaf"));

    return out;
}

// ---- encryption helpers (ported from AppSettings.cpp) ----------------------

constexpr auto kEncPrefix     = "wwenc:v1:";
constexpr quint32 kBackupMagic   = 0x5757424Bu; // 'WWBK'
constexpr quint32 kBackupVersion = 1u;

QByteArray al_variantToBytes(const QVariant &v)
{
    QByteArray out;
    QDataStream s(&out, QIODevice::WriteOnly);
    s << v;
    return out;
}

QVariant al_bytesToVariant(const QByteArray &bytes)
{
    QVariant v;
    QDataStream s(bytes);
    s >> v;
    return v;
}

QByteArray al_streamBlock(const QByteArray &key,
                          const QByteArray &nonce,
                          quint32           counter)
{
    QByteArray block;
    block += key;
    block += nonce;
    block.append(reinterpret_cast<const char *>(&counter),
                 static_cast<int>(sizeof(counter)));
    return QCryptographicHash::hash(block, QCryptographicHash::Sha256);
}

QByteArray al_xorCrypt(const QByteArray &input,
                       const QByteArray &key,
                       const QByteArray &nonce)
{
    QByteArray out = input;
    QByteArray ks;
    ks.reserve(input.size());
    quint32 counter = 0;
    while (ks.size() < input.size())
        ks += al_streamBlock(key, nonce, counter++);

    for (int i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(out[i] ^ ks[i]);
    return out;
}

QByteArray al_payloadSignature(const QByteArray &key,
                               const QByteArray &nonce,
                               const QByteArray &cipher)
{
    QByteArray data;
    data += key;
    data += QByteArrayLiteral("|sig|");
    data += nonce;
    data += cipher;
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

} // anonymous namespace

// ── AccountLicenseStore ───────────────────────────────────────────────────────

AccountLicenseStore::AccountLicenseStore(const AccountLicenseConfig &config)
    : m_config(config)
    , m_settings(config.settingsOrgName, config.settingsAppName)
{
}

// ---- Sensitive key classification ------------------------------------------

/*static*/
QString AccountLicenseStore::licensePrefix()
{
    return QStringLiteral("license/");
}

bool AccountLicenseStore::isSensitiveKey(const QString &key) const
{
    return key == m_config.activationKey
        || key == m_config.serialKey
        || key.startsWith(licensePrefix());
}

// ---- Activation flag -------------------------------------------------------

bool AccountLicenseStore::isActivated() const
{
    const auto raw = value(m_config.activationKey).toByteArray();
    if (raw.isEmpty())
        return false;
    return QByteArray::fromBase64(raw) == QByteArrayLiteral("activated");
}

void AccountLicenseStore::setActivated(bool activated)
{
    const QByteArray encoded = activated
        ? QByteArrayLiteral("activated").toBase64()
        : QByteArray{};
    setValue(m_config.activationKey, encoded);
}

// ---- Serial ----------------------------------------------------------------

QString AccountLicenseStore::serial() const
{
    const QByteArray raw = value(m_config.serialKey).toByteArray();
    if (raw.isEmpty())
        return {};
    return QString::fromUtf8(QByteArray::fromBase64(raw));
}

void AccountLicenseStore::setSerial(const QString &serial)
{
    setValue(m_config.serialKey, serial.toUtf8().toBase64());
}

// ---- Generic value access --------------------------------------------------

QVariant AccountLicenseStore::value(const QString &key,
                                    const QVariant &defaultValue) const
{
    const QString storedKey = primaryStoreKey(key);

    if (!isSensitiveKey(key))
        return m_settings.value(storedKey, defaultValue);

    const bool    hasPrimary = m_settings.contains(storedKey);
    const QVariant rawPrimary = hasPrimary ? m_settings.value(storedKey) : QVariant{};

    QVariant decrypted;
    if (hasPrimary && decryptVariant(rawPrimary, &decrypted))
        return decrypted;

    QVariant rawBackup;
    if (backupRead(key, &rawBackup) && decryptVariant(rawBackup, &decrypted))
        return decrypted;

    return defaultValue;
}

void AccountLicenseStore::setValue(const QString &key, const QVariant &value)
{
    const QString storedKey = primaryStoreKey(key);

    if (isSensitiveKey(key)) {
        const QVariant encrypted = encryptVariant(value);
        m_settings.setValue(storedKey, encrypted);
        backupWrite(key, encrypted);
    } else {
        m_settings.setValue(storedKey, value);
    }
}

// ---- Key obfuscation -------------------------------------------------------

QString AccountLicenseStore::primaryStoreKey(const QString &key) const
{
    if (!isSensitiveKey(key))
        return key;

    QByteArray material;
    material += m_config.keyAliasPrefix.toUtf8();
    material += key.toUtf8();
    material += QByteArrayLiteral("|");
    material += protectionKey();
    const QByteArray digest =
        QCryptographicHash::hash(material, QCryptographicHash::Sha256).toHex();
    return QStringLiteral("s_") + QString::fromLatin1(digest.left(24));
}

// ---- Encryption ------------------------------------------------------------

QByteArray AccountLicenseStore::protectionKey() const
{
    QByteArray machineId = QSysInfo::machineUniqueId();
    if (machineId.isEmpty())
        machineId = QByteArrayLiteral("fallback-machine-id");

    QByteArray input;
    input += machineId;
    // Prefer explicit config values; fall back to QCoreApplication globals.
    const QString org = m_config.settingsOrgName.isEmpty()
        ? QCoreApplication::organizationName()
        : m_config.settingsOrgName;
    const QString app = m_config.settingsAppName.isEmpty()
        ? QCoreApplication::applicationName()
        : m_config.settingsAppName;
    input += org.toUtf8();
    input += app.toUtf8();
    input += m_config.encryptionKeySuffix.toUtf8();
    return QCryptographicHash::hash(input, QCryptographicHash::Sha256);
}

QVariant AccountLicenseStore::encryptVariant(const QVariant &plain) const
{
    const QByteArray key    = protectionKey();
    const QByteArray nonce  = QUuid::createUuid().toRfc4122();
    const QByteArray cipher = al_xorCrypt(al_variantToBytes(plain), key, nonce);
    const QByteArray sig    = al_payloadSignature(key, nonce, cipher);

    return QString(QString::fromLatin1(kEncPrefix)
        + QString::fromLatin1(nonce.toBase64(
              QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))
        + QLatin1Char(':')
        + QString::fromLatin1(cipher.toBase64(
              QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals))
        + QLatin1Char(':')
        + QString::fromLatin1(sig.toBase64(
              QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)));
}

bool AccountLicenseStore::decryptVariant(const QVariant &stored,
                                         QVariant       *out) const
{
    const QString encoded = stored.toString();
    const QString prefix  = QString::fromLatin1(kEncPrefix);
    if (!encoded.startsWith(prefix))
        return false;

    const QStringList parts = encoded.mid(prefix.size()).split(QLatin1Char(':'));
    if (parts.size() != 3)
        return false;

    const QByteArray nonce  = QByteArray::fromBase64(parts[0].toLatin1(),
                                                      QByteArray::Base64UrlEncoding);
    const QByteArray cipher = QByteArray::fromBase64(parts[1].toLatin1(),
                                                      QByteArray::Base64UrlEncoding);
    const QByteArray sig    = QByteArray::fromBase64(parts[2].toLatin1(),
                                                      QByteArray::Base64UrlEncoding);

    const QByteArray key      = protectionKey();
    const QByteArray expected = al_payloadSignature(key, nonce, cipher);
    if (sig != expected)
        return false;

    if (out)
        *out = al_bytesToVariant(al_xorCrypt(cipher, key, nonce));
    return true;
}

// ---- Binary backup store ---------------------------------------------------

QString AccountLicenseStore::backupStorePath() const
{
    const QString dataRoot =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString secureDir =
        al_obfuscatedSubPath(dataRoot, m_config.backupStoreDirLeaf);
    return QDir(secureDir).filePath(m_config.backupStoreFileName);
}

bool AccountLicenseStore::backupRead(const QString &key, QVariant *out) const
{
    QFile file(backupStorePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    quint32 magic = 0, version = 0;
    QMap<QString, QVariant> store;
    in >> magic >> version;
    if (in.status() != QDataStream::Ok
        || magic != kBackupMagic || version != kBackupVersion)
        return false;
    in >> store;
    if (in.status() != QDataStream::Ok)
        return false;

    const auto it = store.constFind(key);
    if (it == store.constEnd())
        return false;
    if (out)
        *out = it.value();
    return true;
}

void AccountLicenseStore::backupWrite(const QString &key,
                                      const QVariant &storedValue)
{
    // Load existing backup, update, save atomically.
    QMap<QString, QVariant> store;
    {
        QFile file(backupStorePath());
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QDataStream in(&file);
            quint32 magic = 0, version = 0;
            in >> magic >> version;
            if (in.status() == QDataStream::Ok
                && magic == kBackupMagic && version == kBackupVersion) {
                in >> store;
                if (in.status() != QDataStream::Ok)
                    store.clear();
            }
        }
    }
    store.insert(key, storedValue);

    QSaveFile file(backupStorePath());
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << kBackupMagic << kBackupVersion << store;
    if (out.status() == QDataStream::Ok)
        file.commit();
    else
        file.cancelWriting();
}

