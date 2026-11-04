// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#pragma once

#include <QDialog>

class QGridLayout;
class QLineEdit;

namespace ktechpit {

/// Every Ktechpit app that runs here, best fit first, with a search field.
class MoreAppsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MoreAppsDialog(const QString& place, QWidget* parent = nullptr);

private:
    void rebuild();

    QString m_place;
    QLineEdit* m_search = nullptr;
    QWidget* m_grid = nullptr;
    QGridLayout* m_gridLayout = nullptr;
};

} // namespace ktechpit
