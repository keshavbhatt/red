#include "ui/actions.h"

#include <QAction>
#include <QKeySequence>
#include <QWidget>

namespace red::ui {

namespace {

QAction* make(QWidget* owner, const QString& text, const QList<QKeySequence>& shortcuts = {})
{
    auto* action = new QAction(text, owner);
    if (!shortcuts.isEmpty()) {
        // A standard key can resolve to the same keys as an explicit one (F11
        // on most desktops); keep each binding once, in the given order.
        QList<QKeySequence> unique;
        for (const QKeySequence& sequence : shortcuts) {
            if (!sequence.isEmpty() && !unique.contains(sequence)) {
                unique << sequence;
            }
        }
        action->setShortcuts(unique);
        action->setShortcutContext(Qt::WindowShortcut);
    }
    // Registered on the window so shortcuts work while the web view has focus.
    owner->addAction(action);
    return action;
}

} // namespace

Actions::Actions(QWidget* owner)
    : QObject(owner)
{
    home = make(owner, tr("Home"), {QKeySequence(Qt::CTRL | Qt::Key_H)});
    back = make(owner, tr("Back"), {QKeySequence::Back});
    forward = make(owner, tr("Forward"), {QKeySequence::Forward});
    reload = make(owner, tr("Reload"), {QKeySequence::Refresh, QKeySequence(Qt::CTRL | Qt::Key_R)});
    download = make(owner, tr("Download this…"), {QKeySequence(Qt::CTRL | Qt::Key_D)});
    downloadUrl = make(owner, tr("Download from link…"), {QKeySequence(Qt::CTRL | Qt::Key_N)});
    downloads = make(owner, tr("Downloads"), {QKeySequence(Qt::CTRL | Qt::Key_J)});
    copyLink = make(owner, tr("Copy video link"), {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C)});
    miniPlayer = make(owner, tr("Mini player"), {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M)});
    tvMode = make(owner, tr("TV mode"), {QKeySequence(Qt::CTRL | Qt::Key_T)});
    musicMode = make(owner, tr("Music mode"), {QKeySequence(Qt::CTRL | Qt::Key_M)});
    playPause = make(owner, tr("Play / Pause"),
                     {QKeySequence(Qt::Key_MediaTogglePlayPause), QKeySequence(Qt::Key_MediaPlay)});
    next = make(owner, tr("Next video"), {QKeySequence(Qt::Key_MediaNext)});
    previous = make(owner, tr("Previous video"), {QKeySequence(Qt::Key_MediaPrevious)});
    zoomIn = make(owner, tr("Zoom in"), {QKeySequence::ZoomIn, QKeySequence(Qt::CTRL | Qt::Key_Equal)});
    zoomOut = make(owner, tr("Zoom out"), {QKeySequence::ZoomOut});
    zoomReset = make(owner, tr("Reset zoom"), {QKeySequence(Qt::CTRL | Qt::Key_0)});
    // F11 everywhere, plus the desktop's own binding: the KDE platform theme
    // (the Flatpak and snap runtimes) maps QKeySequence::FullScreen to
    // Ctrl+Shift+F, and the sheet, guide and store text all say F11.
    fullScreen = make(owner, tr("Full screen"), {QKeySequence(Qt::Key_F11), QKeySequence::FullScreen});
    showHide = make(owner, tr("Hide window"), {QKeySequence(Qt::CTRL | Qt::Key_W)});
    settings = make(owner, tr("Settings…"), {QKeySequence(Qt::CTRL | Qt::Key_Comma)});
    shortcuts = make(owner, tr("Keyboard shortcuts"), {QKeySequence(Qt::Key_F1)});
    onlineGuide = make(owner, tr("Online guide"));
    openLogFolder = make(owner, tr("Open log folder"));
    about = make(owner, tr("About Red"));
    account = make(owner, tr("Account && licence…"));
    quit = make(owner, tr("Quit"), {QKeySequence::Quit});

    miniPlayer->setCheckable(true);
    tvMode->setCheckable(true);
    musicMode->setCheckable(true);
    downloads->setCheckable(true);
    fullScreen->setCheckable(true);
    quit->setMenuRole(QAction::QuitRole);
    about->setMenuRole(QAction::AboutRole);
    settings->setMenuRole(QAction::PreferencesRole);
}

QList<QAction*> Actions::all() const
{
    return {home,        back,       forward,   reload,     download,  downloadUrl, downloads,
            copyLink,    miniPlayer, tvMode,    musicMode,  playPause, next,        previous,
            zoomIn,      zoomOut,    zoomReset, fullScreen, showHide,  settings,    shortcuts,
            onlineGuide, account,    about,     quit};
}

} // namespace red::ui
