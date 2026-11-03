// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#include "ui/account_dialog.h"

#include "core/theme/theme_service.h"
#include "platform/file_manager.h"
#include "services/licensing/license_service.h"
#include "ui/icons.h"
#include "ui/plans_dialog.h"
#include "ui/red_style.h"

#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace red::ui {

AccountDialog::AccountDialog(services::LicenseService& license, const core::ThemeService& theme,
                             QWidget* parent)
    : QDialog(parent)
    , m_license(license)
    , m_theme(theme)
{
    setWindowTitle(tr("Account & licence"));
    setModal(false);
    setMinimumWidth(480);
    const Tokens t = Tokens::forScheme(m_theme.isDark());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* icon = new QLabel(this);
    icon->setFixedSize(44, 44);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(u"background:%1;border-radius:22px;"_s.arg(t.brand.name()));
    icon->setPixmap(icons::pixmap(u"account"_s, t.brandText, 24, devicePixelRatioF()));
    top->addWidget(icon, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* heading = new QLabel(tr("Your Red account"), this);
    heading->setProperty("redTitle", true);
    text->addWidget(heading);
    m_plan = new QLabel(this);
    text->addWidget(m_plan);
    m_hint = new QLabel(this);
    m_hint->setProperty("redMuted", true);
    m_hint->setWordWrap(true);
    text->addWidget(m_hint);
    top->addLayout(text, 1);
    root->addLayout(top);

    auto* card = new QFrame(this);
    card->setProperty("redCard", true);
    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 12, 12);
    cardLayout->setSpacing(10);
    auto* idLabel = new QLabel(tr("Account ID"), card);
    idLabel->setProperty("redMuted", true);
    cardLayout->addWidget(idLabel);
    m_id = new QLabel(card);
    QFont mono = m_id->font();
    mono.setFamilies({u"JetBrains Mono"_s, u"DejaVu Sans Mono"_s, u"monospace"_s});
    m_id->setFont(mono);
    m_id->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(m_id, 1);
    m_copy = new QPushButton(tr("Copy"), card);
    m_copy->setProperty("redFlat", true);
    connect(m_copy, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_license.accountId());
        m_copy->setText(tr("Copied"));
        QTimer::singleShot(1500, m_copy, [this] { m_copy->setText(tr("Copy")); });
    });
    cardLayout->addWidget(m_copy);
    root->addWidget(card);

    auto* note = new QLabel(
        tr("Your licence is tied to this account ID. Keep it: it is what you enter on another computer, "
           "and what support needs."),
        this);
    note->setProperty("redMuted", true);
    note->setWordWrap(true);
    root->addWidget(note);

    auto* buttons = new QHBoxLayout;
    m_refresh = new QPushButton(tr("Refresh"), this);
    m_refresh->setProperty("redFlat", true);
    m_refresh->setToolTip(tr("Check the licence with the server again"));
    connect(m_refresh, &QPushButton::clicked, this, [this] { m_license.refresh(); });
    buttons->addWidget(m_refresh);
    auto* compare = new QPushButton(tr("Compare plans"), this);
    compare->setProperty("redFlat", true);
    compare->setToolTip(tr("What Free, Evaluation and Red Pro include"));
    connect(compare, &QPushButton::clicked, this, &AccountDialog::showPlans);
    buttons->addWidget(compare);
    buttons->addStretch(1);
    m_portal = new QPushButton(tr("Manage subscription"), this);
    connect(m_portal, &QPushButton::clicked, this, [this] { platform::openUrl(m_license.portalUrl()); });
    buttons->addWidget(m_portal);
    m_purchase = new QPushButton(tr("Get Red Pro"), this);
    m_purchase->setProperty("redPrimary", true);
    connect(m_purchase, &QPushButton::clicked, this, [this] { platform::openUrl(m_license.checkoutUrl()); });
    buttons->addWidget(m_purchase);
    auto* close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::close);
    buttons->addWidget(close);
    root->addLayout(buttons);

    connect(&m_license, &services::LicenseService::statusChanged, this, &AccountDialog::refresh);
    connect(&m_license, &services::LicenseService::checkingChanged, this, [this](bool) { refresh(); });
    refresh();
}

void AccountDialog::refresh()
{
    using Tier = services::LicenseService::Tier;
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    m_id->setText(m_license.accountId().isEmpty() ? tr("…") : m_license.accountId());
    m_copy->setEnabled(!m_license.accountId().isEmpty());
    m_refresh->setEnabled(!m_license.checking());

    QString plan;
    QColor color = t.muted;
    QString hint;
    if (m_license.checking() && m_license.accountId().isEmpty()) {
        plan = tr("Checking your licence…");
    } else {
        switch (m_license.tier()) {
        case Tier::Pro:
            plan = tr("Red Pro · active");
            color = t.success;
            hint = m_license.status().expiryDate.isEmpty()
                       ? tr("Thank you for supporting Red. Every feature is unlocked on this computer.")
                       : tr("Every feature is unlocked. Your plan runs until %1.")
                             .arg(m_license.status().expiryDate);
            break;
        case Tier::Evaluation: {
            const int days = std::max(0, m_license.evaluationDaysRemaining());
            plan = days == 1 ? tr("Evaluation · 1 day left") : tr("Evaluation · %1 days left").arg(days);
        }
            color = t.warning;
            hint = tr("The %1-day evaluation includes every Pro feature. Get Red Pro to keep them after it "
                      "ends.")
                       .arg(m_license.evaluationDays());
            break;
        case Tier::Free:
            plan = tr("Free");
            color = t.muted;
            {
                const int left = std::max(0, m_license.downloadsRemainingToday());
                const QString allowance =
                    left == 1 ? tr("Red downloads up to %1 videos a day for free; 1 left today.")
                                    .arg(services::LicenseService::kFreeDownloadsPerDay)
                              : tr("Red downloads up to %1 videos a day for free; %2 left today.")
                                    .arg(services::LicenseService::kFreeDownloadsPerDay)
                                    .arg(left);
                hint = allowance + u' ' +
                       (m_license.evaluationEnded()
                            ? tr("Your evaluation has ended. Get Red Pro for unlimited downloads.")
                            : tr("Get Red Pro for unlimited downloads and to support development."));
            }
            break;
        }
        if (m_license.checking()) {
            plan += tr("  ·  checking…");
        }
    }
    m_plan->setText(plan);
    m_plan->setStyleSheet(u"color:%1;font-weight:600;"_s.arg(color.name()));
    m_hint->setText(hint);
    m_purchase->setVisible(m_license.tier() != Tier::Pro);
    m_portal->setVisible(m_license.tier() == Tier::Pro);
}

void AccountDialog::showPlans()
{
    auto* dialog = new PlansDialog(m_license, m_theme, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void AccountDialog::highlightPurchase()
{
    if (m_purchase->isVisible()) {
        m_purchase->setFocus();
        m_purchase->setDefault(true);
    }
}

} // namespace red::ui
