#pragma once

#include <QFrame>

class QToolButton;
class QLabel;

namespace red::core {
class ThemeService;
}

namespace red::ui {

class Actions;

/// The 48 px brand rail on the left of the window (DESIGN.md §2): navigation,
/// the Download call-to-action, the downloads badge, mini player, TV mode,
/// settings and the overflow menu. Pure presentation over Actions.
class SideRail : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SideRail)

public:
    SideRail(Actions& actions, core::ThemeService& theme, QWidget* parent = nullptr);
    ~SideRail() override = default;

    /// Highlights the Download button as a red pill when the page is downloadable.
    void setDownloadable(bool downloadable);
    /// Active-download count badge on the Downloads button (0 hides it).
    void setActiveDownloads(int count);
    /// Reload shows a stop glyph while the page loads.
    void setLoading(bool loading);
    void setMenu(QMenu* menu);
    /// Drops the overflow menu open (debug grabs).
    void showMenu();

private:
    void setupUi();
    void applyIcons();
    QToolButton* makeButton(QAction* action, const QString& icon);

    Actions& m_actions;
    core::ThemeService& m_theme;
    QToolButton* m_logo = nullptr;
    QToolButton* m_back = nullptr;
    QToolButton* m_forward = nullptr;
    QToolButton* m_reload = nullptr;
    QToolButton* m_download = nullptr;
    QToolButton* m_downloads = nullptr;
    QToolButton* m_miniPlayer = nullptr;
    QToolButton* m_tv = nullptr;
    QToolButton* m_settings = nullptr;
    QToolButton* m_menu = nullptr;
    QLabel* m_badge = nullptr;
    bool m_downloadable = false;
    bool m_loading = false;
};

} // namespace red::ui
