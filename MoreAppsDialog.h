// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#pragma once

#include <QDialog>

class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;

namespace ktechpit {

/// Every Ktechpit app that runs here (installed ones marked), in the site's order, with a search
/// field and a Refresh button that fetches the list now.
class MoreAppsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MoreAppsDialog(const QString& place, QWidget* parent = nullptr);

private:
    void rebuild();

    QString m_place;
    QLineEdit* m_search = nullptr;
    QPushButton* m_refresh = nullptr;
    QLabel* m_status = nullptr;
    QWidget* m_grid = nullptr;
    QGridLayout* m_gridLayout = nullptr;
};

} // namespace ktechpit
