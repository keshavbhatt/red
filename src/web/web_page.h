#pragma once

#include "web/script_dialogs.h"

#include <QWebEnginePage>
#include <QWidget>

namespace red::web {

class WebProfile;

/// The page hosting YouTube. Routes the page console to the `red.web.js`
/// logging category, sends link clicks that leave YouTube/Google to the
/// system browser (FEATURES S11) and hosts window.open() targets (sign-in)
/// in PopupWindows.
class WebPage : public QWebEnginePage
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WebPage)

public:
    explicit WebPage(WebProfile& profile, QObject* parent = nullptr);
    ~WebPage() override = default;

    /// Parent for pop-up windows and dialogs (the main window's view).
    void setHostWidget(QWidget* host) { m_host = host; }

    /// Replacements for the stock script dialogs; pop-up windows inherit them.
    void setScriptDialogs(ScriptDialogs dialogs) { m_dialogs = std::move(dialogs); }

Q_SIGNALS:
    void popupOpened(QWidget* window);

protected:
    bool acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame) override;
    QWebEnginePage* createWindow(WebWindowType type) override;
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message, int lineNumber,
                                  const QString& sourceId) override;
    void javaScriptAlert(const QUrl& securityOrigin, const QString& msg) override;
    bool javaScriptConfirm(const QUrl& securityOrigin, const QString& msg) override;
    bool javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue,
                          QString* result) override;

private:
    [[nodiscard]] QWidget* hostWindow() const { return m_host != nullptr ? m_host->window() : nullptr; }

    WebProfile& m_profile;
    QWidget* m_host = nullptr;
    ScriptDialogs m_dialogs;
};

} // namespace red::web
