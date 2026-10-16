#pragma once

#include <QFrame>

class QLabel;
class QMenu;

namespace red::core {
class ThemeService;
}

namespace red::ui {

class Actions;
class RailButton;

/// The brand rail on the left of the window (DESIGN.md §2): the mode switch
/// (YouTube, YouTube Music, TV), navigation, the Download call-to-action, the
/// downloads badge, mini player, settings, the overflow menu and the labels
/// toggle. Icons only (52 px) or icons with labels (200 px). Pure
/// presentation over Actions.
class SideRail : public QFrame
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SideRail)

public:
    SideRail(Actions& actions, core::ThemeService& theme, QWidget* parent = nullptr);
    ~SideRail() override = default;

    /// Icons with labels (true) or icons only.
    void setExpanded(bool expanded);
    [[nodiscard]] bool isExpanded() const { return m_expanded; }
    /// Turns the Download button's glyph red when the page is downloadable.
    void setDownloadable(bool downloadable);
    /// Active-download count badge on the Downloads button (0 hides it).
    void setActiveDownloads(int count);
    /// Reload shows a stop glyph while the page loads.
    void setLoading(bool loading);
    /// Why the mini player is unavailable, under its label; empty when it is.
    void setMiniPlayerHint(const QString& hint);
    void setMenu(QMenu* menu);
    /// Drops the overflow menu open (debug grabs).
    void showMenu();
    /// F6 arrived here: keyboard focus on the first button that can take it.
    void focusFirst();
    [[nodiscard]] bool hasFocusInside() const;

Q_SIGNALS:
    /// The labels toggle at the bottom was clicked; the owner stores the choice.
    void expandRequested(bool expanded);
    /// Esc on a rail button: the owner hands focus back to the page.
    void leaveRequested();

private:
    void setupUi();
    void applyIcons();
    void applyWidths();
    void placeBadge();
    RailButton* makeButton(QAction* action, const QString& icon, int side = 40);
    QWidget* makeModeSwitch();
    [[nodiscard]] QList<RailButton*> buttons() const;
    /// Top to bottom, the mode switch included: the keyboard's order.
    [[nodiscard]] QList<RailButton*> keyboardOrder() const;
    bool eventFilter(QObject* watched, QEvent* event) override;

    Actions& m_actions;
    core::ThemeService& m_theme;
    RailButton* m_logo = nullptr;
    RailButton* m_youtube = nullptr;
    RailButton* m_music = nullptr;
    RailButton* m_tv = nullptr;
    RailButton* m_back = nullptr;
    RailButton* m_forward = nullptr;
    RailButton* m_reload = nullptr;
    RailButton* m_download = nullptr;
    RailButton* m_downloads = nullptr;
    RailButton* m_miniPlayer = nullptr;
    RailButton* m_settings = nullptr;
    RailButton* m_menu = nullptr;
    RailButton* m_labels = nullptr;
    QFrame* m_modeSwitch = nullptr;
    QList<QFrame*> m_separators;
    QLabel* m_badge = nullptr;
    bool m_expanded = false;
    bool m_downloadable = false;
    bool m_loading = false;
};

} // namespace red::ui
