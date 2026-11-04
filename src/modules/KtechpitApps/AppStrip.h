// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#pragma once

#include <QWidget>

class QBoxLayout;
class QLabel;

namespace ktechpit {

/// "More apps from Ktechpit": a heading, `count` cards that fit the host app, and a
/// "See all" link to MoreAppsDialog. Drop it into an About dialog, a What's new sheet or a
/// welcome page. Hides itself when the catalog is switched off or has nothing to show.
class AppStrip : public QWidget
{
    Q_OBJECT

public:
    enum class Orientation { Horizontal, Vertical };

    /// `place` names where the strip is shown ("about"); it ends up in the ?ref= of links.
    explicit AppStrip(const QString& place, int count = 3,
                      Orientation orientation = Orientation::Vertical, QWidget* parent = nullptr);

    /// Replaces the heading ("More apps from Ktechpit"); empty hides it.
    void setTitle(const QString& title);

private:
    void rebuild();

    QString m_place;
    int m_count;
    QLabel* m_title = nullptr;
    QWidget* m_cards = nullptr;
    QBoxLayout* m_cardsLayout = nullptr;
};

} // namespace ktechpit
