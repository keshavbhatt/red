#include "ui/bug_report_dialog.h"

#include "core/theme/theme_service.h"
#include "platform/crash_handler.h"
#include "platform/file_manager.h"
#include "ui/diagnostics.h"
#include "ui/icons.h"
#include "ui/red_style.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kMaxTitle = 80;
} // namespace

BugReportDialog::BugReportDialog(const core::Settings& settings, const core::ThemeService& theme,
                                 QString userAgent, QString engineSummary, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_userAgent(std::move(userAgent))
    , m_engineSummary(std::move(engineSummary))
{
    setWindowTitle(tr("Report a bug"));
    setModal(true);
    setMinimumWidth(560);
    const Tokens t = Tokens::forScheme(theme.isDark());

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(12);

    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* icon = new QLabel(this);
    icon->setFixedSize(44, 44);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(u"background:%1;border-radius:22px;"_s.arg(t.brand.name()));
    icon->setPixmap(icons::pixmap(u"warning"_s, t.brandText, 24, devicePixelRatioF()));
    top->addWidget(icon, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* title = new QLabel(tr("Report a bug"), this);
    title->setProperty("redTitle", true);
    text->addWidget(title);
    auto* intro =
        new QLabel(tr("A pre-filled report opens in your browser or mail app, and the diagnostics "
                      "(versions, engine state, recent log lines) are copied to your clipboard: paste "
                      "them at the end of the report. Your sign-in and what you watched are never "
                      "included."),
                   this);
    intro->setProperty("redMuted", true);
    intro->setWordWrap(true);
    text->addWidget(intro);
    top->addLayout(text, 1);
    root->addLayout(top);

    auto* titleLabel = new QLabel(tr("Title"), this);
    titleLabel->setProperty("redSection", true);
    root->addWidget(titleLabel);
    m_title = new QLineEdit(this);
    m_title->setMaxLength(kMaxTitle);
    m_title->setPlaceholderText(tr("Short summary of the problem"));
    root->addWidget(m_title);

    auto* descriptionLabel = new QLabel(tr("What happened?"), this);
    descriptionLabel->setProperty("redSection", true);
    root->addWidget(descriptionLabel);
    m_description = new QPlainTextEdit(this);
    m_description->setPlaceholderText(
        tr("e.g. The video turns black after I switch to TV mode, and only a restart fixes it."));
    m_description->setMinimumHeight(110);
    root->addWidget(m_description, 1);

    m_includeCrash = new QCheckBox(tr("Include the last crash report"), this);
    const bool hasCrash = !platform::lastCrashReport().isEmpty();
    m_includeCrash->setChecked(hasCrash);
    m_includeCrash->setVisible(hasCrash);
    root->addWidget(m_includeCrash);

    m_status = new QLabel(this);
    m_status->setProperty("redMuted", true);
    m_status->setWordWrap(true);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(m_status);

    auto* buttons = new QHBoxLayout;
    auto* copy = new QPushButton(tr("Copy diagnostics"), this);
    copy->setProperty("redFlat", true);
    connect(copy, &QPushButton::clicked, this, &BugReportDialog::copyDiagnostics);
    buttons->addWidget(copy);
    buttons->addStretch(1);
    auto* mail = new QPushButton(tr("Send by email"), this);
    connect(mail, &QPushButton::clicked, this, &BugReportDialog::openMail);
    buttons->addWidget(mail);
    auto* issue = new QPushButton(tr("Open GitHub issue"), this);
    issue->setProperty("redPrimary", true);
    issue->setDefault(true);
    connect(issue, &QPushButton::clicked, this, &BugReportDialog::openIssue);
    buttons->addWidget(issue);
    auto* close = new QPushButton(tr("Close"), this);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(close);
    root->addLayout(buttons);
}

QString BugReportDialog::diagnostics() const
{
    return buildDiagnostics(m_settings, m_userAgent, m_engineSummary, 200, m_includeCrash->isChecked());
}

void BugReportDialog::copyDiagnostics()
{
    QApplication::clipboard()->setText(diagnostics());
    m_status->setText(tr("Diagnostics copied to the clipboard."));
}

void BugReportDialog::openIssue()
{
    reportOpened(bugReportUrl(m_userAgent, m_title->text(), m_description->toPlainText()), tr("GitHub"));
}

void BugReportDialog::openMail()
{
    reportOpened(bugReportMailUrl(m_userAgent, m_title->text(), m_description->toPlainText()),
                 tr("your mail app"));
}

void BugReportDialog::reportOpened(const QString& url, const QString& where)
{
    QApplication::clipboard()->setText(diagnostics());
    if (platform::openUrl(url)) {
        m_status->setText(tr("The report is opening in %1. Paste your clipboard (Ctrl+V) at the end to "
                             "attach the diagnostics.")
                              .arg(where));
    } else {
        // The report is already on the clipboard: say so and hand over the address.
        m_status->setText(
            tr("Could not open %1 automatically. The diagnostics are on your clipboard; open this "
               "address and paste them in:\n%2")
                .arg(where, url));
    }
}

} // namespace red::ui
