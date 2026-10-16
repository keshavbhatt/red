#pragma once

#include "services/engine_manager.h"
#include "ui/download_card_delegate.h"

#include <QFrame>
#include <QHash>

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
    /// F6 arrived here: the card list, or "download from a link" when it is empty.
    void focusFirst();
    [[nodiscard]] bool hasFocusInside() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void newDownloadRequested();
    void cardAction(quint64 jobId, red::ui::DownloadCardDelegate::Action action);
    void openFolderRequested();
    void changeFolderRequested();
    void engineRequested(); ///< the chip was clicked: set up / update / show settings
    void closeRequested();
    /// Esc inside the panel: the owner hands focus back to the page.
    void leaveRequested();

private:
    void setupUi();
    void applyIcons();
    void updateEmptyState();
    void updateFooter();
    /// Every action a card offers, for the right mouse button and the Menu key.
    void showCardMenu(const QModelIndex& index, const QPoint& globalPos);
    /// Enter: open a finished file, retry a failed one, resume a paused one.
    void runPrimaryAction(const QModelIndex& index);
    void emitAction(const QModelIndex& index, DownloadCardDelegate::Action action);

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
    QHash<quint64, bool> m_showsError; ///< which cards are tall (a failed card's reason)
    bool m_keyboard = false;           ///< the list is being used from the keyboard
};

} // namespace red::ui
