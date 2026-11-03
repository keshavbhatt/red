#include "ui/settings_dialog.h"

#include "core/downloads/download_options.h"
#include "core/downloads/engine_spec.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/message_sheet.h"
#include "ui/red_style.h"
#include "web/request_interceptor.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {

QLabel* mutedLabel(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setProperty("redMuted", true);
    l->setWordWrap(true);
    return l;
}

QFormLayout* form(QWidget* host)
{
    auto* f = new QFormLayout(host);
    f->setContentsMargins(0, 0, 0, 0);
    f->setHorizontalSpacing(18);
    f->setVerticalSpacing(10);
    f->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return f;
}

QString sponsorCategoryLabel(const QString& id)
{
    static const QHash<QString, QString> kLabels{
        {u"sponsor"_s, QObject::tr("Sponsor")},
        {u"selfpromo"_s, QObject::tr("Unpaid / self promotion")},
        {u"interaction"_s, QObject::tr("Interaction reminder")},
        {u"intro"_s, QObject::tr("Intermission / intro")},
        {u"outro"_s, QObject::tr("Endcards / credits")},
        {u"preview"_s, QObject::tr("Preview / recap")},
        {u"music_offtopic"_s, QObject::tr("Non-music section")},
        {u"filler"_s, QObject::tr("Filler tangent")},
    };
    return kLabels.value(id, id);
}

} // namespace

SettingsDialog::SettingsDialog(core::Settings& settings, core::ThemeService& theme,
                               services::EngineManager& engine, web::RequestInterceptor& interceptor,
                               bool trayAvailable, QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_theme(theme)
    , m_engine(engine)
    , m_interceptor(interceptor)
    , m_trayAvailable(trayAvailable)
{
    setWindowTitle(tr("Settings"));
    setModal(false);
    resize(900, 620);
    setupUi();
    loadValues();
    installWheelGuards();
    refreshEngine(m_engine.status());
    connect(&m_engine, &services::EngineManager::statusChanged, this, &SettingsDialog::refreshEngine);
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { applyNavIcons(); });
    const QByteArray geometry = m_settings.settingsDialogGeometry();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    auto* counter = new QTimer(this);
    counter->setInterval(2000);
    connect(counter, &QTimer::timeout, this, [this] {
        m_blockedCount->setText(tr("%1 requests blocked since launch · %2 lifetime")
                                    .arg(m_interceptor.blockedCount())
                                    .arg(m_settings.blockedCount() + m_interceptor.blockedCount()));
    });
    counter->start();
}

void SettingsDialog::closeEvent(QCloseEvent* event)
{
    m_settings.setSettingsDialogGeometry(saveGeometry());
    QDialog::closeEvent(event);
}

// ---- layout ----------------------------------------------------------------

void SettingsDialog::setupUi()
{
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_nav = new QListWidget(this);
    m_nav->setProperty("redNav", true);
    m_nav->setFixedWidth(200);
    m_nav->setFrameShape(QFrame::NoFrame);
    m_nav->setIconSize(QSize(18, 18));
    m_nav->setSpacing(0);
    m_nav->setFocusPolicy(Qt::NoFocus);
    for (const QString& name : {tr("General"), tr("Appearance"), tr("Playback"), tr("Blocking"),
                                tr("Downloads"), tr("TV mode"), tr("Advanced")}) {
        m_nav->addItem(name);
    }
    root->addWidget(m_nav);

    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::NoFrame); // a VLine paints Fusion's bright bevel; a flat hairline instead
    line->setProperty("redVSeparator", true);
    root->addWidget(line);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildGeneral());
    m_pages->addWidget(buildAppearance());
    m_pages->addWidget(buildPlayback());
    m_pages->addWidget(buildBlocking());
    m_pages->addWidget(buildDownloads());
    m_pages->addWidget(buildTv());
    m_pages->addWidget(buildAdvanced());
    root->addWidget(m_pages, 1);
    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);
    applyNavIcons();
}

void SettingsDialog::applyNavIcons()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const QStringList names{u"general"_s,   u"palette"_s, u"play"_s,    u"shield"_s,
                            u"downloads"_s, u"tv"_s,      u"advanced"_s};
    for (int i = 0; i < m_nav->count() && i < names.size(); ++i) {
        m_nav->item(i)->setIcon(icons::themed(names.at(i), t.text));
    }
}

void SettingsDialog::showPage(int index)
{
    m_nav->setCurrentRow(std::clamp(index, 0, m_nav->count() - 1));
}

QWidget* SettingsDialog::makeCard(const QString& title, QWidget* content)
{
    auto* card = new QFrame(this);
    card->setProperty("redCard", true);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(12);
    if (!title.isEmpty()) {
        auto* t = new QLabel(title, card);
        t->setProperty("redSection", true);
        layout->addWidget(t);
    }
    content->setParent(card);
    layout->addWidget(content);
    return card;
}

QWidget* SettingsDialog::wrapPage(const QString& title, const QString& subtitle, const QList<QWidget*>& cards)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* page = new QWidget(scroll);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    auto* heading = new QLabel(title, page);
    heading->setProperty("redHeading", true);
    layout->addWidget(heading);
    if (!subtitle.isEmpty()) {
        layout->addWidget(mutedLabel(subtitle, page));
    }
    layout->addSpacing(4);
    for (QWidget* card : cards) {
        layout->addWidget(card);
    }
    layout->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

