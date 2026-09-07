#pragma once

#include "core/downloads/download_job.h"
#include "core/downloads/media_info.h"

#include <QDialog>
#include <QUrl>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTabWidget;
class QTreeWidget;

namespace red::core {
class Settings;
class ThemeService;
} // namespace red::core
namespace red::services {
class MediaProbe;
}

namespace red::ui {

class ThumbnailCache;

/// "Save video" (DESIGN.md §3): probes the URL, shows the hero, the Video /
/// Audio / Advanced options and, for playlists, the entry picker. Produces a
/// DownloadJob for the queue. Defaults come from Settings; nothing is written
/// back except the folder when changed.
class DownloadDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadDialog)

public:
    DownloadDialog(const QUrl& url, core::Settings& settings, core::ThemeService& theme,
                   services::MediaProbe& probe, ThumbnailCache& thumbnails, QWidget* parent = nullptr);
    ~DownloadDialog() override;

    /// The job to enqueue after accept().
    [[nodiscard]] core::DownloadJob job() const;
    /// Switches the Video / Audio only / Advanced choice.
    void selectKind(core::DownloadOptions::Kind kind);
    [[nodiscard]] core::DownloadOptions::Kind kind() const;

private:
    void setupUi();
    QWidget* buildHero();
    QWidget* buildKindChooser();
    QWidget* buildVideoTab();
    QWidget* buildAudioTab();
    QWidget* buildAdvancedTab();
    QWidget* buildPlaylistSection();
    QWidget* buildFooter();
    void applyDefaults();
    void startProbe();
    void handleProbeFinished(quint64 id, const core::MediaInfo& info);
    void handleProbeFailed(quint64 id, const QString& error);
    void populateFromInfo();
    void populateFormats();
    void populateEntries();
    void updateHero();
    void updatePreview();
    void updateEntryCount();
    void chooseFolder();
    [[nodiscard]] core::DownloadOptions options() const;
    [[nodiscard]] QString playlistItemsSpec() const;

    QUrl m_url;
    core::Settings& m_settings;
    core::ThemeService& m_theme;
    services::MediaProbe& m_probe;
    ThumbnailCache& m_thumbnails;
    core::MediaInfo m_info;
    quint64 m_probeId = 0;
    bool m_probing = false;
    bool m_isPlaylist = false;
    QString m_folder;

    QLabel* m_thumb = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_retry = nullptr;
    QCheckBox* m_wholePlaylist = nullptr;
    QButtonGroup* m_kinds = nullptr;
    QStackedWidget* m_pages = nullptr;
    QComboBox* m_quality = nullptr;
    QComboBox* m_container = nullptr;
    QComboBox* m_subtitles = nullptr;
    QCheckBox* m_autoSubs = nullptr;
    QCheckBox* m_embedSubs = nullptr;
    QCheckBox* m_embedThumb = nullptr;
    QCheckBox* m_embedMeta = nullptr;
    QCheckBox* m_removeSponsors = nullptr;
    QComboBox* m_audioFormat = nullptr;
    QComboBox* m_audioQuality = nullptr;
    QCheckBox* m_audioEmbedThumb = nullptr;
    QCheckBox* m_audioEmbedMeta = nullptr;
    QTreeWidget* m_formats = nullptr;
    QLabel* m_formatsHint = nullptr;
    QWidget* m_playlistSection = nullptr;
    QListWidget* m_entries = nullptr;
    QLabel* m_entryCount = nullptr;
    QPushButton* m_selectAll = nullptr;
    QPushButton* m_selectNone = nullptr;
    QSpinBox* m_rangeFrom = nullptr;
    QSpinBox* m_rangeTo = nullptr;
    QLabel* m_folderLabel = nullptr;
    QLabel* m_preview = nullptr;
    QPushButton* m_download = nullptr;
};

} // namespace red::ui
