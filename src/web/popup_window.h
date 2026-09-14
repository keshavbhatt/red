#pragma once

#include "web/script_dialogs.h"

#include <QWidget>

class QShortcut;
class QWebEnginePage;
class QWebEngineView;

namespace red::web {

class FullScreenHint;
class WebProfile;

/// Hosts window.open() targets that stay in-app — Google sign-in pop-ups
/// (FEATURES S11). External targets are sent to the browser and the window
/// closes itself. Esc / close button always work.
class PopupWindow : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PopupWindow)

public:
    explicit PopupWindow(WebProfile& profile, QWidget* parent = nullptr);
    ~PopupWindow() override = default;

    [[nodiscard]] QWebEnginePage* page() const;
    /// The same sheets the main page uses, parented to this window.
    void setScriptDialogs(ScriptDialogs dialogs) { m_dialogs = std::move(dialogs); }

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    class Page;
    void exitFullScreen();
    QWebEngineView* m_view = nullptr;
    QShortcut* m_exitFullScreen = nullptr;
    Qt::WindowStates m_stateBeforeFullScreen = Qt::WindowNoState;
    FullScreenHint* m_fullScreenHint = nullptr;
    ScriptDialogs m_dialogs;
};

} // namespace red::web