// ---- pages -----------------------------------------------------------------

QWidget* SettingsDialog::buildGeneral()
{
    auto* startup = new QWidget(this);
    auto* f = form(startup);
    m_startMode = new QComboBox(startup);
    m_startMode->addItem(tr("YouTube"), static_cast<int>(core::StartMode::Desktop));
    m_startMode->addItem(tr("YouTube Music"), static_cast<int>(core::StartMode::Music));
    m_startMode->addItem(tr("TV mode"), static_cast<int>(core::StartMode::Tv));
    m_startMode->addItem(tr("The mode I used last"), static_cast<int>(core::StartMode::LastUsed));
    connect(m_startMode, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setStartMode(static_cast<core::StartMode>(m_startMode->currentData().toInt()));
    });
    f->addRow(tr("Open Red in"), m_startMode);
    m_startPage = new QComboBox(startup);
    m_startPage->addItem(tr("Home"), static_cast<int>(core::StartPage::Home));
    m_startPage->addItem(tr("Subscriptions"), static_cast<int>(core::StartPage::Subscriptions));
    m_startPage->addItem(tr("Where I left off"), static_cast<int>(core::StartPage::LastVisited));
    connect(m_startPage, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setStartPage(static_cast<core::StartPage>(m_startPage->currentData().toInt()));
    });
    f->addRow(tr("YouTube start page"), m_startPage);
    m_closeAction = new QComboBox(startup);
    m_closeAction->addItem(tr("Quit Red"), static_cast<int>(core::CloseAction::Quit));
    m_closeAction->addItem(tr("Keep running in the tray"),
                           static_cast<int>(core::CloseAction::MinimizeToTray));
    connect(m_closeAction, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setCloseAction(static_cast<core::CloseAction>(m_closeAction->currentData().toInt()));
    });
    f->addRow(tr("When closing the window"), m_closeAction);

    auto* integration = new QWidget(this);
    auto* v = new QVBoxLayout(integration);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);
    m_trayEnabled = new QCheckBox(tr("Show a tray icon with playback controls"), integration);
    connect(m_trayEnabled, &QCheckBox::toggled, this, [this](bool on) { m_settings.setTrayEnabled(on); });
    v->addWidget(m_trayEnabled);
    m_showRail = new QCheckBox(tr("Show the side rail"), integration);
    connect(m_showRail, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.setShowRail(on);
        m_railLabels->setEnabled(on);
    });
    v->addWidget(m_showRail);
    m_railLabels = new QCheckBox(tr("Show labels next to the rail's icons"), integration);
    connect(m_railLabels, &QCheckBox::toggled, this, [this](bool on) { m_settings.setRailLabels(on); });
    v->addWidget(m_railLabels);
    connect(&m_settings, &core::Settings::railLabelsChanged, m_railLabels, [this](bool on) {
        const QSignalBlocker blocker(m_railLabels); // the rail's own toggle, while Settings is open
        m_railLabels->setChecked(on);
    });
    v->addWidget(mutedLabel(tr("Without the rail, F10 opens its menu and every other button has a keyboard "
                               "shortcut (F1 lists them)."),
                            integration));
    m_middleScroll = new QCheckBox(tr("Scroll with the middle mouse button"), integration);
    connect(m_middleScroll, &QCheckBox::toggled, this,
            [this](bool on) { m_settings.setMiddleClickScroll(on); });
    v->addWidget(m_middleScroll);
    v->addWidget(mutedLabel(tr("Middle-click the page, then move the mouse up or down; click again to stop. "
                               "Takes effect after a restart."),
                            integration));
    m_notify = new QCheckBox(tr("Notify me when a download finishes"), integration);
    connect(m_notify, &QCheckBox::toggled, this,
            [this](bool on) { m_settings.setNotifyOnDownloadFinish(on); });
    v->addWidget(m_notify);

    return wrapPage(
        tr("General"), tr("How Red starts, closes and sits on your desktop."),
        {makeCard(tr("Startup and window"), startup), makeCard(tr("Desktop integration"), integration)});
}

QWidget* SettingsDialog::buildAppearance()
{
    auto* look = new QWidget(this);
    auto* f = form(look);
    m_theme_ = new QComboBox(look);
    m_theme_->addItem(tr("Follow the system"), static_cast<int>(core::Theme::System));
    m_theme_->addItem(tr("Light"), static_cast<int>(core::Theme::Light));
    m_theme_->addItem(tr("Dark"), static_cast<int>(core::Theme::Dark));
    connect(m_theme_, &QComboBox::currentIndexChanged, this,
            [this](int) { m_settings.setTheme(static_cast<core::Theme>(m_theme_->currentData().toInt())); });
    f->addRow(tr("Theme"), m_theme_);
    f->addRow(QString(), mutedLabel(tr("Applies to Red's own chrome and to YouTube itself."), look));
    m_zoom = new QDoubleSpinBox(look);
    m_zoom->setRange(50, 300);
    m_zoom->setSuffix(u" %"_s);
    m_zoom->setDecimals(0);
    m_zoom->setSingleStep(10);
    connect(m_zoom, &QDoubleSpinBox::valueChanged, this,
            [this](double v) { m_settings.setZoomFactor(v / 100.0); });
    f->addRow(tr("Page zoom"), m_zoom);
    m_scale = new QDoubleSpinBox(look);
    m_scale->setRange(core::Settings::kMinInterfaceScale * 100, core::Settings::kMaxInterfaceScale * 100);
    m_scale->setSuffix(u" %"_s);
    m_scale->setDecimals(0);
    m_scale->setSingleStep(25);
    connect(m_scale, &QDoubleSpinBox::valueChanged, this,
            [this](double v) { m_settings.setInterfaceScale(v / 100.0); });
    f->addRow(tr("Interface scale"), m_scale);
    f->addRow(QString(),
              mutedLabel(tr("Scales Red's chrome and dialogs. Takes effect after a restart."), look));
    return wrapPage(tr("Appearance"), tr("Theme and sizing."), {makeCard(tr("Look"), look)});
}

