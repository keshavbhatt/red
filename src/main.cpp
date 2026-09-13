#include "app/application.h"
#include "app/single_instance.h"
#include "app/version.h"
#include "core/graphics_fallback.h"
#include "core/log_sink.h"
#include "core/settings/settings.h"
#include "core/settings/settings_keys.h"
#include "platform/crash_handler.h"
#include "platform/gpu_stderr_watch.h"
#include "ui/main_window.h"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QMenu>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

using namespace Qt::StringLiterals;

#ifdef Q_OS_UNIX
#include <QSocketNotifier>

#include <csignal>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <cmath>

namespace {

// Interface scale must reach Qt through QT_SCALE_FACTOR, which is read once
// during QApplication construction — so we peek at the stored value first.
void applyInterfaceScaleEnv()
{
    if (qEnvironmentVariableIsSet("QT_SCALE_FACTOR")) {
        return;
    }
    const QSettings store(QString::fromLatin1(red::app::version::kOrganizationName),
                          QString::fromLatin1(red::app::version::kApplicationName));
    const double stored = store.value(red::core::keys::kInterfaceScale, 1.0).toDouble();
    const double scale =
        std::clamp(stored, red::core::Settings::kMinInterfaceScale, red::core::Settings::kMaxInterfaceScale);
    if (std::abs(scale - 1.0) > 0.001) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(scale, 'g', 4));
    }
}

std::atomic<bool> g_graphicsFailed{false};
std::atomic<bool> g_gpuBlackScreen{false};
QtMessageHandler g_previousHandler = nullptr;

void graphicsWatchHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (red::core::isGraphicsInitFailure(message)) {
        g_graphicsFailed.store(true);
    }
    if (red::core::isGbmVulkanFallback(message)) {
        g_gpuBlackScreen.store(true);
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, message);
    }
}

#ifdef Q_OS_UNIX
int g_termPipe[2] = {-1, -1};

void writeTermSignal(int /*signum*/)
{
    const char byte = 1;
    const ssize_t ignored = ::write(g_termPipe[1], &byte, 1);
    static_cast<void>(ignored);
}

int installGracefulTermination()
{
    if (::pipe(g_termPipe) != 0) {
        return -1;
    }
    struct sigaction action{};
    action.sa_handler = writeTermSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);
    return g_termPipe[0];
}
#endif

void relaunch(red::app::Application& app, const char* envKey)
{
    if (envKey != nullptr) {
        qputenv(envKey, "1");
    }
    const QStringList args = QCoreApplication::arguments().mid(1);
    app.singleInstance().release();
    if (QProcess::startDetached(QCoreApplication::applicationFilePath(), args)) {
        QCoreApplication::quit();
    } else {
        qWarning("relaunch failed to start; staying on the current configuration");
    }
}

} // namespace

