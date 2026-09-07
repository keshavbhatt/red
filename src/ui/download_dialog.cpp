#include "ui/download_dialog.h"

#include "core/blocking/sponsorblock.h"
#include "core/downloads/download_options.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "core/youtube_url.h"
#include "services/media_probe.h"
#include "ui/icons.h"
#include "ui/kind_card.h"
#include "ui/red_style.h"
#include "ui/thumbnail_cache.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {

constexpr int kThumbW = 168;
constexpr int kThumbH = 94;
constexpr int kFormatIdRole = Qt::UserRole + 1;
constexpr int kFormatKindRole = Qt::UserRole + 2; // 0 video-only, 1 audio-only, 2 progressive
constexpr int kEntryIndexRole = Qt::UserRole + 3;

QString homeRelative(const QString& path)
{
    const QString home = QDir::homePath();
    return path.startsWith(home) ? u"~"_s + path.mid(home.size()) : path;
}

} // namespace

DownloadDialog::DownloadDialog(const QUrl& url, core::Settings& settings, core::ThemeService& theme,
                               services::MediaProbe& probe, ThumbnailCache& thumbnails, QWidget* parent)
    : QDialog(parent)
    , m_url(url)
    , m_settings(settings)
    , m_theme(theme)
    , m_probe(probe)
    , m_thumbnails(thumbnails)
    , m_folder(settings.downloadDirectory())
{
    // A link can name a video, a playlist/channel, or both (watch?v=…&list=…).
    // Both scopes are read up front so the choice is informed and switching
    // between them is instant.
    const core::YouTubeUrlInfo info = core::classifyYouTubeUrl(url);
    const bool listOnly =
        info.kind == core::YouTubeUrlKind::Playlist || info.kind == core::YouTubeUrlKind::Channel;
    if (!listOnly && !info.videoId.isEmpty()) {
        m_video.url = QUrl(u"https://www.youtube.com/watch?v="_s + info.videoId);
    }
    if (listOnly) {
        m_playlist.url = url;
    } else if (!info.playlistId.isEmpty()) {
        m_playlist.url = QUrl(u"https://www.youtube.com/playlist?list="_s + info.playlistId);
    }
    if (m_video.url.isEmpty() && m_playlist.url.isEmpty()) {
        m_video.url = url; // let yt-dlp have a go at whatever this is
    }
    m_scope = m_video.url.isEmpty() ? Scope::Playlist : Scope::Video;
    m_url = scopeState(m_scope).url;
    m_isPlaylist = m_scope == Scope::Playlist;
    setWindowTitle(m_isPlaylist ? tr("Download playlist") : tr("Download video"));
    setModal(false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMinimumWidth(680);
    setupUi();
    applyDefaults();
    selectKind(m_settings.lastDownloadKind() == core::DownloadKind::Audio ? core::DownloadOptions::Kind::Audio
               : m_settings.lastDownloadKind() == core::DownloadKind::Custom
                   ? core::DownloadOptions::Kind::Custom
                   : core::DownloadOptions::Kind::Video);
    connect(this, &QDialog::accepted, this, [this] {
        const core::DownloadOptions::Kind k = kind();
        m_settings.setLastDownloadKind(k == core::DownloadOptions::Kind::Audio ? core::DownloadKind::Audio
                                       : k == core::DownloadOptions::Kind::Custom
                                           ? core::DownloadKind::Custom
                                           : core::DownloadKind::Video);
    });
    connect(&m_probe, &services::MediaProbe::finished, this, &DownloadDialog::handleProbeFinished);
    connect(&m_probe, &services::MediaProbe::failed, this, &DownloadDialog::handleProbeFailed);
    connect(&m_thumbnails, &ThumbnailCache::ready, this, [this](const QString&) { updateHero(); });
    updateScopeChooser();
    if (!m_video.url.isEmpty()) {
        startProbe(Scope::Video);
    }
    if (!m_playlist.url.isEmpty()) {
        startProbe(Scope::Playlist);
    }
    presentScope();
}

DownloadDialog::~DownloadDialog()
{
    for (ScopeState* state : {&m_video, &m_playlist}) {
        if (state->probing) {
            m_probe.cancel(state->probeId);
        }
    }
}

// ---- construction ----------------------------------------------------------

void DownloadDialog::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    root->setSpacing(14);
    root->addWidget(buildHero());
    m_scopeRow = buildScopeChooser();
    m_scopeRow->setVisible(false);
    root->addWidget(m_scopeRow);

    root->addWidget(buildKindChooser());
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildVideoTab());
    m_pages->addWidget(buildAudioTab());
    m_pages->addWidget(buildAdvancedTab());
    root->addWidget(m_pages);
    connect(m_kinds, &QButtonGroup::idToggled, this, [this](int id, bool on) {
        if (!on) {
            return;
        }
        m_pages->setCurrentIndex(id);
        // Let the dialog shrink to the page shown instead of the tallest one.
        for (int i = 0; i < m_pages->count(); ++i) {
            m_pages->widget(i)->setSizePolicy(QSizePolicy::Preferred,
                                              i == id ? QSizePolicy::Preferred : QSizePolicy::Ignored);
        }
        m_pages->adjustSize();
        updatePreview();
        adjustSize();
    });

    m_playlistSection = buildPlaylistSection();
    m_playlistSection->setVisible(false);
    root->addWidget(m_playlistSection);

    root->addWidget(buildFooter());
}

