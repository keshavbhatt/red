#include "ui/about_dialog.h"

#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "platform/file_manager.h"
#include "ui/diagnostics.h"
#include "ui/icons.h"
#include "ui/links.h"
#include "ui/red_style.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
const QString kDonateUrl = u"https://www.paypal.com/paypalme/keshavnrj/11"_s;
const QString kWebsiteUrl = u"https://ktechpit.com"_s;
} // namespace

AboutDialog::AboutDialog(const core::Settings& settings, core::ThemeService& theme, const QString& userAgent,
                         const QString& engineSummary, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_userAgent(userAgent)
    , m_engineSummary(engineSummary)
{
    setupUi();
}

void AboutDialog::setupUi()
{
    setWindowTitle(tr("About Red"));
    setModal(true);
    setMinimumWidth(520);
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 18);
    root->setSpacing(14);

    auto* hero = new QHBoxLayout;
    hero->setSpacing(18);
    auto* icon = new QLabel(this);
    icon->setPixmap(icons::brand().pixmap(QSize(88, 88), devicePixelRatioF()));
    icon->setFixedSize(88, 88);
    hero->addWidget(icon, 0, Qt::AlignTop);
    auto* identity = new QVBoxLayout;
    identity->setSpacing(3);
    auto* name = new QLabel(u"Red"_s, this);
    name->setProperty("redHeading", true);
    identity->addWidget(name);
    auto* tagline = new QLabel(tr("YouTube, in a real desktop app."), this);
    identity->addWidget(tagline);
    auto* version = new QLabel(tr("Version %1").arg(QApplication::applicationVersion()), this);
    version->setProperty("redMuted", true);
    identity->addWidget(version);
    identity->addSpacing(6);
    auto* author = new QLabel(
        tr("Designed and developed by Keshav Bhatt · <a href=\"%1\">ktechpit.com</a>").arg(kWebsiteUrl),
        this);
    author->setTextFormat(Qt::RichText);
    author->setOpenExternalLinks(false);
    author->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(author, &QLabel::linkActivated, this, [](const QString& link) { platform::openUrl(link); });
    identity->addWidget(author);
    auto* disclaimer =
        new QLabel(tr("YouTube is a trademark of Google LLC. Red is an independent client and is not "
                      "affiliated with, endorsed by, or sponsored by YouTube or Google."),
                   this);
    disclaimer->setProperty("redMuted", true);
    disclaimer->setWordWrap(true);
    identity->addWidget(disclaimer);
    hero->addLayout(identity, 1);
    root->addLayout(hero);

    auto* links = new QHBoxLayout;
    links->setSpacing(8);
    const auto linkButton = [this](const QString& label, const QString& url) {
        auto* button = new QPushButton(label, this);
        connect(button, &QPushButton::clicked, this, [url] { platform::openUrl(url); });
        return button;
    };
    links->addWidget(linkButton(tr("Online guide"), links::kGuide));
    links->addWidget(linkButton(tr("Report a bug"), links::kIssues));
    links->addWidget(linkButton(tr("Donate"), kDonateUrl));
    links->addWidget(linkButton(tr("More apps"), links::kMoreApps));
    links->addStretch(1);
    root->addLayout(links);

    auto* sep = new QFrame(this);
    sep->setProperty("redSeparator", true);
    root->addWidget(sep);

    auto* debugRow = new QHBoxLayout;
    auto* debugTitle = new QLabel(tr("Diagnostics"), this);
    debugTitle->setProperty("redSection", true);
    debugRow->addWidget(debugTitle, 1);
    auto* copy = new QPushButton(tr("Copy"), this);
    copy->setToolTip(
        tr("Copies versions, paths and recent log lines for a bug report. Your session is never included."));
    connect(copy, &QPushButton::clicked, this, &AboutDialog::copyDiagnostics);
    debugRow->addWidget(copy);
    root->addLayout(debugRow);
    m_debugText = new QPlainTextEdit(this);
    m_debugText->setReadOnly(true);
    m_debugText->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_debugText->setPlainText(buildDiagnostics(m_settings, m_userAgent, m_engineSummary, 30));
    m_debugText->setMinimumHeight(160);
    root->addWidget(m_debugText, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* close = new QPushButton(tr("Close"), this);
    close->setProperty("redPrimary", true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(close);
    root->addLayout(buttons);
}

void AboutDialog::copyDiagnostics()
{
    QApplication::clipboard()->setText(buildDiagnostics(m_settings, m_userAgent, m_engineSummary));
}

} // namespace red::ui
