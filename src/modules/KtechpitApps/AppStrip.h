// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#pragma once

#include "AppCatalog.h"

#include <QWidget>

class QBoxLayout;
class QLabel;

namespace ktechpit {

/// "More apps from Ktechpit": a heading, `count` cards and a "See all" link to MoreAppsDialog.
/// By default the first card is the app that fits the host best and the others change each
/// time the strip is built, so users see most of the catalog without opening the full list.
/// Drop it into an About dialog, a What's new sheet or a welcome page. Hides itself when the
/// catalog is switched off or has nothing to show.
class AppStrip : public QWidget
{
    Q_OBJECT

public:
    enum class Orientation { Horizontal, Vertical };

    /// `place` names where the strip is shown ("about"); it ends up in the ?ref= of links.
    explicit AppStrip(const QString& place, int count = 3,
                      Orientation orientation = Orientation::Vertical, QWidget* parent = nullptr);

    /// How the cards are chosen (Pick::Mixed by default); rebuilds the strip.
    void setPick(Pick pick);

    /// Replaces the heading ("More apps from Ktechpit"); empty hides it.
    void setTitle(const QString& title);

private:
    /// `repick`: choose apps afresh; otherwise keep the ones shown (a catalog refresh must not
    /// swap cards under the user's eyes) and only redraw them from the new data.
    void rebuild(bool repick);

    QString m_place;
    int m_count;
    Pick m_pick = Pick::Mixed;
    QStringList m_shown;
    QLabel* m_title = nullptr;
    QWidget* m_cards = nullptr;
    QBoxLayout* m_cardsLayout = nullptr;
};

} // namespace ktechpit
