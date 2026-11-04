// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#include "MoreAppsDialog.h"

#include "AppCard.h"
#include "AppCatalog.h"

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace ktechpit {

namespace {
constexpr int kColumns = 2;
}

MoreAppsDialog::MoreAppsDialog(const QString& place, QWidget* parent)
    : QDialog(parent)
    , m_place(place)
{
    setObjectName(u"ktechpitMoreAppsDialog"_s);
    setWindowTitle(tr("More apps from Ktechpit"));
    resize(720, 560);

    auto* intro = new QLabel(tr("Other apps by the same developer. Get opens your software centre."), this);
    intro->setWordWrap(true);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search apps"));
    m_search->setClearButtonEnabled(true);
    connect(m_search, &QLineEdit::textChanged, this, &MoreAppsDialog::rebuild);

    m_grid = new QWidget;
    m_gridLayout = new QGridLayout(m_grid);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setSpacing(8);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(m_grid);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_search);
    layout->addWidget(scroll, 1);
    layout->addWidget(buttons);

    connect(AppCatalog::instance(), &AppCatalog::changed, this, &MoreAppsDialog::rebuild);
    rebuild();
}

void MoreAppsDialog::rebuild()
{
    while (QLayoutItem* item = m_gridLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const QString needle = m_search->text().trimmed();
    int n = 0;
    const QList<AppInfo> apps = AppCatalog::instance()->apps();
    for (const AppInfo& app : apps) {
        const bool match = needle.isEmpty() || app.name.contains(needle, Qt::CaseInsensitive)
                           || app.summary.contains(needle, Qt::CaseInsensitive);
        if (!match) {
            continue;
        }
        m_gridLayout->addWidget(new AppCard(app, m_place, m_grid), n / kColumns, n % kColumns);
        ++n;
    }
    if (n == 0) {
        auto* none = new QLabel(tr("No app matches."), m_grid);
        none->setAlignment(Qt::AlignCenter);
        m_gridLayout->addWidget(none, 0, 0, 1, kColumns);
    }
    m_gridLayout->setRowStretch(m_gridLayout->rowCount(), 1);
}

} // namespace ktechpit