int main(int argc, char* argv[])
{
    red::core::LogSink::install();
    applyInterfaceScaleEnv();
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    red::app::Application app(argc, argv);
    if (app.shouldExit()) {
        return app.exitCode();
    }

    red::platform::installCrashHandler(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
                                       QLatin1String("/last-crash.txt"));
    g_previousHandler = qInstallMessageHandler(graphicsWatchHandler);

    auto* gpuWatch = new red::platform::GpuStderrWatch(&app);
    if (gpuWatch->install()) {
        QObject::connect(gpuWatch, &red::platform::GpuStderrWatch::gpuContextLostStorm, &app, [&app] {
            // The experimental GPU video decoder is the usual culprit (no
            // VA-API driver): back that out first and keep the rest of the
            // GPU; only a storm without it costs hardware acceleration.
            if (app.settings().hardwareVideoDecode()) {
                qWarning("GPU context-loss storm with GPU video decoding on; turning it off and relaunching");
                app.settings().setHardwareVideoDecode(false);
                app.settings().setVideoDecodeFallbackNotice(true);
            } else {
                qWarning("GPU context-loss storm detected; disabling hardware acceleration and relaunching");
                app.settings().setGpuAutoDisabled(true);
                app.settings().setGpuFallbackNotice(true);
            }
            app.settings().sync();
            relaunch(app, nullptr);
        });
    }

    red::ui::MainWindow window(app.settings(), app.themeService(),
                               QString::fromLatin1(red::app::version::kVersion));
    QObject::connect(&app, &red::app::Application::raiseRequested, &window,
                     &red::ui::MainWindow::showAndRaise);
    QObject::connect(&app, &red::app::Application::openRequested, &window, &red::ui::MainWindow::openUrl);
    QObject::connect(&app, &red::app::Application::downloadRequested, &window,
                     &red::ui::MainWindow::downloadUrl);
    QObject::connect(&app, &red::app::Application::tvModeRequested, &window,
                     [&window] { window.setTvMode(true); });
    QObject::connect(&app, &red::app::Application::settingsRequested, &window,
                     &red::ui::MainWindow::showSettings);
    QObject::connect(&app, &red::app::Application::quitRequested, &window, &red::ui::MainWindow::quit);

    const red::app::CliOptions& cli = app.cliOptions();
    window.start(cli.tvMode);

    // Developer aid: RED_DEBUG_WINDOW_SIZE=WxH sizes the window for a grab.
    if (const QString size = qEnvironmentVariable("RED_DEBUG_WINDOW_SIZE"); size.contains(u'x')) {
        window.resize(size.section(u'x', 0, 0).toInt(), size.section(u'x', 1, 1).toInt());
    }

    // Developer aid: RED_DEBUG_OPEN=about|shortcuts|tray opens that screen
    // once the window is up (ADR-010).
    if (qEnvironmentVariableIsSet("RED_DEBUG_OPEN")) {
        const QString what = qEnvironmentVariable("RED_DEBUG_OPEN");
        QTimer::singleShot(1200, &window, [&window, what] { window.debugOpen(what); });
    }

    // Developer aid: RED_DEBUG_GRAB=<png path>[,<seconds>] saves a picture of
    // the window after the delay (headless verification, bug reports on a
    // locked desktop). Repeats every <seconds> if the path contains "%1".
    if (qEnvironmentVariableIsSet("RED_DEBUG_GRAB")) {
        const QStringList spec = qEnvironmentVariable("RED_DEBUG_GRAB").split(u',');
        const int delayMs = spec.size() > 1 ? spec.at(1).toInt() * 1000 : 8000;
        auto* grabTimer = new QTimer(&window);
        int shot = 0;
        grabTimer->setInterval(std::max(1000, delayMs));
        QObject::connect(grabTimer, &QTimer::timeout, &window, [&window, spec, grabTimer, shot]() mutable {
            const QString path = spec.first().contains(u"%1"_s) ? spec.first().arg(++shot) : spec.first();
            window.grab().save(path);
            qInfo("debug grab saved to %s", qPrintable(path));
            // Dialogs are separate top-level windows: grab those too.
            int extra = 0;
            for (QWidget* top : QApplication::topLevelWidgets()) {
                const bool popup = qobject_cast<QMenu*>(top) != nullptr; // tray / context menus
                if (top != &window && top->isVisible() && top->isWindow() &&
                    (popup || !top->windowTitle().isEmpty())) {
                    const QString dialogPath = path.chopped(4) + u"-dialog%1.png"_s.arg(++extra);
                    top->grab().save(dialogPath);
                    qInfo("debug grab saved to %s (%s)", qPrintable(dialogPath),
                          qPrintable(top->windowTitle()));
                }
            }
            if (!spec.first().contains(u"%1"_s)) {
                grabTimer->stop();
            }
        });
        grabTimer->start();
    }

    QTimer::singleShot(20000, &app, [&app] { app.markGpuStable(); });

    const bool retried = qEnvironmentVariableIsSet("RED_XCB_RETRY");
    if (!retried && QGuiApplication::platformName() == QLatin1StringView("wayland")) {
        QTimer::singleShot(3000, &window, [&app] {
            if (red::core::shouldRetryUnderXcb(QLatin1StringView("wayland"), false,
                                               g_graphicsFailed.load() || g_gpuBlackScreen.load())) {
                qputenv("QT_QPA_PLATFORM", "xcb");
                relaunch(app, "RED_XCB_RETRY");
            }
        });
    }

#ifdef Q_OS_UNIX
    const int termFd = installGracefulTermination();
    if (termFd >= 0) {
        auto* termNotifier = new QSocketNotifier(termFd, QSocketNotifier::Read, &window);
        QObject::connect(termNotifier, &QSocketNotifier::activated, &window, [&window] {
            char byte = 0;
            const ssize_t ignored = ::read(g_termPipe[0], &byte, 1);
            static_cast<void>(ignored);
            window.quit();
        });
    }
#endif

    // Commands given on our own command line (we are the primary instance).
    // Deferred until the window is mapped: a dialog opened before the
    // (possibly full-screen) main window has a surface gets closed by the
    // compositor on Wayland.
    QTimer::singleShot(400, &window, [&app, &cli] {
        for (const QJsonObject& command : red::app::commandsFor(cli)) {
            const QString name = command.value(QLatin1StringView(red::app::cmd::kKey)).toString();
            if (name != QLatin1StringView(red::app::cmd::kRaise) &&
                name != QLatin1StringView(red::app::cmd::kTv)) {
                app.dispatchCommand(command);
            }
        }
    });

    return red::app::Application::exec();
}
