#pragma once

#include <QWebEnginePermission>

#include <functional>

class QWidget;

namespace red::ui {

/// Non-blocking Allow / Don't allow question for a web permission (FEATURES
/// M1). `answer(allow, remember)` is invoked exactly once: a button press is
/// remembered, dismissing the sheet (Esc, close) denies this time only.
void askPermission(QWidget* parent, const QWebEnginePermission& permission,
                   std::function<void(bool, bool)> answer);

} // namespace red::ui
