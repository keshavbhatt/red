#pragma once

#include <QString>
#include <QUrl>

#include <functional>

class QWidget;

namespace red::web {

/// The UI's replacements for the engine's stock alert / confirm / prompt
/// boxes (the "leave this page?" guard included), shared by the main page and
/// every pop-up window it opens. `window` is the window the page lives in,
/// the parent for the sheet. Each runs modally; `prompt` returns false when
/// cancelled. Unset ones fall back to Qt's.
struct ScriptDialogs
{
    std::function<void(QWidget* window, const QUrl& origin, const QString& message)> alert;
    std::function<bool(QWidget* window, const QUrl& origin, const QString& message)> confirm;
    std::function<bool(QWidget* window, const QUrl& origin, const QString& message,
                       const QString& defaultValue, QString* result)>
        prompt;
};

} // namespace red::web