QWidget* DownloadDialog::buildScopeChooser()
{
    // Shown only for links that are a video inside a playlist.
    auto* card = new QFrame(this);
    card->setProperty("redCard", true);
    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(12);
    auto* icon = new QLabel(card);
    icon->setPixmap(
        icons::pixmap(u"playlist"_s, Tokens::forScheme(m_theme.isDark()).muted, 20, devicePixelRatioF()));
    layout->addWidget(icon, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    auto* heading = new QLabel(tr("This video is part of a playlist"), card);
    QFont f = heading->font();
    f.setWeight(QFont::DemiBold);
    heading->setFont(f);
    text->addWidget(heading);
    auto* choices = new QHBoxLayout;
    choices->setSpacing(18);
    m_scopeVideo = new QRadioButton(tr("Just this video"), card);
    m_scopeVideo->setChecked(true);
    m_scopePlaylist = new QRadioButton(tr("Whole playlist"), card);
    choices->addWidget(m_scopeVideo);
    choices->addWidget(m_scopePlaylist);
    choices->addStretch(1);
    text->addLayout(choices);
    auto* hintRow = new QHBoxLayout;
    hintRow->setSpacing(8);
    m_scopeHint = new QLabel(card);
    m_scopeHint->setProperty("redMuted", true);
    m_scopeHint->setWordWrap(true);
    hintRow->addWidget(m_scopeHint, 1);
    m_scopeRetry = new QPushButton(tr("Retry"), card);
    m_scopeRetry->setProperty("redFlat", true);
    m_scopeRetry->setVisible(false);
    connect(m_scopeRetry, &QPushButton::clicked, this, [this] { startProbe(Scope::Playlist); });
    hintRow->addWidget(m_scopeRetry);
    text->addLayout(hintRow);
    layout->addLayout(text, 1);
    connect(m_scopeVideo, &QRadioButton::toggled, this, [this](bool on) {
        if (on) {
            setScope(Scope::Video);
        }
    });
    connect(m_scopePlaylist, &QRadioButton::toggled, this, [this](bool on) {
        if (on) {
            setScope(Scope::Playlist);
        }
    });
    return card;
}

QWidget* DownloadDialog::buildKindChooser()
{
    // The first decision, made unmissable: what comes out of this download.
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    m_kinds = new QButtonGroup(this);
    m_kinds->setExclusive(true);
    const struct
    {
        QString icon;
        QString title;
        QString hint;
    } cards[] = {
        {u"video"_s, tr("Video"), tr("Picture and sound")},
        {u"music"_s, tr("Audio only"), tr("Just the sound, for music")},
        {u"sliders"_s, tr("Advanced"), tr("Pick the exact streams")},
    };
    int id = 0;
    for (const auto& card : cards) {
        auto* button = new KindCard(card.icon, card.title, card.hint, m_theme, row);
        m_kinds->addButton(button, id++);
        layout->addWidget(button, 1);
    }
    m_kinds->button(0)->setChecked(true);
    return row;
}

QWidget* DownloadDialog::buildHero()
{
    auto* hero = new QWidget(this);
    auto* layout = new QHBoxLayout(hero);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);
    m_thumb = new QLabel(hero);
    m_thumb->setFixedSize(kThumbW, kThumbH);
    m_thumb->setAlignment(Qt::AlignCenter);
    m_thumb->setStyleSheet(
        u"border-radius:8px;background:%1;"_s.arg(Tokens::forScheme(m_theme.isDark()).hover.name()));
    layout->addWidget(m_thumb, 0, Qt::AlignTop);

    auto* text = new QVBoxLayout;
    text->setSpacing(4);
    m_title = new QLabel(hero);
    m_title->setProperty("redTitle", true);
    m_title->setWordWrap(true);
    m_title->setMaximumHeight(48);
    text->addWidget(m_title);
    m_subtitle = new QLabel(hero);
    m_subtitle->setProperty("redMuted", true);
    m_subtitle->setWordWrap(true);
    text->addWidget(m_subtitle);
    auto* statusRow = new QHBoxLayout;
    m_status = new QLabel(hero);
    m_status->setProperty("redMuted", true);
    statusRow->addWidget(m_status);
    m_retry = new QPushButton(tr("Retry"), hero);
    m_retry->setVisible(false);
    connect(m_retry, &QPushButton::clicked, this, &DownloadDialog::retryProbe);
    statusRow->addWidget(m_retry);
    statusRow->addStretch(1);
    text->addLayout(statusRow);
    text->addStretch(1);
    layout->addLayout(text, 1);
    return hero;
}