QWidget* SettingsDialog::buildPlayback()
{
    auto* player = new QWidget(this);
    auto* f = form(player);
    m_autoplay = new QCheckBox(tr("Start playing without a click"), player);
    connect(m_autoplay, &QCheckBox::toggled, this, [this](bool on) { m_settings.setAutoplay(on); });
    f->addRow(tr("Autoplay"), m_autoplay);
    m_codec = new QComboBox(player);
    m_codec->addItem(tr("Let YouTube decide"), static_cast<int>(core::CodecPreference::Auto));
    m_codec->addItem(tr("H.264 — lowest CPU use"), static_cast<int>(core::CodecPreference::H264));
    m_codec->addItem(tr("VP9"), static_cast<int>(core::CodecPreference::Vp9));
    m_codec->addItem(tr("AV1 — best quality per bit"), static_cast<int>(core::CodecPreference::Av1));
    connect(m_codec, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setCodecPreference(static_cast<core::CodecPreference>(m_codec->currentData().toInt()));
    });
    f->addRow(tr("Preferred codec"), m_codec);
    f->addRow(
        QString(),
        mutedLabel(tr("H.264 plays on every GPU and uses a fraction of the CPU. Applies to the next video."),
                   player));
    m_hwDecode = new QCheckBox(tr("Decode video on the GPU (VA-API, experimental)"), player);
    connect(m_hwDecode, &QCheckBox::toggled, this,
            [this](bool on) { m_settings.setHardwareVideoDecode(on); });
    f->addRow(tr("Hardware decoding"), m_hwDecode);
    f->addRow(QString(), mutedLabel(tr("Needs a working VA-API driver. Takes effect after a restart; turn it "
                                       "off if video goes blank."),
                                    player));

    auto* behaviour = new QWidget(this);
    auto* g = form(behaviour);
    m_keepAwake = new QCheckBox(tr("Keep the screen awake while a video plays"), behaviour);
    connect(m_keepAwake, &QCheckBox::toggled, this, [this](bool on) { m_settings.setKeepScreenAwake(on); });
    g->addRow(tr("Screen"), m_keepAwake);
    m_background = new QCheckBox(tr("Keep playing when the window is hidden or minimized"), behaviour);
    connect(m_background, &QCheckBox::toggled, this, [this](bool on) { m_settings.setPlayInBackground(on); });
    g->addRow(tr("Background"), m_background);
    m_taskbarProgress =
        new QCheckBox(tr("Show the playback position as a progress bar on the taskbar"), behaviour);
    connect(m_taskbarProgress, &QCheckBox::toggled, this,
            [this](bool on) { m_settings.setTaskbarProgress(on); });
    g->addRow(tr("Taskbar"), m_taskbarProgress);
    m_miniSize = new QComboBox(behaviour);
    m_miniSize->addItem(tr("Small"), static_cast<int>(core::MiniPlayerSize::Small));
    m_miniSize->addItem(tr("Medium"), static_cast<int>(core::MiniPlayerSize::Medium));
    m_miniSize->addItem(tr("Large"), static_cast<int>(core::MiniPlayerSize::Large));
    connect(m_miniSize, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setMiniPlayerSize(static_cast<core::MiniPlayerSize>(m_miniSize->currentData().toInt()));
    });
    g->addRow(tr("Mini player size"), m_miniSize);
    g->addRow(QString(),
              mutedLabel(tr("Videos get a wide window; Shorts and YouTube Music a tall one."), behaviour));
    return wrapPage(tr("Playback"), tr("How videos play."),
                    {makeCard(tr("Player"), player), makeCard(tr("Behaviour"), behaviour)});
}

