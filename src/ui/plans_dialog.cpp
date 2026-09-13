#include "ui/plans_dialog.h"

#include "core/theme/theme_service.h"
#include "platform/file_manager.h"
#include "services/licensing/license_service.h"
#include "ui/icons.h"
#include "ui/red_style.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {

using Tier = services::LicenseService::Tier;

/// One comparison row: a feature and what each plan gets. Empty cell text
/// means "included" (a check) or "not included" (a dash) per the bool.
struct Row
{
    QString feature;
    bool free;
    bool evaluation;
    bool pro;
    QString freeText;
    QString paidText;
};

} // namespace

PlansDialog::PlansDialog(const services::LicenseService& license, const core::ThemeService& theme,
                         QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Plans"));
    setModal(true);
    setMinimumWidth(600);
    const Tokens t = Tokens::forScheme(theme.isDark());
    const Tier current = license.tier();

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* icon = new QLabel(this);
    icon->setFixedSize(44, 44);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(u"background:%1;border-radius:22px;"_s.arg(t.accent.name()));
    icon->setPixmap(icons::pixmap(u"bolt"_s, t.accentText, 24, devicePixelRatioF()));
    top->addWidget(icon, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* title = new QLabel(tr("What each plan includes"), this);
    title->setProperty("redTitle", true);
    text->addWidget(title);
    auto* subtitle = new QLabel(
        tr("Nearly all of Red is free. Red Pro lifts the daily download limit and keeps the work going."),
        this);
    subtitle->setProperty("redMuted", true);
    subtitle->setWordWrap(true);
    text->addWidget(subtitle);
    top->addLayout(text, 1);
    root->addLayout(top);

    auto* card = new QFrame(this);
    card->setProperty("redCard", true);
    auto* grid = new QGridLayout(card);
    grid->setContentsMargins(18, 14, 18, 14);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(0);

    // Column headers: the plan name, with "your plan" under the current one.
    const struct
    {
        Tier tier;
        QString name;
    } plans[] = {{Tier::Free, tr("Free")}, {Tier::Evaluation, tr("Evaluation")}, {Tier::Pro, tr("Red Pro")}};
    for (int c = 0; c < 3; ++c) {
        auto* head = new QWidget(card);
        auto* col = new QVBoxLayout(head);
        col->setContentsMargins(0, 0, 0, 0);
        col->setSpacing(2);
        auto* name = new QLabel(plans[c].name, head);
        name->setAlignment(Qt::AlignCenter);
        name->setStyleSheet(
            u"font-weight:600;color:%1;"_s.arg(plans[c].tier == Tier::Pro ? t.accent.name() : t.text.name()));
        col->addWidget(name);
        auto* mark = new QLabel(plans[c].tier == current ? tr("your plan") : QString(), head);
        mark->setAlignment(Qt::AlignCenter);
        mark->setProperty("redMuted", true);
        mark->setStyleSheet(u"font-size:11px;"_s);
        col->addWidget(mark);
        grid->addWidget(head, 0, c + 1, Qt::AlignCenter | Qt::AlignBottom);
    }
    grid->setRowMinimumHeight(0, 44);

    const int limit = services::LicenseService::kFreeDownloadsPerDay;
    const QList<Row> rows = {
        {tr("Desktop and TV modes"), true, true, true, {}, {}},
        {tr("Ad, tracker and Shorts blocking, SponsorBlock, dislike counts"), true, true, true, {}, {}},
        {tr("Media keys, tray controls, mini player, screen kept awake"), true, true, true, {}, {}},
        {tr("Video, audio-only and exact-stream downloads with subtitles, thumbnail and chapters"),
         true,
         true,
         true,
         {},
         {}},
        {tr("Playlist and channel downloads"), true, true, true, {}, {}},
        {tr("Sign-in reused for age-restricted videos, sponsor segments cut out of files"),
         true,
         true,
         true,
         {},
         {}},
        {tr("Download engine kept up to date automatically"), true, true, true, {}, {}},
        {tr("Downloads per day"), true, true, true, tr("%1").arg(limit), tr("Unlimited")},
        {tr("Keeps Red maintained when YouTube changes"), false, false, true, {}, {}},
    };
    const QPixmap check = icons::pixmap(u"check"_s, t.success, 18, devicePixelRatioF());
    auto cell = [&](bool included, const QString& label) {
        auto* w = new QLabel(card);
        w->setAlignment(Qt::AlignCenter);
        if (!label.isEmpty()) {
            w->setText(label);
            w->setStyleSheet(u"color:%1;font-weight:600;"_s.arg(t.text.name()));
        } else if (included) {
            w->setPixmap(check);
        } else {
            w->setText(u"—"_s);
            w->setProperty("redMuted", true);
        }
        return w;
    };
    int r = 1;
    for (const Row& row : rows) {
        auto* sep = new QFrame(card);
        sep->setProperty("redSeparator", true);
        grid->addWidget(sep, r++, 0, 1, 4);
        auto* feature = new QLabel(row.feature, card);
        feature->setWordWrap(true);
        grid->addWidget(feature, r, 0);
        grid->addWidget(cell(row.free, row.freeText), r, 1);
        grid->addWidget(cell(row.evaluation, row.paidText), r, 2);
        grid->addWidget(cell(row.pro, row.paidText), r, 3);
        grid->setRowMinimumHeight(r, 40);
        ++r;
    }
    grid->setColumnStretch(0, 5);
    for (int c = 1; c <= 3; ++c) {
        grid->setColumnMinimumWidth(c, 88);
    }
    root->addWidget(card);

    auto* note = new QLabel(
        tr("Red is built and kept working by one developer, and Red Pro is what pays for that. The "
           "evaluation runs for %1 days from the first start; Free never expires.")
            .arg(license.evaluationDays()),
        this);
    note->setProperty("redMuted", true);
    note->setWordWrap(true);
    root->addWidget(note);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    if (current != Tier::Pro) {
        auto* purchase = new QPushButton(tr("Get Red Pro"), this);
        purchase->setProperty("redPrimary", true);
        connect(purchase, &QPushButton::clicked, this,
                [url = license.checkoutUrl()] { platform::openUrl(url); });
        buttons->addWidget(purchase);
    }
    auto* close = new QPushButton(tr("Close"), this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    root->addLayout(buttons);
}

} // namespace red::ui
