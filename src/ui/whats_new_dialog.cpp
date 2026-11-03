#include "ui/whats_new_dialog.h"

#include "core/changelog.h"
#include "platform/file_manager.h"
#include "ui/icons.h"
#include "ui/links.h"
#include "ui/red_style.h"

#include <QComboBox>
#include <QDate>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace red::ui {

QString WhatsNewDialog::bundledChangelog()
{
    QFile file(u":/text/CHANGELOG.md"_s);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString WhatsNewDialog::bundledNotes(const QString& version)
{
    return core::changelogSection(bundledChangelog(), version);
}

WhatsNewDialog::WhatsNewDialog(const QString& runningVersion, QWidget* parent)
    : QDialog(parent)
    , m_running(runningVersion)
    , m_markdown(bundledChangelog())
    , m_releases(core::changelogReleases(m_markdown))
{
    const bool dark = parent != nullptr && parent->palette().window().color().lightness() < 128;
    const Tokens t = Tokens::forScheme(dark);
    setModal(true);
    setWindowTitle(tr("What's new in Red"));
    setMinimumWidth(560);
    setMaximumWidth(680);
    resize(600, 600);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* badge = new QLabel(this);
    badge->setFixedSize(44, 44);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(u"background:%1;border-radius:22px;"_s.arg(t.brand.name()));
    badge->setPixmap(icons::pixmap(u"bolt"_s, t.brandText, 24, devicePixelRatioF()));
    top->addWidget(badge, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    m_title = new QLabel(this);
    m_title->setProperty("redTitle", true);
    text->addWidget(m_title);
    m_subtitle = new QLabel(this);
    m_subtitle->setProperty("redMuted", true);
    text->addWidget(m_subtitle);
    top->addLayout(text, 1);
    root->addLayout(top);

    // Every release in the changelog, newest first, with its date.
    auto* pickerRow = new QHBoxLayout;
    pickerRow->setSpacing(10);
    auto* pickerLabel = new QLabel(tr("Version"), this);
    pickerLabel->setProperty("redMuted", true);
    pickerRow->addWidget(pickerLabel);
    m_picker = new QComboBox(this);
    m_picker->setAccessibleName(tr("Version"));
    for (qsizetype i = 0; i < m_releases.size(); ++i) {
        const core::ChangelogRelease& release = m_releases.at(i);
        QString label = release.version;
        const QDate date = QDate::fromString(release.date, Qt::ISODate);
        if (date.isValid()) {
            label += u" · "_s + QLocale().toString(date, u"d MMM yyyy"_s);
        }
        if (i == 0) {
            label = tr("%1 (latest)").arg(label);
        }
        m_picker->addItem(label, release.version);
    }
    pickerLabel->setBuddy(m_picker);
    pickerRow->addWidget(m_picker, 1);
    root->addLayout(pickerRow);

    auto* card = new QFrame(this);
    card->setProperty("redCard", true);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 8, 12, 8);
    m_notes = new QTextBrowser(card);
    m_notes->setProperty("redNotes", true);
    m_notes->setReadOnly(true);
    m_notes->setFrameShape(QFrame::NoFrame);
    m_notes->viewport()->setAutoFillBackground(false);
    m_notes->setOpenLinks(false);
    m_notes->setOpenExternalLinks(false);
    m_notes->setFocusPolicy(Qt::NoFocus); // Enter goes to the default button, not a link
    m_notes->setMinimumHeight(220);
    connect(m_notes, &QTextBrowser::anchorClicked, this,
            [](const QUrl& url) { platform::openUrl(url.toString()); });
    cardLayout->addWidget(m_notes, 1);
    root->addWidget(card, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* guide = new QPushButton(tr("Guide"), this);
    connect(guide, &QPushButton::clicked, this, [] { platform::openUrl(links::kGuide); });
    buttons->addWidget(guide);
    auto* full = new QPushButton(tr("Full changelog"), this);
    connect(full, &QPushButton::clicked, this, [] { platform::openUrl(links::kChangelog); });
    buttons->addWidget(full);
    auto* ok = new QPushButton(tr("Got it"), this);
    ok->setProperty("redPrimary", true);
    ok->setDefault(true);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(ok);
    root->addLayout(buttons);

    connect(m_picker, &QComboBox::currentIndexChanged, this,
            [this](int index) { showVersion(m_picker->itemData(index).toString()); });
    // Open on the running version, or the newest release when it has no section yet.
    const int running = m_picker->findData(m_running);
    if (m_picker->count() > 0) {
        m_picker->setCurrentIndex(qMax(0, running));
        showVersion(m_picker->currentData().toString());
    }
}

void WhatsNewDialog::showVersion(const QString& version)
{
    if (const int index = m_picker->findData(version); index >= 0 && index != m_picker->currentIndex()) {
        m_picker->setCurrentIndex(index); // re-enters through currentIndexChanged
        return;
    }
    m_title->setText(tr("What's new in Red %1").arg(version));
    const auto release =
        std::find_if(m_releases.cbegin(), m_releases.cend(),
                     [&version](const core::ChangelogRelease& r) { return r.version == version; });
    const QDate date = release != m_releases.cend() ? QDate::fromString(release->date, Qt::ISODate) : QDate();
    m_subtitle->setText(version == m_running ? tr("Highlights of this release.")
                        : date.isValid()
                            ? tr("Released %1.").arg(QLocale().toString(date, u"d MMMM yyyy"_s))
                            : tr("Not released yet."));
    m_notes->setMarkdown(core::changelogSection(m_markdown, version));
    // Markdown headings ignore the document style sheet; keep "### Added"
    // a shade above body size instead of banner-sized.
    for (QTextBlock block = m_notes->document()->begin(); block.isValid(); block = block.next()) {
        if (block.blockFormat().headingLevel() > 0) {
            QTextCursor cursor(block);
            cursor.select(QTextCursor::BlockUnderCursor);
            QTextCharFormat format;
            format.setFontPointSize(m_notes->font().pointSizeF() + 1);
            format.setFontWeight(QFont::DemiBold);
            cursor.mergeCharFormat(format);
        }
    }
    m_notes->verticalScrollBar()->setValue(0);
}

QString WhatsNewDialog::shownVersion() const
{
    return m_picker->count() > 0 ? m_picker->currentData().toString() : m_running;
}

} // namespace red::ui