QWidget* SettingsDialog::buildBlocking()
{
    auto* ads = new QWidget(this);
    auto* v = new QVBoxLayout(ads);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);
    m_blockAds = new QCheckBox(tr("Block ads"), ads);
    connect(m_blockAds, &QCheckBox::toggled, this, [this](bool on) { m_settings.setBlockAds(on); });
    v->addWidget(m_blockAds);
    v->addWidget(mutedLabel(tr("Removes ads from the player and feeds before YouTube renders them, and "
                               "blocks ad networks at the network level."),
                            ads));
    m_blockTrackers = new QCheckBox(tr("Block trackers"), ads);
    connect(m_blockTrackers, &QCheckBox::toggled, this, [this](bool on) { m_settings.setBlockTrackers(on); });
    v->addWidget(m_blockTrackers);
    v->addWidget(mutedLabel(
        tr("Stops YouTube's event logging and third-party analytics. Watch history still works."), ads));
    m_hideShorts = new QCheckBox(tr("Hide Shorts"), ads);
    connect(m_hideShorts, &QCheckBox::toggled, this, [this](bool on) { m_settings.setHideShorts(on); });
    v->addWidget(m_hideShorts);
    m_showDislikes = new QCheckBox(tr("Show dislike counts (Return YouTube Dislike)"), ads);
    connect(m_showDislikes, &QCheckBox::toggled, this, [this](bool on) { m_settings.setShowDislikes(on); });
    v->addWidget(m_showDislikes);
    m_blockedCount = mutedLabel(QString(), ads);
    v->addWidget(m_blockedCount);

    auto* sponsor = new QWidget(this);
    auto* s = new QVBoxLayout(sponsor);
    s->setContentsMargins(0, 0, 0, 0);
    s->setSpacing(10);
    m_sponsorEnabled = new QCheckBox(tr("Skip sponsored segments with SponsorBlock"), sponsor);
    connect(m_sponsorEnabled, &QCheckBox::toggled, this,
            [this](bool on) { m_settings.setSponsorBlockEnabled(on); });
    s->addWidget(m_sponsorEnabled);
    s->addWidget(mutedLabel(
        tr("Community-submitted segments from sponsor.ajay.app. Only a short hash of the video id is sent."),
        sponsor));
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(8);
    int row = 0;
    for (const QString& category : core::sponsorCategories()) {
        auto* label = new QLabel(sponsorCategoryLabel(category), sponsor);
        auto* combo = new QComboBox(sponsor);
        combo->setProperty("sponsorCategory", category);
        combo->addItem(tr("Skip automatically"), static_cast<int>(core::SponsorAction::Skip));
        combo->addItem(tr("Show in the progress bar"), static_cast<int>(core::SponsorAction::Show));
        combo->addItem(tr("Ignore"), static_cast<int>(core::SponsorAction::Off));
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int) {
            m_settings.setSponsorAction(combo->property("sponsorCategory").toString(),
                                        static_cast<core::SponsorAction>(combo->currentData().toInt()));
        });
        m_sponsorCombos << combo;
        // One column: two label + drop-down pairs side by side were wider than
        // the dialog's default size, and the page clipped on the right.
        grid->addWidget(label, row, 0);
        grid->addWidget(combo, row, 1);
        ++row;
    }
    grid->setColumnStretch(1, 1);
    s->addLayout(grid);
    m_sponsorMarkers = new QCheckBox(tr("Show segment markers on the progress bar"), sponsor);
    connect(m_sponsorMarkers, &QCheckBox::toggled, this,
            [this](bool on) { m_settings.setSponsorBlockMarkers(on); });
    s->addWidget(m_sponsorMarkers);
    return wrapPage(tr("Blocking"), tr("Ads, trackers, Shorts and sponsor segments."),
                    {makeCard(tr("Ads and trackers"), ads), makeCard(tr("SponsorBlock"), sponsor)});
}

