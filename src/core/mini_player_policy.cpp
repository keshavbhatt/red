#include "core/mini_player_policy.h"

using namespace Qt::StringLiterals;

namespace red::core {

MiniPlayerShape miniPlayerShape(AppMode mode, const QUrl& url)
{
    if (mode == AppMode::Music) {
        return MiniPlayerShape::Music;
    }
    return url.path().startsWith(u"/shorts/"_s) ? MiniPlayerShape::Shorts : MiniPlayerShape::Wide;
}

QSize miniPlayerSize(MiniPlayerSize size, MiniPlayerShape shape)
{
    // Rows: Small, Medium, Large. Shorts keep the video's 9:16; YouTube Music
    // needs room under the art for the title, seek bar and controls.
    const int row = size == MiniPlayerSize::Small ? 0 : size == MiniPlayerSize::Large ? 2 : 1;
    static constexpr QSize kWide[]{{400, 225}, {480, 270}, {640, 360}};
    static constexpr QSize kShorts[]{{225, 400}, {270, 480}, {338, 600}};
    static constexpr QSize kMusic[]{{320, 568}, {360, 640}, {405, 720}};
    switch (shape) {
    case MiniPlayerShape::Shorts:
        return kShorts[row];
    case MiniPlayerShape::Music:
        return kMusic[row];
    case MiniPlayerShape::Wide:
        break;
    }
    return kWide[row];
}

} // namespace red::core