QWidget* DownloadDialog::buildVideoTab()
{
    auto* tab = new QWidget(this);
    auto* grid = new QGridLayout(tab);
    grid->setContentsMargins(4, 14, 4, 4);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(10);

    auto label = [tab](const QString& text) {
        auto* l = new QLabel(text, tab);
        l->setProperty("redMuted", true);
        return l;
    };
    m_quality = new QComboBox(tab);
    for (int q = 0; q <= static_cast<int>(core::VideoQuality::Q360); ++q) {
        m_quality->addItem(core::qualityLabel(static_cast<core::VideoQuality>(q)), q);
    }
    m_container = new QComboBox(tab);
    m_container->addItem(u"MP4"_s, static_cast<int>(core::Container::Mp4));
    m_container->addItem(u"MKV"_s, static_cast<int>(core::Container::Mkv));
    m_container->addItem(u"WebM"_s, static_cast<int>(core::Container::Webm));
    m_subtitles = new QComboBox(tab);
    m_subtitles->addItem(tr("None"), QString());
    m_autoSubs = new QCheckBox(tr("Include auto-generated subtitles"), tab);
    m_embedSubs = new QCheckBox(tr("Embed subtitles in the file"), tab);
    m_embedThumb = new QCheckBox(tr("Embed thumbnail"), tab);
    m_embedMeta = new QCheckBox(tr("Embed metadata and chapters"), tab);
    m_removeSponsors = new QCheckBox(tr("Remove sponsor segments (SponsorBlock)"), tab);

    grid->addWidget(label(tr("Quality")), 0, 0);
    grid->addWidget(m_quality, 0, 1);
    grid->addWidget(label(tr("Container")), 0, 2);
    grid->addWidget(m_container, 0, 3);
    grid->addWidget(label(tr("Subtitles")), 1, 0);
    grid->addWidget(m_subtitles, 1, 1, 1, 3);
    // Toggles sit in two aligned columns under the pickers.
    grid->addWidget(m_autoSubs, 2, 0, 1, 2);
    grid->addWidget(m_embedSubs, 2, 2, 1, 2);
    grid->addWidget(m_embedThumb, 3, 0, 1, 2);
    grid->addWidget(m_embedMeta, 3, 2, 1, 2);
    grid->addWidget(m_removeSponsors, 4, 0, 1, 4);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    for (QComboBox* c : {m_quality, m_container, m_subtitles}) {
        connect(c, &QComboBox::currentIndexChanged, this, [this](int) { updatePreview(); });
    }
    return tab;
}

QWidget* DownloadDialog::buildAudioTab()
{
    auto* tab = new QWidget(this);
    auto* grid = new QGridLayout(tab);
    grid->setContentsMargins(4, 14, 4, 4);
    grid->setHorizontalSpacing(18);
    grid->setVerticalSpacing(10);
    auto label = [tab](const QString& text) {
        auto* l = new QLabel(text, tab);
        l->setProperty("redMuted", true);
        return l;
    };
    m_audioFormat = new QComboBox(tab);
    m_audioFormat->addItem(tr("Best (original codec)"), static_cast<int>(core::AudioFormat::Best));
    m_audioFormat->addItem(u"MP3"_s, static_cast<int>(core::AudioFormat::Mp3));
    m_audioFormat->addItem(u"M4A (AAC)"_s, static_cast<int>(core::AudioFormat::M4a));
    m_audioFormat->addItem(u"Opus"_s, static_cast<int>(core::AudioFormat::Opus));
    m_audioFormat->addItem(u"FLAC"_s, static_cast<int>(core::AudioFormat::Flac));
    m_audioFormat->addItem(u"WAV"_s, static_cast<int>(core::AudioFormat::Wav));
    m_audioQuality = new QComboBox(tab);
    m_audioQuality->addItem(tr("Best"), 0);
    m_audioQuality->addItem(u"256 kbps"_s, 256);
    m_audioQuality->addItem(u"192 kbps"_s, 192);
    m_audioQuality->addItem(u"128 kbps"_s, 128);
    m_audioEmbedThumb = new QCheckBox(tr("Embed cover art"), tab);
    m_audioEmbedMeta = new QCheckBox(tr("Embed metadata and chapters"), tab);
    grid->addWidget(label(tr("Format")), 0, 0);
    grid->addWidget(m_audioFormat, 0, 1);
    grid->addWidget(label(tr("Quality")), 0, 2);
    grid->addWidget(m_audioQuality, 0, 3);
    grid->addWidget(m_audioEmbedThumb, 1, 0, 1, 2);
    grid->addWidget(m_audioEmbedMeta, 1, 2, 1, 2);
    auto* hint = new QLabel(
        tr("Converting to MP3, FLAC or WAV re-encodes the audio; \"Best\" keeps YouTube's original stream."),
        tab);
    hint->setProperty("redMuted", true);
    hint->setWordWrap(true);
    grid->addWidget(hint, 2, 0, 1, 4);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);
    connect(m_audioFormat, &QComboBox::currentIndexChanged, this, [this](int) { updatePreview(); });
    return tab;
}