QWidget* SettingsDialog::buildDownloads()
{
    auto* where = new QWidget(this);
    auto* f = form(where);
    auto* folderRow = new QWidget(where);
    auto* fr = new QHBoxLayout(folderRow);
    fr->setContentsMargins(0, 0, 0, 0);
    m_folder = new QLineEdit(folderRow);
    m_folder->setReadOnly(true);
    fr->addWidget(m_folder, 1);
    auto* change = new QPushButton(tr("Change…"), folderRow);
    connect(change, &QPushButton::clicked, this, [this] {
        const QString dir =
            QFileDialog::getExistingDirectory(this, tr("Download folder"), m_settings.downloadDirectory());
        if (!dir.isEmpty()) {
            m_settings.setDownloadDirectory(dir);
            m_folder->setText(dir);
        }
    });
    fr->addWidget(change);
    f->addRow(tr("Save to"), folderRow);
    m_pattern = new QComboBox(where);
    m_pattern->addItem(tr("Title.mp4"), static_cast<int>(core::FilenamePattern::Title));
    m_pattern->addItem(tr("Title [id].mp4"), static_cast<int>(core::FilenamePattern::TitleId));
    m_pattern->addItem(tr("Channel - Title.mp4"), static_cast<int>(core::FilenamePattern::ChannelTitle));
    connect(m_pattern, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setFilenamePattern(static_cast<core::FilenamePattern>(m_pattern->currentData().toInt()));
    });
    f->addRow(tr("File names"), m_pattern);
    m_organise = new QCheckBox(tr("Sort downloads into folders (Videos, Music, Playlists, Channels)"), where);
    connect(m_organise, &QCheckBox::toggled, this, [this](bool on) { m_settings.setOrganiseDownloads(on); });
    f->addRow(tr("Folders"), m_organise);

    auto* defaults = new QWidget(this);
    auto* d = form(defaults);
    m_quality = new QComboBox(defaults);
    for (int q = 0; q <= static_cast<int>(core::VideoQuality::Q360); ++q) {
        m_quality->addItem(core::qualityLabel(static_cast<core::VideoQuality>(q)), q);
    }
    connect(m_quality, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setDefaultQuality(static_cast<core::VideoQuality>(m_quality->currentData().toInt()));
    });
    d->addRow(tr("Video quality"), m_quality);
    m_container = new QComboBox(defaults);
    m_container->addItem(u"MP4"_s, static_cast<int>(core::Container::Mp4));
    m_container->addItem(u"MKV"_s, static_cast<int>(core::Container::Mkv));
    m_container->addItem(u"WebM"_s, static_cast<int>(core::Container::Webm));
    connect(m_container, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setDefaultContainer(static_cast<core::Container>(m_container->currentData().toInt()));
    });
    d->addRow(tr("Container"), m_container);
    m_audioFormat = new QComboBox(defaults);
    m_audioFormat->addItem(tr("Best (original)"), static_cast<int>(core::AudioFormat::Best));
    m_audioFormat->addItem(u"MP3"_s, static_cast<int>(core::AudioFormat::Mp3));
    m_audioFormat->addItem(u"M4A"_s, static_cast<int>(core::AudioFormat::M4a));
    m_audioFormat->addItem(u"Opus"_s, static_cast<int>(core::AudioFormat::Opus));
    m_audioFormat->addItem(u"FLAC"_s, static_cast<int>(core::AudioFormat::Flac));
    m_audioFormat->addItem(u"WAV"_s, static_cast<int>(core::AudioFormat::Wav));
    connect(m_audioFormat, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setDefaultAudioFormat(
            static_cast<core::AudioFormat>(m_audioFormat->currentData().toInt()));
    });
    d->addRow(tr("Audio-only format"), m_audioFormat);
    m_embedThumb = new QCheckBox(tr("Embed the thumbnail"), defaults);
    connect(m_embedThumb, &QCheckBox::toggled, this, [this](bool on) { m_settings.setEmbedThumbnail(on); });
    d->addRow(tr("Extras"), m_embedThumb);
    m_embedMeta = new QCheckBox(tr("Embed metadata and chapters"), defaults);
    connect(m_embedMeta, &QCheckBox::toggled, this, [this](bool on) { m_settings.setEmbedMetadata(on); });
    d->addRow(QString(), m_embedMeta);
    m_removeSponsors = new QCheckBox(tr("Cut sponsor segments out of downloads"), defaults);
    connect(m_removeSponsors, &QCheckBox::toggled, this,
            [this](bool on) { m_settings.setRemoveSponsors(on); });
    d->addRow(QString(), m_removeSponsors);

    auto* transfer = new QWidget(this);
    auto* t = form(transfer);
    m_concurrent = new QSpinBox(transfer);
    m_concurrent->setRange(1, core::Settings::kMaxConcurrentDownloads);
    connect(m_concurrent, &QSpinBox::valueChanged, this,
            [this](int v) { m_settings.setConcurrentDownloads(v); });
    t->addRow(tr("Downloads at once"), m_concurrent);
    m_speedLimit = new QSpinBox(transfer);
    m_speedLimit->setRange(0, 1000000);
    m_speedLimit->setSingleStep(500);
    m_speedLimit->setSpecialValueText(tr("Unlimited"));
    m_speedLimit->setSuffix(tr(" kB/s"));
    connect(m_speedLimit, &QSpinBox::valueChanged, this, [this](int v) { m_settings.setSpeedLimitKbps(v); });
    t->addRow(tr("Speed limit"), m_speedLimit);
    m_cookies = new QCheckBox(tr("Use my YouTube sign-in for downloads"), transfer);
    connect(m_cookies, &QCheckBox::toggled, this, [this](bool on) { m_settings.setUseSessionCookies(on); });
    t->addRow(tr("Sign-in"), m_cookies);
    t->addRow(QString(),
              mutedLabel(tr("Needed for age-restricted, members-only and rate-limited videos. Cookies are "
                            "handed to yt-dlp in a private temporary file and never stored."),
                         transfer));

    auto* engine = new QWidget(this);
    auto* e = new QVBoxLayout(engine);
    e->setContentsMargins(0, 0, 0, 0);
    e->setSpacing(10);
    auto* engineRow = new QHBoxLayout;
    m_engineSummary = mutedLabel(QString(), engine);
    engineRow->addWidget(m_engineSummary, 1);
    m_engineButton = new QPushButton(tr("Manage…"), engine);
    connect(m_engineButton, &QPushButton::clicked, this, &SettingsDialog::engineDialogRequested);
    engineRow->addWidget(m_engineButton);
    e->addLayout(engineRow);
    m_engineAutoUpdate =
        new QCheckBox(tr("Keep the download engine up to date automatically (checked daily)"), engine);
    connect(m_engineAutoUpdate, &QCheckBox::toggled, this,
            [this](bool on) { m_settings.setEngineAutoUpdate(on); });
    e->addWidget(m_engineAutoUpdate);
    m_engineUseSystem = new QCheckBox(tr("Use an engine already installed on this system (yt-dlp)"), engine);
    connect(m_engineUseSystem, &QCheckBox::toggled, this, [this](bool on) {
        m_settings.setEngineUseSystem(on);
        m_engineSystemPath->setEnabled(on);
    });
    e->addWidget(m_engineUseSystem);
    m_engineSystemPath = new QLineEdit(engine);
    m_engineSystemPath->setPlaceholderText(tr("Path to yt-dlp (empty = search PATH)"));
    connect(m_engineSystemPath, &QLineEdit::editingFinished, this,
            [this] { m_settings.setEngineSystemPath(m_engineSystemPath->text().trimmed()); });
    e->addWidget(m_engineSystemPath);

    return wrapPage(tr("Downloads"),
                    tr("Where files go, what they look like, and the engine that fetches them."),
                    {makeCard(tr("Location"), where), makeCard(tr("Defaults for new downloads"), defaults),
                     makeCard(tr("Transfer"), transfer), makeCard(tr("Download engine"), engine)});
}

