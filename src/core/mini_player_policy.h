#pragma once

#include "core/settings/settings.h"

#include <QSize>
#include <QUrl>

// Pure mini player rules (FEATURES D6): which window shape the content wants
// and how big that window is for each size setting.
namespace red::core {

enum class MiniPlayerShape
{
    Wide,   ///< a regular video, 16:9
    Shorts, ///< a YouTube Short, 9:16
    Music,  ///< YouTube Music's own responsive player page, tall
};

[[nodiscard]] MiniPlayerShape miniPlayerShape(AppMode mode, const QUrl& url);
[[nodiscard]] QSize miniPlayerSize(MiniPlayerSize size, MiniPlayerShape shape);

} // namespace red::core
