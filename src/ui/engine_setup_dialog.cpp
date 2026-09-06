#include "ui/engine_setup_dialog.h"

#include "core/downloads/engine_spec.h"
#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/red_style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

EngineSetupDialog::EngineSetupDialog(services::EngineManager& engine, core::ThemeService& theme,
                                     QWidget* parent)
    : QDialog(parent)
    , m_engine(engine)
    , m_theme(theme)
{
    setWindowTitle(tr("Download engine"));
    setModal(true);
    setMinimumWidth(480);
    setupUi();
    refresh(m_engine.status());
    connect(&m_engine, &services::EngineManager::statusChanged, this, &EngineSetupDialog::refresh);
}

QWidget* EngineSetupDialog::makeRow(QLabel** glyph, QLabel** text, const QString& title)
{
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    *glyph = new QLabel(row);
    (*glyph)->setFixedSize(20, 20);
    layout->addWidget(*glyph);
    auto* name = new QLabel(title, row);
    name->setMinimumWidth(150);
    QFont f = name->font();
    f.setWeight(QFont::DemiBold);
    name->setFont(f);
    layout->addWidget(name);
    *text = new QLabel(row);
    (*text)->setProperty("redMuted", true);
    layout->addWidget(*text, 1);
    return row;
}

void EngineSetupDialog::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(12);
    auto* heading = new QLabel(tr("Setting up downloads"), this);
    heading->setProperty("redHeading", true);
    root->addWidget(heading);
    auto* intro = new QLabel(tr("Red downloads with yt-dlp. The engine and its helpers are fetched once from "
                                "their official releases and kept up to date automatically."),
                             this);
    intro->setProperty("redMuted", true);
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* card = new QFrame(this);
    card->setProperty("redCard", true);
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(10);
    cardLayout->addWidget(makeRow(&m_ytdlpGlyph, &m_ytdlpText, u"yt-dlp"_s));
    cardLayout->addWidget(makeRow(&m_jsGlyph, &m_jsText, tr("JavaScript runtime")));
    cardLayout->addWidget(makeRow(&m_ffmpegGlyph, &m_ffmpegText, u"ffmpeg"_s));
    root->addWidget(card);

    m_step = new QLabel(this);
    m_step->setProperty("redMuted", true);
    root->addWidget(m_step);
    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->setRange(0, 1000);
    root->addWidget(m_progress);
    m_error = new QLabel(this);
    m_error->setWordWrap(true);
    m_error->setVisible(false);
    root->addWidget(m_error);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    m_close = new QPushButton(tr("Close"), this);
    connect(m_close, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(m_close);
    m_action = new QPushButton(tr("Set up"), this);
    m_action->setProperty("redPrimary", true);
    connect(m_action, &QPushButton::clicked, this, [this] {
        if (m_engine.status().isReady()) {
            if (m_engine.status().updateAvailable) {
                m_engine.update();
            } else {
                m_engine.checkForUpdates(true);
            }
        } else if (m_engine.status().ffmpegMissing && !m_engine.status().ytdlpPath.isEmpty()) {
            m_engine.initialize(); // re-detect after the user installed ffmpeg
        } else {
            m_engine.install();
        }
    });
    buttons->addWidget(m_action);
    root->addLayout(buttons);
}

void EngineSetupDialog::refresh(const services::EngineManager::Status& s)
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const qreal dpr = devicePixelRatioF();
    auto glyph = [&](QLabel* label, bool ok, bool busy) {
        label->setPixmap(icons::pixmap(ok ? u"check"_s : (busy ? u"reload"_s : u"warning"_s),
                                       ok ? t.success : (busy ? t.link : t.warning), 20, dpr));
    };
    const bool busy = s.isBusy();
    glyph(m_ytdlpGlyph, !s.ytdlpPath.isEmpty(), busy);
    glyph(m_jsGlyph, !s.jsRuntime.isEmpty(), busy);
    glyph(m_ffmpegGlyph, !s.ffmpegPath.isEmpty(), busy);
    m_ytdlpText->setText(s.ytdlpPath.isEmpty()
                             ? tr("will be downloaded (~40 MB)")
                             : (s.systemYtdlp ? tr("system installation") : s.ytdlpVersion));
    m_jsText->setText(s.jsRuntime.isEmpty() ? tr("QuickJS will be downloaded (~3 MB)")
                                            : core::jsRuntimeLabel(s.jsRuntime));
    m_ffmpegText->setText(s.ffmpegPath.isEmpty() ? tr("not installed — %1").arg(core::ffmpegInstallHint())
                                                 : s.ffmpegPath);

    m_step->setText(busy ? s.stepLabel : (s.isReady() ? tr("Everything is ready.") : QString()));
    m_progress->setVisible(busy);
    if (busy) {
        if (s.progress >= 0) {
            m_progress->setRange(0, 1000);
            m_progress->setValue(static_cast<int>(s.progress * 1000));
        } else {
            m_progress->setRange(0, 0);
        }
    }
    m_error->setVisible(!s.error.isEmpty() &&
                        (s.state == services::EngineManager::State::Error || s.ffmpegMissing));
    m_error->setText(s.error);
    m_error->setStyleSheet(u"color:%1;"_s.arg(t.danger.name()));

    m_action->setEnabled(!busy);
    if (s.isReady()) {
        m_action->setText(s.updateAvailable ? tr("Update to %1").arg(s.latestVersion)
                                            : tr("Check for updates"));
        m_action->setProperty("redPrimary", s.updateAvailable);
    } else {
        const bool onlyFfmpeg = !s.ytdlpPath.isEmpty() && !s.jsRuntime.isEmpty() && s.ffmpegMissing;
        m_action->setText(onlyFfmpeg                                         ? tr("Check again")
                          : s.state == services::EngineManager::State::Error ? tr("Retry")
                                                                             : tr("Set up"));
        m_action->setProperty("redPrimary", true);
    }
    m_action->style()->unpolish(m_action);
    m_action->style()->polish(m_action);
    m_close->setText(s.isReady() ? tr("Done") : tr("Close"));
}

} // namespace red::ui
