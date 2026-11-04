// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#include "AppCatalog.h"

#include "AppLinks.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <algorithm>

using namespace Qt::StringLiterals;

// Q_INIT_RESOURCE must not be called from inside a namespace.
static void initKtechpitAppsResources()
{
    Q_INIT_RESOURCE(ktechpit_apps);
}

namespace ktechpit {

namespace {

constexpr int kSchema = 1;
QPointer<AppCatalog> s_instance;

QString userAgent(const QString& host)
{
    return u"KtechpitApps/1 (%1)"_s.arg(host.isEmpty() ? u"unknown"_s : host);
}

QStringList stringList(const QJsonValue& value)
{
    QStringList out;
    const QJsonArray array = value.toArray();
    for (const QJsonValue& v : array) {
        if (v.isString() && !v.toString().isEmpty()) {
            out << v.toString();
        }
    }
    return out;
}

} // namespace

void AppCatalog::init(const CatalogConfig& config)
{
    if (s_instance) {
        return;
    }
    s_instance = new AppCatalog(config, QCoreApplication::instance());
}

AppCatalog* AppCatalog::instance()
{
    if (!s_instance) {
        init(CatalogConfig{});
    }
    return s_instance;
}

AppCatalog::AppCatalog(const CatalogConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
{
    initKtechpitAppsResources();
    load();
    QTimer::singleShot(std::max(0, m_config.refreshDelayMs), this, [this] {
        const QFileInfo cached(cacheDir() + u"/catalog.json"_s);
        const bool stale = !cached.exists()
                           || cached.lastModified().daysTo(QDateTime::currentDateTime()) >= m_config.refreshDays;
        if (stale) {
            refresh();
        }
    });
}

QString AppCatalog::cacheDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/ktechpit-apps"_s;
}

void AppCatalog::load()
{
    // The cached download wins over the bundled copy when it parses.
    const QStringList paths{cacheDir() + u"/catalog.json"_s, u":/ktechpit-apps/catalog.json"_s};
    for (const QString& path : paths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly) && adopt(file.readAll())) {
            return;
        }
    }
}

bool AppCatalog::adopt(const QByteArray& json)
{
    bool ok = false;
    QList<AppInfo> apps = parse(json, &ok);
    if (!ok || apps.isEmpty()) {
        return false;
    }
    m_apps = std::move(apps);
    Q_EMIT changed();
    return true;
}

void AppCatalog::refresh()
{
    if (!m_network) {
        m_network = new QNetworkAccessManager(this);
        m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        m_network->setTransferTimeout(30000);
    }
    QNetworkRequest request(m_config.catalogUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent(m_config.hostAppId));
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            return; // keep what we have; the next start tries again
        }
        const QByteArray body = reply->readAll();
        if (!adopt(body)) {
            return;
        }
        QDir().mkpath(cacheDir());
        QSaveFile file(cacheDir() + u"/catalog.json"_s);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(body);
            file.commit();
        }
    });
}