QWidget* SettingsDialog::buildTv()
{
    auto* tv = new QWidget(this);
    auto* v = new QVBoxLayout(tv);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);
    v->addWidget(
        mutedLabel(tr("TV mode shows YouTube's living-room interface, the one on smart TVs and consoles, "
                      "with keyboard, mouse and gamepad control. Switch to it with the TV button on the rail "
                      "or Ctrl+T; to open Red in it, see General."),
                   tv));
    m_tvFullscreen = new QCheckBox(tr("Go full screen when entering TV mode"), tv);
    connect(m_tvFullscreen, &QCheckBox::toggled, this, [this](bool on) { m_settings.setTvFullscreen(on); });
    v->addWidget(m_tvFullscreen);
    m_tvGamepad = new QCheckBox(tr("Gamepad support"), tv);
    connect(m_tvGamepad, &QCheckBox::toggled, this, [this](bool on) { m_settings.setTvGamepad(on); });
    v->addWidget(m_tvGamepad);
    m_tvHideShorts = new QCheckBox(tr("Hide Shorts in TV mode"), tv);
    connect(m_tvHideShorts, &QCheckBox::toggled, this, [this](bool on) { m_settings.setTvHideShorts(on); });
    v->addWidget(m_tvHideShorts);
    m_tvUnlock = new QCheckBox(tr("Allow resolutions above the screen size (4K on a 1080p display)"), tv);
    connect(m_tvUnlock, &QCheckBox::toggled, this, [this](bool on) { m_settings.setTvUnlockResolution(on); });
    v->addWidget(m_tvUnlock);
    m_tvLowMemory = new QCheckBox(tr("Low-memory mode (lighter interface)"), tv);
    connect(m_tvLowMemory, &QCheckBox::toggled, this, [this](bool on) { m_settings.setTvLowMemory(on); });
    v->addWidget(m_tvLowMemory);

    auto* keys = new QWidget(this);
    auto* k = new QGridLayout(keys);
    k->setContentsMargins(0, 0, 0, 0);
    k->setHorizontalSpacing(24);
    k->setVerticalSpacing(6);
    const QList<QPair<QString, QString>> rows{
        {tr("Arrows / Enter"), tr("Navigate / select")},
        {tr("Esc, Backspace, right click"), tr("Back")},
        {tr("Ctrl+T, or the Back to YouTube button"), tr("Back to YouTube")},
        {tr("Ctrl+D"), tr("Download the current video")},
        {tr("+ / −, M"), tr("Volume, mute")},
        {tr("A / D, S"), tr("Playback speed down / up by 0.05 (with Shift by 0.25), reset")},
        {tr("Gamepad A / B"), tr("Select / back")},
        {tr("Gamepad LT / RT"), tr("Seek back / forward")},
    };
    int r = 0;
    for (const auto& [key, what] : rows) {
        auto* kl = new QLabel(u"<b>%1</b>"_s.arg(key), keys);
        k->addWidget(kl, r, 0);
        k->addWidget(mutedLabel(what, keys), r, 1);
        ++r;
    }
    k->setColumnStretch(1, 1);
    return wrapPage(tr("TV mode"), tr("The living-room YouTube."),
                    {makeCard(tr("TV mode"), tv), makeCard(tr("Controls"), keys)});
}

