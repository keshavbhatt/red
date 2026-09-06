#include "core/blocking/sponsorblock.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace red::core {

QString sponsorHashPrefix(const QString& videoId)
{
    const QByteArray digest = QCryptographicHash::hash(videoId.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex().left(4));
}

QStringList enabledSponsorCategories(const QHash<QString, SponsorAction>& actions)
{
    QStringList enabled;
    for (const QString& category : sponsorCategories()) {
        if (actions.value(category, defaultSponsorAction(category)) != SponsorAction::Off) {
            enabled << category;
        }
    }
    return enabled;
}

QUrl sponsorSegmentsUrl(const QString& videoId, const QStringList& categories)
{
    QJsonArray cats;
    for (const QString& c : categories) {
        cats.append(c);
    }
    QUrl url(u"https://sponsor.ajay.app/api/skipSegments/"_s + sponsorHashPrefix(videoId));
    QUrlQuery query;
    query.addQueryItem(u"categories"_s,
                       QString::fromUtf8(QJsonDocument(cats).toJson(QJsonDocument::Compact)));
    query.addQueryItem(u"actionTypes"_s, u"[\"skip\",\"mute\",\"full\"]"_s);
    url.setQuery(query);
    return url;
}

QList<SponsorSegment> parseSponsorSegments(const QByteArray& body, const QString& videoId)
{
    QList<SponsorSegment> result;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isArray()) {
        return result;
    }
    const QJsonArray videos = doc.array();
    for (const auto& v : videos) {
        const QJsonObject video = v.toObject();
        if (video.value(u"videoID"_s).toString() != videoId) {
            continue;
        }
        const QJsonArray segments = video.value(u"segments"_s).toArray();
        for (const auto& s : segments) {
            const QJsonObject o = s.toObject();
            const QJsonArray range = o.value(u"segment"_s).toArray();
            if (range.size() != 2) {
                continue;
            }
            SponsorSegment seg;
            seg.uuid = o.value(u"UUID"_s).toString();
            seg.category = o.value(u"category"_s).toString();
            seg.actionType = o.value(u"actionType"_s).toString(u"skip"_s);
            seg.start = range.at(0).toDouble();
            seg.end = range.at(1).toDouble();
            seg.locked = o.value(u"locked"_s).toInt() != 0;
            seg.votes = o.value(u"votes"_s).toInt();
            if (seg.end > seg.start && !seg.category.isEmpty()) {
                result << seg;
            }
        }
    }
    std::sort(result.begin(), result.end(),
              [](const SponsorSegment& a, const SponsorSegment& b) { return a.start < b.start; });
    return result;
}

QJsonArray sponsorSegmentsForPage(const QList<SponsorSegment>& segments,
                                  const QHash<QString, SponsorAction>& actions)
{
    QJsonArray out;
    for (const SponsorSegment& seg : segments) {
        const SponsorAction action = actions.value(seg.category, defaultSponsorAction(seg.category));
        if (action == SponsorAction::Off) {
            continue;
        }
        // "full" labels the whole video (e.g. entirely sponsored); it cannot be skipped.
        const bool skip = action == SponsorAction::Skip && seg.actionType != u"full"_s;
        out.append(QJsonObject{
            {u"start"_s, seg.start},
            {u"end"_s, seg.end},
            {u"category"_s, seg.category},
            {u"action"_s, skip ? u"skip"_s : u"show"_s},
            {u"uuid"_s, seg.uuid},
        });
    }
    return out;
}

QStringList sponsorRemoveCategories(const QHash<QString, SponsorAction>& actions)
{
    QStringList remove;
    for (const QString& category : sponsorCategories()) {
        if (actions.value(category, defaultSponsorAction(category)) == SponsorAction::Skip) {
            remove << category;
        }
    }
    return remove;
}

} // namespace red::core
