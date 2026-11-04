// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: MIT
#include "AppCard.h"

#include "AppLinks.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace ktechpit {

namespace {
constexpr int kIconSize = 40;
}

AppCard::AppCard(const AppInfo& app, const QString& place, QWidget* parent)
    : QFrame(parent)
    , m_app(app)
    , m_place(place)
{
    setObjectName(u"ktechpitAppCard"_s);
    setFrameShape(QFrame::StyledPanel);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("%1\nClick for details").arg(app.title));
    setAttribute(Qt::WA_Hover);

    m_icon = new QLabel(this);
    m_icon->setFixedSize(kIconSize, kIconSize);
    m_icon->setAlignment(Qt::AlignCenter);
    updateIcon();

    auto* name = new QLabel(app.name, this);
    QFont bold = name->font();
    bold.setBold(true);
    name->setFont(bold);

    auto* summary = new QLabel(app.summary, this);
    summary->setWordWrap(true);
    summary->setForegroundRole(QPalette::PlaceholderText);
    // Two lines at most: the full text is on the product page.
    const int lineHeight = summary->fontMetrics().lineSpacing();
    summary->setMaximumHeight(lineHeight * 2 + 2);
    summary->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    m_get = new QPushButton(tr("Get"), this);
    m_get->setCursor(Qt::PointingHandCursor);
    m_get->setToolTip(tr("Install %1").arg(app.name));
    connect(m_get, &QPushButton::clicked, this, [this] { AppLinks::openGet(m_app, m_place); });

    auto* text = new QVBoxLayout;
    text->setSpacing(2);
    text->addWidget(name);
    text->addWidget(summary);
    text->addStretch();

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 10, 10, 10);
    row->setSpacing(10);
    row->addWidget(m_icon, 0, Qt::AlignTop);
    row->addLayout(text, 1);
    row->addWidget(m_get, 0, Qt::AlignVCenter);

    connect(AppCatalog::instance(), &AppCatalog::iconReady, this, [this](const QString& id) {
        if (id == m_app.id) {
            updateIcon();
        }
    });
}

void AppCard::updateIcon()
{
    m_icon->setPixmap(AppCatalog::instance()->icon(m_app, kIconSize));
}

void AppCard::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        AppLinks::openPage(m_app, m_place);
    }
    QFrame::mouseReleaseEvent(event);
}

void AppCard::enterEvent(QEnterEvent* event)
{
    setBackgroundRole(QPalette::AlternateBase);
    setAutoFillBackground(true);
    QFrame::enterEvent(event);
}

void AppCard::leaveEvent(QEvent* event)
{
    setAutoFillBackground(false);
    QFrame::leaveEvent(event);
}

} // namespace ktechpit
