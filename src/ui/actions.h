#pragma once

#include <QList>
#include <QObject>

class QAction;
class QWidget;

namespace red::ui {

/// All user-triggerable actions with their shortcuts, created once and shared
/// by the window, the rail, the tray menu and the shortcuts sheet. The owner
/// connects behaviour; this class only declares.
class Actions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Actions)

public:
    explicit Actions(QWidget* owner);
    ~Actions() override = default;

    QAction* home = nullptr;
    QAction* back = nullptr;
    QAction* forward = nullptr;
    QAction* reload = nullptr;
    QAction* download = nullptr;
    QAction* downloadUrl = nullptr;
    QAction* downloads = nullptr;
    QAction* copyLink = nullptr;
    QAction* miniPlayer = nullptr;
    QAction* tvMode = nullptr;
    QAction* musicMode = nullptr;
    QAction* playPause = nullptr;
    QAction* next = nullptr;
    QAction* previous = nullptr;
    QAction* zoomIn = nullptr;
    QAction* zoomOut = nullptr;
    QAction* zoomReset = nullptr;
    QAction* fullScreen = nullptr;
    QAction* showHide = nullptr;
    QAction* settings = nullptr;
    QAction* shortcuts = nullptr;
    QAction* onlineGuide = nullptr;
    QAction* openLogFolder = nullptr;
    QAction* about = nullptr;
    QAction* account = nullptr;
    QAction* quit = nullptr;

    /// Every action with a shortcut, in sheet order.
    [[nodiscard]] QList<QAction*> all() const;
};

} // namespace red::ui
