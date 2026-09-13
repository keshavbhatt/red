#include "ui/whats_new_dialog.h"

#include "core/changelog.h"
#include "platform/file_manager.h"
#include "ui/icons.h"
#include "ui/red_style.h"

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
const QString kChangelogUrl = u"https://github.com/keshavbhatt/red/blob/master/CHANGELOG.md"_s;
}

QString WhatsNewDialog::bundledNotes(const QString& version)
{
    QFile file(u":/text/CHANGELOG.md"_s);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return core::changelogSection(QString::fromUtf8(file.readAll()), version);
}

WhatsNewDialog::WhatsNewDialog(const QString& version, const QString& notesMarkdown, QWidget* parent)
    : QDialog(parent)
{
    const bool dark = parent != nullptr && parent->palette().window().color().lightness() < 128;
    const Tokens t = Tokens::forScheme(dark);
    setModal(true);
    setWindowTitle(tr("What's new in Red"));
    setMinimumWidth(560);
    setMaximumWidth(680);
    resize(600, 560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* badge = new QLabel(this);
    badge->setFixedSize(44, 44);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(u"background:%1;border-radius:22px;"_s.arg(t.accent.name()));
    badge->setPixmap(icons::pixmap(u"bolt"_s, t.accentText, 24, devicePixelRatioF()));
    top->addWidget(badge, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* title = new QLabel(tr("What's new in Red %1").arg(version), this);
    title->setProperty("redTitle", true);
    text->addWidget(title);
    auto* subtitle = new QLabel(tr("Highlights of this release."), this);
    subtitle->setProperty("redMuted", true);
    text->addWidget(subtitle);
    top->addLayout(text, 1);
    root->addLayout(top);

    auto* card = new QFrame(this);
    card->setProperty("redCard", true);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 8, 12, 8);
    auto* notes = new QTextBrowser(card);
    notes->setProperty("redNotes", true);
    notes->setReadOnly(true);
    notes->setFrameShape(QFrame::NoFrame);
    notes->viewport()->setAutoFillBackground(false);
    notes->setOpenLinks(false);
    notes->setOpenExternalLinks(false);
    notes->setFocusPolicy(Qt::NoFocus); // Enter goes to the default button, not a link
    notes->setMinimumHeight(220);
    connect(notes, &QTextBrowser::anchorClicked, this,
            [](const QUrl& url) { platform::openUrl(url.toString()); });
    notes->setMarkdown(notesMarkdown);
    // Markdown headings ignore the document style sheet; keep "### Added"
    // a shade above body size instead of banner-sized.
    for (QTextBlock block = notes->document()->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0) {
            QTextCursor cursor(block);
            cursor.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat format;
            format.setFontPointSize(notes->font().pointSizeF() + 1);
            format.setFontWeight(QFont::DemiBold);
            cursor.mergeCharFormat(format);
        }
    }
    cardLayout->addWidget(notes, 1);
    root->addWidget(card, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* full = new QPushButton(tr("Full changelog"), this);
    connect(full, &QPushButton::clicked, this, [] { platform::openUrl(kChangelogUrl); });
    buttons->addWidget(full);
    auto* ok = new QPushButton(tr("Got it"), this);
    ok->setProperty("redPrimary", true);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(ok);
    root->addLayout(buttons);
}

} // namespace red::ui
