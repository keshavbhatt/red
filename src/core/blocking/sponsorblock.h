#pragma once

#include "core/settings/settings.h"

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QUrl>

// Pure SponsorBlock helpers (FEATURES B6, ADR-007): request building and
// response parsing. The HTTP client lives in services/.
namespace red::core {

struct SponsorSegment
{
    QString uuid;
    QString category;   ///< sponsor, selfpromo, interaction, intro, outro, preview, music_offtopic, filler
    QString actionType; ///< skip, mute, full, poi, chapter
    double start = 0;
    double end = 0;
    bool locked = false;
    int votes = 0;
};

/// First 4 hex chars of sha256(videoId) — the k-anonymity prefix the API
/// accepts so the server never learns the exact video.
[[nodiscard]] QString sponsorHashPrefix(const QString& videoId);

/// Categories the user has not switched Off, as the API's JSON-array query value.
[[nodiscard]] QStringList enabledSponsorCategories(const QHash<QString, SponsorAction>& actions);

/// https://sponsor.ajay.app/api/skipSegments/<prefix>?categories=[…]&actionTypes=[…]
[[nodiscard]] QUrl sponsorSegmentsUrl(const QString& videoId, const QStringList& categories);

/// Parses the hash-prefix response (an array of {videoID, segments:[…]}) and
/// keeps only the entries for `videoId`. Returns an empty list on any shape error.
[[nodiscard]] QList<SponsorSegment> parseSponsorSegments(const QByteArray& body, const QString& videoId);

/// What the page script needs: [{start, end, category, action: "skip"|"show", uuid}].
[[nodiscard]] QJsonArray sponsorSegmentsForPage(const QList<SponsorSegment>& segments,
                                                const QHash<QString, SponsorAction>& actions);

/// Segments to cut when downloading (--sponsorblock-remove): the Skip categories.
[[nodiscard]] QStringList sponsorRemoveCategories(const QHash<QString, SponsorAction>& actions);

} // namespace red::core
