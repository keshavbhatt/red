#include "ui/welcome_dialog.h"

#include "ui/icons.h"
#include "ui/red_style.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

WelcomeDialog::WelcomeDialog(QWidget* parent)
    : QDialog(parent)
{
    const bool dark = parent != nullptr && parent->palette().window().color().lightness() < 128;
    const Tokens t = Tokens::forScheme(dark);
    setModal(true);
    setWindowTitle(tr("Welcome to Red"));
    setMinimumWidth(560);
    setMaximumWidth(640);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(16);

    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* logo = new QLabel(this);
    logo->setFixedSize(44, 44);
    logo->setPixmap(icons::brand().pixmap(44, 44));
    top->addWidget(logo, 0, Qt::AlignTop);
    auto* heading = new QVBoxLayout;
    heading->setSpacing(4);
    auto* title = new QLabel(tr("Welcome to Red"), this);
    title->setProperty("redTitle", true);
    heading->addWidget(title);
    auto* subtitle = new QLabel(tr("YouTube on your desktop, with downloads and without the ads."), this);
    subtitle->setProperty("redMuted", true);
    subtitle->setWordWrap(true);
    heading->addWidget(subtitle);
    top->addLayout(heading, 1);
    root->addLayout(top);

    auto* card = new QFrame(this);
    card->setProperty("redCard", true);
    auto* rows = new QVBoxLayout(card);
    rows->setContentsMargins(16, 14, 16, 14);
    rows->setSpacing(14);
    const auto addRow = [&](const QString& icon, const QString& head, const QString& body) {
        auto* row = new QHBoxLayout;
        row->setSpacing(14);
        auto* tile = new QLabel(card);
        tile->setFixedSize(36, 36);
        tile->setAlignment(Qt::AlignCenter);
        tile->setStyleSheet(u"background:%1;border-radius:10px;"_s.arg(t.hover.name()));
        tile->setPixmap(icons::pixmap(icon, t.text, 20, devicePixelRatioF()));
        row->addWidget(tile, 0, Qt::AlignTop);
        auto* text = new QVBoxLayout;
        text->setSpacing(2);
        auto* h = new QLabel(head, card);
        h->setStyleSheet(u"font-weight:600;"_s);
        text->addWidget(h);
        auto* b = new QLabel(body, card);
        b->setProperty("redMuted", true);
        b->setWordWrap(true);
        text->addWidget(b);
        row->addLayout(text, 1);
        rows->addLayout(row);
    };
    addRow(u"youtube"_s, tr("Three ways to watch"),
           tr("YouTube, YouTube Music and TV mode sit at the top of the side rail. Ctrl+1, Ctrl+2 and Ctrl+3 "
              "switch between them."));
    addRow(u"download"_s, tr("Download videos and music"),
           tr("On a video, playlist or channel, press Download this on the rail (Ctrl+D), or right-click a "
              "thumbnail. Downloads (Ctrl+J) shows what is in progress."));
    addRow(u"miniplayer"_s, tr("Keep watching while you work"),
           tr("The mini player shrinks the video into a small window that stays on top (Ctrl+Shift+M). Esc "
              "brings the full window back."));
    addRow(u"shield"_s, tr("Fewer interruptions"),
           tr("Ads, trackers and sponsor segments are skipped. Settings, Blocking decides what goes."));
    root->addWidget(card);

    auto* buttons = new QHBoxLayout;
    auto* hint = new QLabel(tr("Find this again under More, Show me around."), this);
    hint->setProperty("redMuted", true);
    buttons->addWidget(hint, 1);
    auto* keys = new QPushButton(tr("Keyboard shortcuts"), this);
    connect(keys, &QPushButton::clicked, this, &WelcomeDialog::shortcutsRequested);
    buttons->addWidget(keys);
    auto* start = new QPushButton(tr("Get started"), this);
    start->setProperty("redPrimary", true);
    start->setDefault(true);
    connect(start, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(start);
    root->addLayout(buttons);
}

} // namespace red::ui