QWidget* SettingsDialog::buildAdvanced()
{
    auto* graphics = new QWidget(this);
    auto* f = form(graphics);
    m_hwAccel = new QComboBox(graphics);
    m_hwAccel->addItem(tr("Automatic (recommended)"), static_cast<int>(core::HardwareAcceleration::Auto));
    m_hwAccel->addItem(tr("Always on (ignore the GPU blocklist)"),
                       static_cast<int>(core::HardwareAcceleration::On));
    m_hwAccel->addItem(tr("Off (software rendering)"), static_cast<int>(core::HardwareAcceleration::Off));
    connect(m_hwAccel, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setHardwareAcceleration(
            static_cast<core::HardwareAcceleration>(m_hwAccel->currentData().toInt()));
    });
    f->addRow(tr("Hardware acceleration"), m_hwAccel);
    f->addRow(QString(), mutedLabel(tr("Automatic falls back to software rendering by itself if the GPU "
                                       "crashes twice. Takes effect after a restart."),
                                    graphics));

    auto* identity = new QWidget(this);
    auto* i = form(identity);
    m_signInUa = new QComboBox(identity);
    m_signInUa->addItem(tr("Firefox on Google sign-in pages (default)"),
                        static_cast<int>(core::SignInUserAgent::Firefox));
    m_signInUa->addItem(tr("Same as the app (Chrome)"), static_cast<int>(core::SignInUserAgent::Chrome));
    connect(m_signInUa, &QComboBox::currentIndexChanged, this, [this](int) {
        m_settings.setSignInUserAgent(static_cast<core::SignInUserAgent>(m_signInUa->currentData().toInt()));
    });
    i->addRow(tr("Sign-in identity"), m_signInUa);
    i->addRow(QString(),
              mutedLabel(tr("Google refuses to sign in from embedded Chrome (\"This browser or app "
                            "may not be secure\"); only the sign-in pages see Firefox."),
                         identity));
    m_uaOverride = new QLineEdit(identity);
    m_uaOverride->setPlaceholderText(tr("Custom user agent (leave empty for the default)"));
    connect(m_uaOverride, &QLineEdit::editingFinished, this,
            [this] { m_settings.setUserAgentOverride(m_uaOverride->text()); });
    i->addRow(tr("User agent"), m_uaOverride);
    i->addRow(QString(), mutedLabel(tr("Changes apply to the next page load. TV mode uses its own identity."),
                                    identity));

    auto* storageButtons = new QWidget(this);
    auto* s = new QHBoxLayout(storageButtons);
    s->setContentsMargins(0, 0, 0, 0);
    s->setSpacing(8);
    auto* clearCache = new QPushButton(tr("Clear cache"), storageButtons);
    connect(clearCache, &QPushButton::clicked, this, [this] {
        Q_EMIT clearCacheRequested();
        showStatus(m_storageStatus, tr("Cache cleared."));
    });
    s->addWidget(clearCache);
    auto* clearSession = new QPushButton(tr("Sign out and clear session…"), storageButtons);
    connect(clearSession, &QPushButton::clicked, this, &SettingsDialog::clearSessionRequested);
    s->addWidget(clearSession);
    auto* resetPermissions = new QPushButton(tr("Reset site permissions"), storageButtons);
    resetPermissions->setToolTip(
        tr("Forget the microphone, camera and location answers; YouTube asks again."));
    connect(resetPermissions, &QPushButton::clicked, this, &SettingsDialog::resetPermissionsRequested);
    s->addWidget(resetPermissions);
    s->addStretch(1);
    m_storageStatus = mutedLabel(QString(), this);
    QWidget* storage = withStatus(storageButtons, m_storageStatus);

    auto* supportButtons = new QWidget(this);
    auto* su = new QHBoxLayout(supportButtons);
    su->setContentsMargins(0, 0, 0, 0);
    su->setSpacing(8);
    auto* logs = new QPushButton(tr("Open log folder"), supportButtons);
    connect(logs, &QPushButton::clicked, this, &SettingsDialog::openLogFolderRequested);
    su->addWidget(logs);
    auto* diag = new QPushButton(tr("Copy diagnostics"), supportButtons);
    connect(diag, &QPushButton::clicked, this, [this] {
        Q_EMIT copyDiagnosticsRequested();
        showStatus(m_supportStatus, tr("Diagnostics copied to the clipboard."));
    });
    su->addWidget(diag);
    auto* reset = new QPushButton(tr("Reset all settings…"), supportButtons);
    connect(reset, &QPushButton::clicked, this, [this] {
        if (MessageSheet::confirm(
                this, MessageSheet::Tone::Warning, tr("Reset all settings?"),
                tr("Every setting goes back to its default. Your sign-in and downloads are kept."),
                tr("Reset"))) {
            m_settings.resetToDefaults();
            loadValues();
            showStatus(m_supportStatus, tr("Every setting is back to its default."));
        }
    });
    su->addWidget(reset);
    su->addStretch(1);
    m_supportStatus = mutedLabel(QString(), this);
    QWidget* support = withStatus(supportButtons, m_supportStatus);

    return wrapPage(tr("Advanced"), tr("Graphics, identity, storage and support."),
                    {makeCard(tr("Graphics"), graphics), makeCard(tr("Identity"), identity),
                     makeCard(tr("Storage"), storage), makeCard(tr("Support"), support)});
}

/// A row of buttons with a confirmation line under it, hidden until used.
QWidget* SettingsDialog::withStatus(QWidget* buttons, QLabel* status)
{
    auto* host = new QWidget(this);
    auto* v = new QVBoxLayout(host);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(8);
    v->addWidget(buttons);
    status->hide();
    v->addWidget(status);
    return host;
}

/// Confirms an action that otherwise leaves no trace, for a few seconds.
void SettingsDialog::showStatus(QLabel* status, const QString& text)
{
    status->setText(text);
    status->show();
    QTimer::singleShot(5000, status, [status, text] {
        if (status->text() == text) {
            status->hide();
        }
    });
}

void SettingsDialog::showStorageStatus(const QString& text)
{
    showStatus(m_storageStatus, text);
}

// ---- values ----------------------------------------------------------------