QWidget* DownloadDialog::buildAdvancedTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    layout->setContentsMargins(4, 12, 4, 4);
    layout->setSpacing(8);
    m_formatsHint =
        new QLabel(tr("Pick one video stream and one audio stream, or a single combined stream."), tab);
    m_formatsHint->setProperty("redMuted", true);
    layout->addWidget(m_formatsHint);
    m_formats = new QTreeWidget(tab);
    m_formats->setColumnCount(6);
    m_formats->setHeaderLabels(
        {tr("Stream"), tr("Resolution / bitrate"), tr("FPS"), tr("Codec"), tr("Range"), tr("Size")});
    m_formats->setRootIsDecorated(false);
    m_formats->setUniformRowHeights(true);
    m_formats->setMinimumHeight(220);
    m_formats->setAlternatingRowColors(false);
    m_formats->header()->setStretchLastSection(false);
    m_formats->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 6; ++c) {
        m_formats->header()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    }
    connect(m_formats, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (column != 0 || item->checkState(0) != Qt::Checked) {
            updatePreview();
            return;
        }
        // Exclusive within a kind: a video pick clears other video picks (and
        // any progressive pick), and so on.
        const int kind = item->data(0, kFormatKindRole).toInt();
        for (int i = 0; i < m_formats->topLevelItemCount(); ++i) {
            QTreeWidgetItem* group = m_formats->topLevelItem(i);
            for (int j = 0; j < group->childCount(); ++j) {
                QTreeWidgetItem* other = group->child(j);
                if (other == item) {
                    continue;
                }
                const int otherKind = other->data(0, kFormatKindRole).toInt();
                const bool conflict = otherKind == kind || kind == 2 || otherKind == 2;
                if (conflict && other->checkState(0) == Qt::Checked) {
                    other->setCheckState(0, Qt::Unchecked);
                }
            }
        }
        updatePreview();
    });
    layout->addWidget(m_formats, 1);
    return tab;
}

QWidget* DownloadDialog::buildPlaylistSection()
{
    auto* section = new QFrame(this);
    section->setProperty("redCard", true);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(8);
    auto* header = new QHBoxLayout;
    m_entryCount = new QLabel(section);
    m_entryCount->setProperty("redMuted", true);
    header->addWidget(m_entryCount, 1);
    m_selectAll = new QPushButton(tr("Select all"), section);
    m_selectAll->setProperty("redFlat", true);
    m_selectNone = new QPushButton(tr("Select none"), section);
    m_selectNone->setProperty("redFlat", true);
    header->addWidget(m_selectAll);
    header->addWidget(m_selectNone);
    auto* rangeLabel = new QLabel(tr("Range"), section);
    rangeLabel->setProperty("redMuted", true);
    m_rangeFrom = new QSpinBox(section);
    m_rangeTo = new QSpinBox(section);
    m_rangeFrom->setMinimum(1);
    m_rangeTo->setMinimum(1);
    m_rangeFrom->setFixedWidth(72);
    m_rangeTo->setFixedWidth(72);
    header->addSpacing(8);
    header->addWidget(rangeLabel);
    header->addWidget(m_rangeFrom);
    header->addWidget(new QLabel(u"–"_s, section));
    header->addWidget(m_rangeTo);
    layout->addLayout(header);
    m_entries = new QListWidget(section);
    m_entries->setMinimumHeight(140);
    m_entries->setMaximumHeight(220);
    m_entries->setUniformItemSizes(true);
    layout->addWidget(m_entries);

    auto setAll = [this](Qt::CheckState state) {
        for (int i = 0; i < m_entries->count(); ++i) {
            m_entries->item(i)->setCheckState(state);
        }
        updateEntryCount();
    };
    connect(m_selectAll, &QPushButton::clicked, this, [setAll] { setAll(Qt::Checked); });
    connect(m_selectNone, &QPushButton::clicked, this, [setAll] { setAll(Qt::Unchecked); });
    auto applyRange = [this] {
        const int from = m_rangeFrom->value();
        const int to = m_rangeTo->value();
        for (int i = 0; i < m_entries->count(); ++i) {
            const int index = i + 1;
            m_entries->item(i)->setCheckState(index >= from && index <= to ? Qt::Checked : Qt::Unchecked);
        }
        updateEntryCount();
    };
    connect(m_rangeFrom, &QSpinBox::valueChanged, this, [applyRange](int) { applyRange(); });
    connect(m_rangeTo, &QSpinBox::valueChanged, this, [applyRange](int) { applyRange(); });
    connect(m_entries, &QListWidget::itemChanged, this, [this](QListWidgetItem*) { updateEntryCount(); });
    return section;
}

