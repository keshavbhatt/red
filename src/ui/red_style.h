#pragma once

#include <QColor>
#include <QString>

namespace red::ui {

/// Red's brand tokens (DESIGN.md §1), per scheme. The single source of truth
/// for every colour the native chrome paints — the style sheet, the rail, the
/// download cards and the dialogs all read from here.
struct Tokens
{
    // YouTube's colour roles: red only for small brand marks, the inverted
    // pill for primary actions, blue for controls and selection (ADR-017).
    QColor brand;     ///< YouTube red: progress, badges, the active mode glyph
    QColor brandText; ///< glyphs on a brand badge
    QColor primary;   ///< primary buttons and the rail's Download pill
    QColor primaryHover;
    QColor primaryText;
    QColor control;     ///< Red's red: ticked check boxes, radios, selection, the picked card
    QColor controlText; ///< the tick and selected text on `control`
    QColor hoverBorder; ///< an input's border under the mouse
    QColor focusBorder; ///< an input's border with keyboard focus: visible, not glaring
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