QList<AppInfo> AppCatalog::parse(const QByteArray& json, bool* ok)
{
    if (ok) {
        *ok = false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    const QJsonObject root = doc.object();
    if (root.value(u"schema"_s).toInt() != kSchema) {
        return {};
    }
    QList<AppInfo> apps;
    const QJsonArray array = root.value(u"apps"_s).toArray();
    for (const QJsonValue& v : array) {
        const QJsonObject o = v.toObject();
        AppInfo app;
        app.id = o.value(u"id"_s).toString();
        app.name = o.value(u"name"_s).toString();
        if (app.id.isEmpty() || app.name.isEmpty()) {
            continue;
        }
        app.title = o.value(u"title"_s).toString(app.name);
        app.summary = o.value(u"summary"_s).toString();
        app.type = o.value(u"type"_s).toString(u"freeware"_s);
        app.platforms = stringList(o.value(u"platforms"_s));
        app.tags = stringList(o.value(u"tags"_s));
        app.trending = o.value(u"trending"_s).toInt(0);
        app.icon = QUrl(o.value(u"icon"_s).toString());
        app.page = QUrl(o.value(u"page"_s).toString());
        app.snap = o.value(u"snap"_s).toString();
        app.flatpak = o.value(u"flatpak"_s).toString();
        apps << app;
    }
    if (ok) {
        *ok = true;
    }
    return apps;
}

QString AppCatalog::currentPlatform()
{
#if defined(Q_OS_WIN)
    return u"Windows"_s;
#elif defined(Q_OS_MACOS)
    return u"macOS"_s;
#else
    return u"Linux"_s;
#endif
}

QList<AppInfo> AppCatalog::rank(const QList<AppInfo>& apps, const QString& hostId, int count)
{
    QStringList hostTags;
    for (const AppInfo& app : apps) {
        if (app.id == hostId) {
            hostTags = app.tags;
        }
    }
    struct Scored {
        AppInfo app;
        int shared;
        int order;
    };
    QList<Scored> pool;
    int order = 0;
    for (const AppInfo& app : apps) {
        if (app.id == hostId) {
            continue;
        }
        int shared = 0;
        for (const QString& tag : app.tags) {
            shared += hostTags.contains(tag) ? 1 : 0;
        }
        pool.append({app, shared, order++});
    }
    // Most shared tags first; then trending apps by rank; then the catalog's own order.
    std::stable_sort(pool.begin(), pool.end(), [](const Scored& a, const Scored& b) {
        if (a.shared != b.shared) {
            return a.shared > b.shared;
        }
        const int ta = a.app.trending > 0 ? a.app.trending : 1000;
        const int tb = b.app.trending > 0 ? b.app.trending : 1000;
        if (ta != tb) {
            return ta < tb;
        }
        return a.order < b.order;
    });
    QList<AppInfo> out;
    for (const Scored& s : std::as_const(pool)) {
        if (count >= 0 && out.size() >= count) {
            break;
        }
        out << s.app;
    }
    return out;
}

QList<AppInfo> AppCatalog::apps() const
{
    const QString platform = currentPlatform();
    QList<AppInfo> out;
    for (const AppInfo& app : m_apps) {
        if (app.id == m_config.hostAppId) {
            out << app; // kept for rank(), which reads the host's tags and drops it
            continue;
        }
        if (!app.platforms.isEmpty() && !app.platforms.contains(platform)) {
            continue;
        }
        if (AppLinks::isInstalled(app)) {
            continue;
        }
        out << app;
    }
    return rank(out, m_config.hostAppId, -1);
}

QList<AppInfo> AppCatalog::related(int count) const
{
    return apps().mid(0, count);
}

void AppCatalog::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    Q_EMIT enabledChanged(enabled);
}

QPixmap AppCatalog::placeholderIcon(const QString& name, int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    // A stable colour per name, so the tile does not change between runs.
    const uint hash = qHash(name, 0U);
    p.setBrush(QColor::fromHsl(static_cast<int>(hash % 360U), 140, 110));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(0, 0, size, size), size * 0.22, size * 0.22);
    QFont font = p.font();
    font.setBold(true);
    font.setPixelSize(std::max(8, size / 2));
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, name.left(1).toUpper());
    return pixmap;
}

QPixmap AppCatalog::icon(const AppInfo& app, int size) const
{
    const qreal dpr = qGuiApp ? qGuiApp->devicePixelRatio() : 1.0;
    const int px = static_cast<int>(size * dpr);
    const QString cached = cacheDir() + u"/icons/"_s + app.id + u".png"_s;
    const QStringList paths{cached, u":/ktechpit-apps/icons/"_s + app.id + u".png"_s};
    for (const QString& path : paths) {
        QPixmap pixmap(path);
        if (!pixmap.isNull()) {
            pixmap = pixmap.scaled(px, px, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            pixmap.setDevicePixelRatio(dpr);
            return pixmap;
        }
    }
    fetchIcon(app);
    return placeholderIcon(app.name, size);
}

void AppCatalog::fetchIcon(const AppInfo& app) const
{
    if (!app.icon.isValid() || m_iconRequests.contains(app.id)) {
        return;
    }
    m_iconRequests.insert(app.id);
    auto* self = const_cast<AppCatalog*>(this);
    if (!self->m_network) {
        self->m_network = new QNetworkAccessManager(self);
        self->m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
        self->m_network->setTransferTimeout(30000);
    }
    QNetworkRequest request(app.icon);
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent(m_config.hostAppId));
    QNetworkReply* reply = self->m_network->get(request);
    const QString id = app.id;
    connect(reply, &QNetworkReply::finished, self, [self, reply, id] {
        reply->deleteLater();
        QPixmap pixmap;
        if (reply->error() != QNetworkReply::NoError || !pixmap.loadFromData(reply->readAll())) {
            return;
        }
        const QString dir = self->cacheDir() + u"/icons"_s;
        QDir().mkpath(dir);
        if (pixmap.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                .save(dir + u"/"_s + id + u".png"_s, "PNG")) {
            Q_EMIT self->iconReady(id);
        }
    });
}

} // namespace ktechpit
