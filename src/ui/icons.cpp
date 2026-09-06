#include "ui/icons.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QPainter>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSvgRenderer>

using namespace Qt::StringLiterals;

namespace red::ui::icons {

namespace {

QByteArray tintedSvg(const QString& name, const QColor& color)
{
    QFile file(u":/icons/ui/"_s + name + u".svg"_s);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray svg = file.readAll();
    svg.replace("#000000", color.name(QColor::HexRgb).toLatin1());
    return svg;
}

QPixmap render(const QByteArray& svg, int size, qreal dpr)
{
    QSvgRenderer renderer(svg);
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    if (renderer.isValid()) {
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing);
        renderer.render(&painter, QRectF(0, 0, size, size));
    }
    return pm;
}

QString cacheKey(const QString& name, const QColor& color, int size, qreal dpr)
{
    return name + u'|' + color.name(QColor::HexArgb) + u'|' + QString::number(size) + u'|' +
           QString::number(dpr);
}

} // namespace

QPixmap pixmap(const QString& name, const QColor& color, int size, qreal devicePixelRatio)
{
    static QHash<QString, QPixmap> cache;
    const QString key = cacheKey(name, color, size, devicePixelRatio);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }
    const QPixmap pm = render(tintedSvg(name, color), size, devicePixelRatio);
    cache.insert(key, pm);
    return pm;
}

QIcon themed(const QString& name, const QColor& color, const QColor& disabledColor)
{
    QIcon icon;
    const QColor disabled =
        disabledColor.isValid() ? disabledColor : QColor(color.red(), color.green(), color.blue(), 90);
    for (const int size : {16, 20, 24, 32, 48}) {
        for (const qreal dpr : {1.0, 2.0}) {
            icon.addPixmap(pixmap(name, color, size, dpr), QIcon::Normal);
            icon.addPixmap(pixmap(name, disabled, size, dpr), QIcon::Disabled);
        }
    }
    return icon;
}

QIcon brand()
{
    return QIcon(u":/icons/red.svg"_s);
}

QString tintedFile(const QString& name, const QColor& color)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + u"/icons"_s;
    QDir().mkpath(dir);
    const QString path = dir + u'/' + name + u'-' + color.name(QColor::HexRgb).mid(1) + u".svg"_s;
    if (!QFile::exists(path)) {
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(tintedSvg(name, color));
            file.commit();
        }
    }
    return path;
}

} // namespace red::ui::icons
