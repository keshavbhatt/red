#include "core/theme/youtube_pref.h"

using namespace Qt::StringLiterals;

namespace red::core {

namespace {

quint64 f6Of(const QString& prefValue)
{
    for (const QString& field : prefValue.split(u'&', Qt::SkipEmptyParts)) {
        if (field.startsWith(u"f6="_s)) {
            bool ok = false;
            const quint64 value = field.mid(3).toULongLong(&ok, 16);
            return ok ? value : 0;
        }
    }
    return 0;
}

} // namespace

QString youTubePrefForTheme(const QString& prefValue, Theme theme)
{
    quint64 f6 = f6Of(prefValue) & ~(kYouTubePrefDarkBit | kYouTubePrefLightBit);
    switch (theme) {
    case Theme::Dark:
        f6 |= kYouTubePrefDarkBit;
        break;
    case Theme::Light:
        f6 |= kYouTubePrefLightBit;
        break;
    case Theme::System:
        break;
    }
    QStringList fields;
    bool replaced = false;
    for (const QString& field : prefValue.split(u'&', Qt::SkipEmptyParts)) {
        if (field.startsWith(u"f6="_s)) {
            if (f6 != 0) {
                fields << u"f6="_s + QString::number(f6, 16);
            }
            replaced = true;
        } else {
            fields << field;
        }
    }
    if (!replaced && f6 != 0) {
        fields.prepend(u"f6="_s + QString::number(f6, 16));
    }
    return fields.join(u'&');
}

Theme themeFromYouTubePref(const QString& prefValue)
{
    const quint64 f6 = f6Of(prefValue);
    if ((f6 & kYouTubePrefLightBit) != 0) {
        return Theme::Light;
    }
    if ((f6 & kYouTubePrefDarkBit) != 0) {
        return Theme::Dark;
    }
    return Theme::System;
}

} // namespace red::core
