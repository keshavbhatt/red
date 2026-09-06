#pragma once

#include <QColor>
#include <QString>

namespace red::ui {

/// Red's brand tokens (DESIGN.md §1), per scheme. The single source of truth
/// for every colour the native chrome paints — the style sheet, the rail, the
/// download cards and the dialogs all read from here.
struct Tokens
{
    QColor accent;
    QColor accentHover;
    QColor accentText;
    QColor bg;
    QColor rail;
    QColor panel;
    QColor elevated;
    QColor hover;
    QColor input;
    QColor border;
    QColor text;
    QColor muted;
    QColor link;
    QColor success;
    QColor warning;
    QColor danger;

    [[nodiscard]] static Tokens forScheme(bool dark);
};

/// The application-wide Qt style sheet built from the tokens (applied by
/// ThemeApplier on every scheme change). Controls carry YouTube's rounded
/// language; primary buttons opt in with the `redPrimary` dynamic property,
/// flat icon buttons with `redFlat`, and cards with `redCard`.
[[nodiscard]] QString redStyleSheet(bool dark);

} // namespace red::ui
