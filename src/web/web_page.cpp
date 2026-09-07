#include "web/web_page.h"

#include "core/navigation_policy.h"
#include "platform/file_manager.h"
#include "web/logging.h"
#include "web/popup_window.h"
#include "web/web_profile.h"

using namespace Qt::StringLiterals;

namespace red::web {

WebPage::WebPage(WebProfile& profile, QObject* parent)
    : QWebEnginePage(&profile, parent)
    , m_profile(profile)
{}

bool WebPage::acceptNavigationRequest(const QUrl& url, NavigationType type, bool isMainFrame)
{
    if (isMainFrame && type == NavigationTypeLinkClicked) {
        const QUrl target = core::unwrapRedirect(url);
        if (core::shouldOpenExternally(target)) {
            qCInfo(lcWeb) << "link → system browser:" << target;
            platform::openUrl(target.toString());
            return false;
        }
    }
    return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
}

QWebEnginePage* WebPage::createWindow(WebWindowType type)
{
    Q_UNUSED(type)
    // The window decides on its first navigation whether it stays (Google
    // sign-in) or hands the URL to the browser and closes.
    auto* window = new PopupWindow(m_profile, m_host);
    window->show();
    Q_EMIT popupOpened(window);
    return window->page();
}

void WebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString& message,
                                       int lineNumber, const QString& sourceId)
{
    switch (level) {
    case InfoMessageLevel:
        qCDebug(lcWebJs).noquote() << sourceId << lineNumber << message;
        break;
    case WarningMessageLevel:
        // YouTube preloads generate_204 pings to warm up its media servers and
        // Chromium complains they were "not used"; Chrome logs the same.
        if (message.contains(u"was preloaded using link preload"_s)) {
            break;
        }
        qCDebug(lcWebJs).noquote() << "warning:" << sourceId << lineNumber << message;
        break;
    case ErrorMessageLevel:
        qCWarning(lcWebJs).noquote() << sourceId << lineNumber << message;
        break;
    }
}

void WebPage::javaScriptAlert(const QUrl& securityOrigin, const QString& msg)
{
    if (m_dialogs.alert) {
        m_dialogs.alert(securityOrigin, msg);
        return;
    }
    QWebEnginePage::javaScriptAlert(securityOrigin, msg);
}

bool WebPage::javaScriptConfirm(const QUrl& securityOrigin, const QString& msg)
{
    if (m_dialogs.confirm) {
        return m_dialogs.confirm(securityOrigin, msg);
    }
    return QWebEnginePage::javaScriptConfirm(securityOrigin, msg);
}

bool WebPage::javaScriptPrompt(const QUrl& securityOrigin, const QString& msg, const QString& defaultValue,
                               QString* result)
{
    if (m_dialogs.prompt) {
        return m_dialogs.prompt(securityOrigin, msg, defaultValue, result);
    }
    return QWebEnginePage::javaScriptPrompt(securityOrigin, msg, defaultValue, result);
}

} // namespace red::web
