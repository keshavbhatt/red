#include "core/licensing/legacy_account.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSettings>
#include <QtEnvironmentVariables>

using namespace Qt::StringLiterals;

namespace red::core {

bool looksLikeAccountId(const QString& id)
{
    static const QRegularExpression kId(u"^[A-Za-z0-9]{8,64}$"_s);
    return kId.match(id).hasMatch();
}

QStringList legacyAccountSearchHomes()
{
    QStringList homes{QDir::homePath()};
    for (const char* var : {"SNAP_REAL_HOME", "SNAP_USER_COMMON", "SNAP_USER_DATA"}) {
        if (const QString v = qEnvironmentVariable(var); !v.isEmpty()) {
            homes << v;
        }
    }
    // A native or Flatpak Red 10 next to the old snap: its data survives.
    const QString real = qEnvironmentVariable("SNAP_REAL_HOME", QDir::homePath());
    homes << real + u"/snap/red-app/current"_s << real + u"/snap/red-app/common"_s;
    homes.removeDuplicates();
    return homes;
}

QString legacyRedAccountId(const QStringList& homes)
{
    for (const QString& home : homes) {
        // QSettings: read the INI file directly rather than binding the whole
        // process to another organisation name.
        const QString conf = home + u"/.config/org.keshavnrj.ubuntu/Red.conf"_s;
        if (QFile::exists(conf)) {
            const QSettings settings(conf, QSettings::IniFormat);
            const QString id = settings.value(u"accountId"_s).toString().trimmed();
            if (looksLikeAccountId(id)) {
                return id;
            }
        }
        QFile file(home + u"/Downloads/.red.id"_s);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString id = QString::fromUtf8(file.readLine()).trimmed();
            if (looksLikeAccountId(id)) {
                return id;
            }
        }
    }
    return {};
}

} // namespace red::core