QWidget* DownloadDialog::buildFooter()
{
    auto* footer = new QWidget(this);
    auto* layout = new QVBoxLayout(footer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto* sep = new QFrame(footer);
    sep->setProperty("redSeparator", true);
    layout->addWidget(sep);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    auto* folderIcon = new QLabel(footer);
    folderIcon->setPixmap(
        icons::pixmap(u"folder"_s, Tokens::forScheme(m_theme.isDark()).muted, 18, devicePixelRatioF()));
    row->addWidget(folderIcon);
    m_folderLabel = new QLabel(footer);
    m_folderLabel->setProperty("redMuted", true);
    row->addWidget(m_folderLabel);
    auto* change = new QPushButton(tr("Change…"), footer);
    change->setProperty("redFlat", true);
    connect(change, &QPushButton::clicked, this, &DownloadDialog::chooseFolder);
    row->addWidget(change);
    row->addStretch(1);
    m_preview = new QLabel(footer);
    m_preview->setProperty("redMuted", true);
    m_preview->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(m_preview, 1);
    layout->addLayout(row);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* cancel = new QPushButton(tr("Cancel"), footer);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(cancel);
    m_download = new QPushButton(tr("Download"), footer);
    m_download->setProperty("redPrimary", true);
    m_download->setEnabled(false);
    m_download->setDefault(true);
    connect(m_download, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(m_download);
    layout->addLayout(buttons);
    return footer;
}

void DownloadDialog::applyDefaults()
{
    m_quality->setCurrentIndex(m_quality->findData(static_cast<int>(m_settings.defaultQuality())));
    m_container->setCurrentIndex(m_container->findData(static_cast<int>(m_settings.defaultContainer())));
    m_audioFormat->setCurrentIndex(
        m_audioFormat->findData(static_cast<int>(m_settings.defaultAudioFormat())));
    m_embedThumb->setChecked(m_settings.embedThumbnail());
    m_embedMeta->setChecked(m_settings.embedMetadata());
    m_audioEmbedThumb->setChecked(m_settings.embedThumbnail());
    m_audioEmbedMeta->setChecked(m_settings.embedMetadata());
    m_removeSponsors->setChecked(m_settings.removeSponsors());
    m_embedSubs->setChecked(true);
    m_folderLabel->setText(homeRelative(m_folder));
    m_folderLabel->setToolTip(m_folder);
    updateHero();
    updatePreview();
}

// ---- probing ---------------------------------------------------------------

void DownloadDialog::startProbe(Scope scope)
{
    ScopeState& state = scopeState(scope);
    if (state.probing) {
        m_probe.cancel(state.probeId);
    }
    state.probing = true;
    state.info.reset();
    state.error.clear();
    state.probeId = m_probe.probe(state.url, scope == Scope::Playlist);
    if (scope == m_scope) {
        presentScope();
    } else {
        updateScopeChooser();
    }
}

void DownloadDialog::retryProbe()
{
    startProbe(m_scope);
}

void DownloadDialog::setScope(Scope scope)
{
    if (scope == m_scope || scopeState(scope).url.isEmpty()) {
        return;
    }
    m_scope = scope;
    m_url = scopeState(scope).url;
    m_isPlaylist = scope == Scope::Playlist;
    setWindowTitle(m_isPlaylist ? tr("Download playlist") : tr("Download video"));
    presentScope();
}

/// Renders the active scope from its cached state: probing, failed, or ready.
void DownloadDialog::presentScope()
{
    const ScopeState& state = scopeState(m_scope);
    m_probing = state.probing;
    m_status->setStyleSheet({});
    m_retry->setVisible(false);
    m_formats->clear();
    m_entries->clear();
    m_playlistSection->setVisible(false);
    if (!state.info) {
        m_info = {};
        m_title->setText(m_url.toString());
        m_subtitle->clear();
        if (state.probing || state.error.isEmpty()) {
            m_status->setText(m_isPlaylist ? tr("Reading playlist…") : tr("Reading video info…"));
        } else {
            // The user can still download blind ("Download anyway"): yt-dlp
            // resolves the title itself.
            m_status->setText(state.error);
            m_status->setStyleSheet(u"color:%1;"_s.arg(Tokens::forScheme(m_theme.isDark()).danger.name()));
            m_retry->setVisible(true);
        }
        m_kinds->button(2)->setEnabled(false);
        if (m_kinds->checkedId() == 2) {
            m_kinds->button(0)->setChecked(true);
        }
        updateHero();
        updateScopeChooser();
        updatePreview();
        return;
    }
    m_info = *state.info;
    m_status->clear();
    populateFromInfo();
    if (m_info.isLive) {
        // FEATURES E14 (LATER): recording a live stream needs its own UI.
        m_status->setText(tr("This is a live stream. Red can download it once the stream has ended."));
    }
}

/// The playlist choice tells what it knows: count and title once read, or
/// why it could not be read.
void DownloadDialog::updateScopeChooser()
{
    const bool both = !m_video.url.isEmpty() && !m_playlist.url.isEmpty();
    m_scopeRow->setVisible(both);
    if (!both) {
        return;
    }
    const ScopeState& pl = m_playlist;
    m_scopeRetry->setVisible(!pl.probing && !pl.info && !pl.error.isEmpty());
    m_scopePlaylist->setEnabled(!pl.probing && pl.info.has_value());
    if (m_wantPlaylist && m_scopePlaylist->isEnabled()) {
        m_wantPlaylist = false;
        m_scopePlaylist->setChecked(true);
    }
    if (pl.probing) {
        m_scopeHint->setText(tr("Reading the playlist…"));
    } else if (pl.info) {
        const int count =
            pl.info->entryCount > 0 ? pl.info->entryCount : static_cast<int>(pl.info->entries.size());
        m_scopePlaylist->setText(tr("Whole playlist (%n videos)", nullptr, count));
        m_scopeHint->setText(pl.info->title.isEmpty() ? QString() : pl.info->title);
    } else if (!pl.error.isEmpty()) {
        m_scopeHint->setText(tr("The playlist could not be read: %1").arg(pl.error));
    } else {
        m_scopeHint->clear();
    }
    m_scopeHint->setVisible(!m_scopeHint->text().isEmpty());
}

void DownloadDialog::handleProbeFinished(quint64 id, const core::MediaInfo& info)
{
    for (Scope scope : {Scope::Video, Scope::Playlist}) {
        ScopeState& state = scopeState(scope);
        if (!state.probing || state.probeId != id) {
            continue;
        }
        state.probing = false;
        state.info = info;
        if (scope == m_scope) {
            presentScope();
        } else {
            updateScopeChooser();
        }
        return;
    }
}

void DownloadDialog::handleProbeFailed(quint64 id, const QString& error)
{
    for (Scope scope : {Scope::Video, Scope::Playlist}) {
        ScopeState& state = scopeState(scope);
        if (!state.probing || state.probeId != id) {
            continue;
        }
        state.probing = false;
        state.error = error.isEmpty() ? tr("Unknown error") : error;
        if (scope == m_scope) {
            presentScope();
        } else {
            updateScopeChooser();
        }
        return;
    }
}

void DownloadDialog::populateFromInfo()
{
    m_status->setStyleSheet({});
    m_title->setText(m_info.title);
    QStringList parts;
    if (!m_info.uploader.isEmpty()) {
        parts << m_info.uploader;
    }
    if (m_isPlaylist) {
        parts << tr("%n videos", nullptr,
                    m_info.entryCount > 0 ? m_info.entryCount : static_cast<int>(m_info.entries.size()));
    } else {
        if (!m_info.durationText.isEmpty()) {
            parts << m_info.durationText;
        }
        if (m_info.viewCount >= 0) {
            parts << tr("%1 views").arg(core::formatCount(m_info.viewCount));
        }
        if (m_info.uploadDate.size() == 8) {
            const QDate date = QDate::fromString(m_info.uploadDate, u"yyyyMMdd"_s);
            if (date.isValid()) {
                parts << QLocale().toString(date, u"d MMM yyyy"_s);
            }
        }
    }
    m_subtitle->setText(parts.join(u" · "_s));
    updateScopeChooser();

    // Subtitles.
    m_subtitles->blockSignals(true);
    m_subtitles->clear();
    m_subtitles->addItem(tr("None"), QString());
    const QStringList manual = m_info.subtitleLanguages(false);
    const QStringList all = m_info.subtitleLanguages(true);
    for (const QString& lang : all) {
        const QString label = manual.contains(lang) ? lang : tr("%1 (auto)").arg(lang);
        m_subtitles->addItem(label, lang);
    }
    for (const QString& want : m_settings.subtitleLanguages()) {
        const int idx = m_subtitles->findData(want);
        if (idx > 0) {
            m_subtitles->setCurrentIndex(idx);
            break;
        }
    }
    m_subtitles->blockSignals(false);
    m_subtitles->setEnabled(m_subtitles->count() > 1);
    m_autoSubs->setChecked(!all.isEmpty() && manual.isEmpty());

    // Quality entries unavailable for this video are greyed out.
    const QList<int> heights = m_info.availableHeights();
    if (!heights.isEmpty()) {
        auto* model = qobject_cast<QStandardItemModel*>(m_quality->model());
        for (int i = 0; i < m_quality->count(); ++i) {
            const int h =
                core::qualityHeight(static_cast<core::VideoQuality>(m_quality->itemData(i).toInt()));
            const bool available =
                h == 0 || heights.contains(h) ||
                std::any_of(heights.cbegin(), heights.cend(), [h](int have) { return have >= h; });
            if (model != nullptr) {
                if (QStandardItem* item = model->item(i); item != nullptr) {
                    item->setEnabled(available);
                }
            }
        }
    }
    m_removeSponsors->setEnabled(!m_isPlaylist || true);
    populateFormats();
    if (m_isPlaylist) {
        populateEntries();
    }
    m_playlistSection->setVisible(m_isPlaylist);
    m_kinds->button(2)->setEnabled(!m_isPlaylist && !m_info.formats.isEmpty());
    if (!m_kinds->button(2)->isEnabled() && m_kinds->checkedId() == 2) {
        m_kinds->button(0)->setChecked(true);
    }
    updateHero();
    updatePreview();
    adjustSize();
}

void DownloadDialog::populateFormats()
{
    m_formats->blockSignals(true);
    m_formats->clear();
    auto* videoGroup = new QTreeWidgetItem(m_formats, {tr("Video streams")});
    auto* audioGroup = new QTreeWidgetItem(m_formats, {tr("Audio streams")});
    auto* combinedGroup = new QTreeWidgetItem(m_formats, {tr("Combined (video + audio)")});
    for (QTreeWidgetItem* g : {videoGroup, audioGroup, combinedGroup}) {
        g->setFlags(Qt::ItemIsEnabled);
        QFont f = g->font(0);
        f.setBold(true);
        g->setFont(0, f);
        g->setExpanded(true);
    }
    QList<core::MediaFormat> formats = m_info.formats;
    std::sort(formats.begin(), formats.end(), [](const core::MediaFormat& a, const core::MediaFormat& b) {
        if (a.height != b.height) {
            return a.height > b.height;
        }
        return a.tbr > b.tbr;
    });
    // HLS duplicates of the same streams only add noise; keep them when they are all there is.
    const bool hasDirect = std::any_of(formats.cbegin(), formats.cend(), [](const core::MediaFormat& f) {
        return !f.protocol.startsWith(u"m3u8"_s);
    });
    for (const core::MediaFormat& f : formats) {
        if (hasDirect && f.protocol.startsWith(u"m3u8"_s)) {
            continue;
        }
        QTreeWidgetItem* parent =
            f.isVideoOnly() ? videoGroup : (f.isAudioOnly() ? audioGroup : combinedGroup);
        const int kind = f.isVideoOnly() ? 0 : (f.isAudioOnly() ? 1 : 2);
        QString stream = f.ext.toUpper();
        if (!f.note.isEmpty()) {
            stream += u" · "_s + f.note;
        }
        if (!f.language.isEmpty() && f.isAudioOnly()) {
            stream += u" · "_s + f.language;
        }
        const QString res = f.hasVideo() ? u"%1×%2"_s.arg(f.width).arg(f.height)
                                         : (f.abr > 0 ? u"%1 kbps"_s.arg(static_cast<int>(f.abr))
                                                      : u"%1 kbps"_s.arg(static_cast<int>(f.tbr)));
        auto* item = new QTreeWidgetItem(
            parent,
            {stream, res, f.fps > 0 ? QString::number(static_cast<int>(f.fps)) : QString(), f.codecLabel(),
             f.hasVideo() ? f.dynamicRange : QString(), core::formatBytes(f.filesize)});
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        item->setCheckState(0, Qt::Unchecked);
        item->setData(0, kFormatIdRole, f.id);
        item->setData(0, kFormatKindRole, kind);
        item->setToolTip(0, u"format id %1 · %2"_s.arg(f.id, f.protocol));
    }
    for (QTreeWidgetItem* g : {videoGroup, audioGroup, combinedGroup}) {
        if (g->childCount() == 0) {
            delete g;
        }
    }
    m_formats->blockSignals(false);
}

void DownloadDialog::populateEntries()
{
    m_entries->blockSignals(true);
    m_entries->clear();
    int index = 0;
    for (const core::MediaEntry& e : m_info.entries) {
        ++index;
        QString text = u"%1.  %2"_s.arg(index).arg(e.title.isEmpty() ? e.id : e.title);
        if (e.duration > 0) {
            text += u"   (%1)"_s.arg(core::formatDuration(e.duration));
        }
        auto* item = new QListWidgetItem(text, m_entries);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        item->setData(kEntryIndexRole, index);
    }
    m_entries->blockSignals(false);
    const int count = std::max(1, index);
    m_rangeFrom->blockSignals(true);
    m_rangeTo->blockSignals(true);
    m_rangeFrom->setMaximum(count);
    m_rangeTo->setMaximum(count);
    m_rangeFrom->setValue(1);
    m_rangeTo->setValue(count);
    m_rangeFrom->blockSignals(false);
    m_rangeTo->blockSignals(false);
    updateEntryCount();
}

// ---- presentation ----------------------------------------------------------

void DownloadDialog::updateHero()
{
    QString thumbUrl = m_info.thumbnail;
    if (thumbUrl.isEmpty()) {
        const core::YouTubeUrlInfo info = core::classifyYouTubeUrl(m_url);
        if (!info.videoId.isEmpty()) {
            thumbUrl = core::thumbnailUrl(info.videoId).toString();
        }
    }
    const QPixmap pm = m_thumbnails.get(thumbUrl);
    if (pm.isNull()) {
        m_thumb->setPixmap(icons::pixmap(m_isPlaylist ? u"playlist"_s : u"video"_s,
                                         Tokens::forScheme(m_theme.isDark()).muted, 32, devicePixelRatioF()));
        return;
    }
    const qreal dpr = devicePixelRatioF();
    QPixmap scaled =
        pm.scaled(QSize(kThumbW, kThumbH) * dpr, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QRect crop((scaled.width() - static_cast<int>(kThumbW * dpr)) / 2,
                     (scaled.height() - static_cast<int>(kThumbH * dpr)) / 2, static_cast<int>(kThumbW * dpr),
                     static_cast<int>(kThumbH * dpr));
    scaled = scaled.copy(crop);
    scaled.setDevicePixelRatio(dpr);
    m_thumb->setPixmap(scaled);
}

void DownloadDialog::updatePreview()
{
    const core::DownloadOptions opts = options();
    const QString id = m_info.id.isEmpty() ? core::classifyYouTubeUrl(m_url).videoId : m_info.id;
    QString preview =
        core::previewFileName(opts, m_info.title.isEmpty() ? tr("Video") : m_info.title, id, m_info.uploader);
    if (m_isPlaylist) {
        preview = tr("%1/…").arg(m_info.title.isEmpty() ? tr("Playlist") : m_info.title);
    }
    m_preview->setText(m_preview->fontMetrics().elidedText(preview, Qt::ElideMiddle, 300));
    m_preview->setToolTip(preview);
    const core::DownloadOptions::Kind current = kind();
    const ScopeState& state = scopeState(m_scope);
    const bool failed = !state.probing && !state.info && !state.error.isEmpty();
    m_download->setText(failed                                          ? tr("Download anyway")
                        : current == core::DownloadOptions::Kind::Audio ? tr("Download audio")
                        : current == core::DownloadOptions::Kind::Video
                            ? (m_isPlaylist ? tr("Download videos") : tr("Download video"))
                            : tr("Download"));
    if (current == core::DownloadOptions::Kind::Custom) {
        const bool picked = !opts.customVideoFormat.isEmpty() || !opts.customAudioFormat.isEmpty();
        m_download->setEnabled(picked && !m_probing);
    } else {
        m_download->setEnabled(!m_probing && !(state.info && state.info->isLive));
    }
}

void DownloadDialog::updateEntryCount()
{
    int selected = 0;
    for (int i = 0; i < m_entries->count(); ++i) {
        if (m_entries->item(i)->checkState() == Qt::Checked) {
            ++selected;
        }
    }
    m_entryCount->setText(tr("%1 of %2 selected").arg(selected).arg(m_entries->count()));
    m_download->setEnabled(selected > 0 && !m_probing);
}

void DownloadDialog::chooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Save to"), m_folder);
    if (dir.isEmpty()) {
        return;
    }
    m_folder = dir;
    m_folderLabel->setText(homeRelative(dir));
    m_folderLabel->setToolTip(dir);
    m_settings.setDownloadDirectory(dir);
}

void DownloadDialog::selectKind(core::DownloadOptions::Kind kind)
{
    const int id = kind == core::DownloadOptions::Kind::Audio    ? 1
                   : kind == core::DownloadOptions::Kind::Custom ? 2
                                                                 : 0;
    if (QAbstractButton* button = m_kinds->button(id); button && button->isEnabled()) {
        button->setChecked(true);
    }
}

void DownloadDialog::selectWholePlaylist()
{
    m_wantPlaylist = true;
    updateScopeChooser();
}

void DownloadDialog::selectPlaylistRange(int from, int to)
{
    if (!m_isPlaylist || m_entries->count() == 0) {
        return;
    }
    m_rangeFrom->setValue(from);
    m_rangeTo->setValue(to);
}

core::DownloadOptions::Kind DownloadDialog::kind() const
{
    switch (m_kinds->checkedId()) {
    case 1:
        return core::DownloadOptions::Kind::Audio;
    case 2:
        return core::DownloadOptions::Kind::Custom;
    default:
        return core::DownloadOptions::Kind::Video;
    }
}

// ---- result ----------------------------------------------------------------

QString DownloadDialog::playlistItemsSpec() const
{
    QStringList picked;
    bool all = true;
    for (int i = 0; i < m_entries->count(); ++i) {
        if (m_entries->item(i)->checkState() == Qt::Checked) {
            picked << QString::number(m_entries->item(i)->data(kEntryIndexRole).toInt());
        } else {
            all = false;
        }
    }
    return all ? QString() : picked.join(u',');
}

core::DownloadOptions DownloadDialog::options() const
{
    core::DownloadOptions o;
    o.kind = kind();
    o.quality = static_cast<core::VideoQuality>(m_quality->currentData().toInt());
    o.container = static_cast<core::Container>(m_container->currentData().toInt());
    o.audioFormat = static_cast<core::AudioFormat>(m_audioFormat->currentData().toInt());
    o.audioBitrateKbps = m_audioQuality->currentData().toInt();
    if (o.kind == core::DownloadOptions::Kind::Custom) {
        for (int i = 0; i < m_formats->topLevelItemCount(); ++i) {
            QTreeWidgetItem* group = m_formats->topLevelItem(i);
            for (int j = 0; j < group->childCount(); ++j) {
                QTreeWidgetItem* item = group->child(j);
                if (item->checkState(0) != Qt::Checked) {
                    continue;
                }
                const int kind = item->data(0, kFormatKindRole).toInt();
                if (kind == 1) {
                    o.customAudioFormat = item->data(0, kFormatIdRole).toString();
                } else {
                    o.customVideoFormat = item->data(0, kFormatIdRole).toString();
                }
            }
        }
    }
    const QString lang = m_subtitles->currentData().toString();
    if (!lang.isEmpty() && o.kind != core::DownloadOptions::Kind::Audio) {
        o.subtitleLanguages = {lang};
        o.includeAutoSubtitles = m_autoSubs->isChecked();
        o.embedSubtitles = m_embedSubs->isChecked();
    }
    if (o.kind == core::DownloadOptions::Kind::Audio) {
        o.embedThumbnail = m_audioEmbedThumb->isChecked();
        o.embedMetadata = m_audioEmbedMeta->isChecked();
    } else {
        o.embedThumbnail = m_embedThumb->isChecked();
        o.embedMetadata = m_embedMeta->isChecked();
    }
    o.removeSponsors = m_removeSponsors->isChecked();
    o.sponsorCategories = core::sponsorRemoveCategories(m_settings.sponsorActions());
    o.outputDirectory = m_folder;
    o.filenamePattern = m_settings.filenamePattern();
    o.isPlaylist = m_isPlaylist;
    o.playlistItems = m_isPlaylist ? playlistItemsSpec() : QString();
    o.speedLimitKbps = m_settings.speedLimitKbps();
    return o;
}

core::DownloadJob DownloadDialog::job() const
{
    core::DownloadJob j;
    j.url = m_url.toString();
    j.options = options();
    j.title = m_info.title;
    j.uploader = m_info.uploader;
    j.thumbnail = m_info.thumbnail;
    j.duration = m_info.duration;
    j.videoId = m_info.isPlaylist() ? QString() : m_info.id;
    if (j.videoId.isEmpty() && !m_isPlaylist) {
        j.videoId = core::classifyYouTubeUrl(m_url).videoId;
    }
    if (j.thumbnail.isEmpty() && !j.videoId.isEmpty()) {
        j.thumbnail = core::thumbnailUrl(j.videoId).toString();
    }
    if (m_isPlaylist) {
        int selected = 0;
        for (int i = 0; i < m_entries->count(); ++i) {
            selected += m_entries->item(i)->checkState() == Qt::Checked ? 1 : 0;
        }
        j.itemCount = selected > 0 ? selected : m_info.entryCount;
    }
    return j;
}

} // namespace red::ui
