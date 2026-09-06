#pragma once

#include "services/engine_manager.h"
#include "ui/download_card_delegate.h"

#include <QFrame>

class QLabel;
class QListView;
class QMenu;
class QPushButton;
class QStackedWidget;
class QToolButton;

namespace red::core {
class DownloadQueue;
class Settings;
class ThemeService;
} // namespace red::core

namespace red::ui {

class ThumbnailCache;

/// The slide-in downloads panel (DESIGN.md §2): header with the engine chip,
/// the card list, an empty state, and the "Save to" footer. Presentation only;
/// DownloadsHub wires the actions.
class DownloadsPanel : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadsPanel)

public:
    DownloadsPanel(core::DownloadQueue& queue, core::Settings& settings, core::ThemeService& theme,
                   ThumbnailCache& thumbnails, QWidget* parent = nullptr);
    ~DownloadsPanel() override = default;

    void setEngineStatus(const services::EngineManager::Status& status);
    static constexpr int kWidth = 380;

Q_SIGNALS:
    void newDownloadRequested();
    void cardAction(quint64 jobId, red::ui::DownloadCardDelegate::Action action);
    void openFolderRequested();
    void changeFolderRequested();
    void engineRequested(); ///< the chip was clicked: set up / update / show settings
    void closeRequested();

private:
    void setupUi();
    void applyIcons();
    void updateEmptyState();
    void updateFooter();

    core::DownloadQueue& m_queue;
    core::Settings& m_settings;
    core::ThemeService& m_theme;
    ThumbnailCache& m_thumbnails;
    DownloadCardDelegate* m_delegate = nullptr;
    QListView* m_list = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_title = nullptr;
    QToolButton* m_engineChip = nullptr;
    QToolButton* m_newButton = nullptr;
    QToolButton* m_moreButton = nullptr;
    QToolButton* m_closeButton = nullptr;
    QMenu* m_moreMenu = nullptr;
    QLabel* m_emptyIcon = nullptr;
    QLabel* m_emptyTitle = nullptr;
    QLabel* m_emptyBody = nullptr;
    QPushButton* m_emptyButton = nullptr;
    QLabel* m_folderLabel = nullptr;
    QToolButton* m_folderButton = nullptr;
    QToolButton* m_changeButton = nullptr;
    services::EngineManager::Status m_engine;
};

} // namespace red::ui