void SettingsDialog::loadValues()
{
    const QSignalBlocker b0(m_startMode), b1(m_startPage), b2(m_closeAction), b3(m_theme_), b4(m_zoom),
        b5(m_scale), b6(m_codec), b7(m_miniSize), b8(m_quality), b9(m_container), b10(m_audioFormat),
        b11(m_concurrent), b12(m_speedLimit), b13(m_hwAccel), b14(m_signInUa), b15(m_pattern);
    m_startMode->setCurrentIndex(m_startMode->findData(static_cast<int>(m_settings.startMode())));
    m_startPage->setCurrentIndex(m_startPage->findData(static_cast<int>(m_settings.startPage())));
    m_closeAction->setCurrentIndex(m_closeAction->findData(static_cast<int>(m_settings.closeAction())));
    m_closeAction->setEnabled(m_trayAvailable);
    m_trayEnabled->setChecked(m_settings.trayEnabled());
    m_showRail->setChecked(m_settings.showRail());
    m_railLabels->setChecked(m_settings.railLabels());
    m_railLabels->setEnabled(m_settings.showRail());
    m_notify->setChecked(m_settings.notifyOnDownloadFinish());
    m_middleScroll->setChecked(m_settings.middleClickScroll());
    m_theme_->setCurrentIndex(m_theme_->findData(static_cast<int>(m_settings.theme())));
    m_zoom->setValue(m_settings.zoomFactor() * 100);
    m_scale->setValue(m_settings.interfaceScale() * 100);
    m_autoplay->setChecked(m_settings.autoplay());
    m_codec->setCurrentIndex(m_codec->findData(static_cast<int>(m_settings.codecPreference())));
    m_hwDecode->setChecked(m_settings.hardwareVideoDecode());
    m_keepAwake->setChecked(m_settings.keepScreenAwake());
    m_background->setChecked(m_settings.playInBackground());
    m_taskbarProgress->setChecked(m_settings.taskbarProgress());
    m_miniSize->setCurrentIndex(m_miniSize->findData(static_cast<int>(m_settings.miniPlayerSize())));
    m_blockAds->setChecked(m_settings.blockAds());
    m_blockTrackers->setChecked(m_settings.blockTrackers());
    m_hideShorts->setChecked(m_settings.hideShorts());
    m_showDislikes->setChecked(m_settings.showDislikes());
    m_sponsorEnabled->setChecked(m_settings.sponsorBlockEnabled());
    m_sponsorMarkers->setChecked(m_settings.sponsorBlockMarkers());
    for (QComboBox* combo : m_sponsorCombos) {
        const QSignalBlocker b(combo);
        combo->setCurrentIndex(combo->findData(
            static_cast<int>(m_settings.sponsorAction(combo->property("sponsorCategory").toString()))));
    }
    m_folder->setText(m_settings.downloadDirectory());
    m_pattern->setCurrentIndex(m_pattern->findData(static_cast<int>(m_settings.filenamePattern())));
    m_organise->setChecked(m_settings.organiseDownloads());
    m_quality->setCurrentIndex(m_quality->findData(static_cast<int>(m_settings.defaultQuality())));
    m_container->setCurrentIndex(m_container->findData(static_cast<int>(m_settings.defaultContainer())));
    m_audioFormat->setCurrentIndex(
        m_audioFormat->findData(static_cast<int>(m_settings.defaultAudioFormat())));
    m_embedThumb->setChecked(m_settings.embedThumbnail());
    m_embedMeta->setChecked(m_settings.embedMetadata());
    m_removeSponsors->setChecked(m_settings.removeSponsors());
    m_concurrent->setValue(m_settings.concurrentDownloads());
    m_speedLimit->setValue(m_settings.speedLimitKbps());
    m_cookies->setChecked(m_settings.useSessionCookies());
    m_engineAutoUpdate->setChecked(m_settings.engineAutoUpdate());
    m_engineUseSystem->setChecked(m_settings.engineUseSystem());
    m_engineSystemPath->setText(m_settings.engineSystemPath());
    m_engineSystemPath->setEnabled(m_settings.engineUseSystem());
    m_tvFullscreen->setChecked(m_settings.tvFullscreen());
    m_tvGamepad->setChecked(m_settings.tvGamepad());
    m_tvHideShorts->setChecked(m_settings.tvHideShorts());
    m_tvUnlock->setChecked(m_settings.tvUnlockResolution());
    m_tvLowMemory->setChecked(m_settings.tvLowMemory());
    m_hwAccel->setCurrentIndex(m_hwAccel->findData(static_cast<int>(m_settings.hardwareAcceleration())));
    m_signInUa->setCurrentIndex(m_signInUa->findData(static_cast<int>(m_settings.signInUserAgent())));
    m_uaOverride->setText(m_settings.userAgentOverride());
}

void SettingsDialog::refreshEngine(const services::EngineManager::Status& s)
{
    QString text;
    if (s.isReady()) {
        text = s.systemYtdlp ? tr("Engine from this system · media converter ready")
                             : tr("Engine %1 · media converter ready").arg(s.ytdlpVersion);
        if (s.updateAvailable) {
            text += tr(" · update %1 available").arg(s.latestVersion);
        }
    } else if (s.isBusy()) {
        text = s.stepLabel;
    } else if (s.state == services::EngineManager::State::Error || s.ffmpegMissing) {
        text = tr("Problem: %1").arg(s.error);
    } else {
        text = tr("Not set up yet — the first download will fetch it.");
    }
    m_engineSummary->setText(text);
    m_engineButton->setText(s.updateAvailable ? tr("Update…") : tr("Manage…"));
}

// ---- wheel guard -----------------------------------------------------------

void SettingsDialog::installWheelGuards()
{
    for (QComboBox* c : findChildren<QComboBox*>()) {
        c->setFocusPolicy(Qt::StrongFocus);
        c->installEventFilter(this);
    }
    for (QAbstractSpinBox* s : findChildren<QAbstractSpinBox*>()) {
        s->setFocusPolicy(Qt::StrongFocus);
        s->installEventFilter(this);
    }
}

bool SettingsDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget != nullptr && !widget->hasFocus()) {
            event->ignore();
            return true; // scroll the page, never a control the pointer happens to be over
        }
    }
    return QDialog::eventFilter(watched, event);
}

} // namespace red::ui
