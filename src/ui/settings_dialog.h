#pragma once

#include "services/engine_manager.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;

namespace red::core {
class Settings;
class ThemeService;
} // namespace red::core
namespace red::web {
class RequestInterceptor;
}

namespace red::ui {

/// Settings (DESIGN.md §4): sidebar navigation with seven pages of grouped
/// cards. Widgets write straight to core::Settings; nothing is buffered.
/// Destructive or engine-level operations are signalled to the window.
class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SettingsDialog)

public:
    SettingsDialog(core::Settings& settings, core::ThemeService& theme, services::EngineManager& engine,
                   web::RequestInterceptor& interceptor, bool trayAvailable, QWidget* parent = nullptr);
    ~SettingsDialog() override = default;

    void showPage(int index);
    enum Page
    {
        General,
        Appearance,
        Playback,
        Blocking,
        Downloads,
        TvMode,
        Advanced,
    };

Q_SIGNALS:
    void clearCacheRequested();
    void resetPermissionsRequested();
    void clearSessionRequested();
    void openLogFolderRequested();
    void copyDiagnosticsRequested();
    void engineDialogRequested();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUi();
    QWidget* wrapPage(const QString& title, const QString& subtitle, const QList<QWidget*>& cards);
    QWidget* makeCard(const QString& title, QWidget* content);
    QWidget* buildGeneral();
    QWidget* buildAppearance();
    QWidget* buildPlayback();
    QWidget* buildBlocking();
    QWidget* buildDownloads();
    QWidget* buildTv();
    QWidget* buildAdvanced();
    void loadValues();
    void refreshEngine(const services::EngineManager::Status& status);
    void installWheelGuards();
    void applyNavIcons();

    core::Settings& m_settings;
    core::ThemeService& m_theme;
    services::EngineManager& m_engine;
    web::RequestInterceptor& m_interceptor;
    bool m_trayAvailable;

    QListWidget* m_nav = nullptr;
    QStackedWidget* m_pages = nullptr;

    // general
    QComboBox* m_startMode = nullptr;
    QComboBox* m_startPage = nullptr;
    QComboBox* m_closeAction = nullptr;
    QCheckBox* m_trayEnabled = nullptr;
    QCheckBox* m_showRail = nullptr;
    QCheckBox* m_railLabels = nullptr;
    QCheckBox* m_notify = nullptr;
    // appearance
    QComboBox* m_theme_ = nullptr;
    QDoubleSpinBox* m_zoom = nullptr;
    QDoubleSpinBox* m_scale = nullptr;
    // playback
    QCheckBox* m_autoplay = nullptr;
    QComboBox* m_codec = nullptr;
    QCheckBox* m_hwDecode = nullptr;
    QCheckBox* m_keepAwake = nullptr;
    QCheckBox* m_taskbarProgress = nullptr;
    QCheckBox* m_background = nullptr;
    QComboBox* m_miniSize = nullptr;
    // blocking
    QCheckBox* m_blockAds = nullptr;
    QCheckBox* m_blockTrackers = nullptr;
    QCheckBox* m_hideShorts = nullptr;
    QCheckBox* m_showDislikes = nullptr;
    QLabel* m_blockedCount = nullptr;
    QCheckBox* m_sponsorEnabled = nullptr;
    QCheckBox* m_sponsorMarkers = nullptr;
    QList<QComboBox*> m_sponsorCombos;
    // downloads
    QLineEdit* m_folder = nullptr;
    QComboBox* m_pattern = nullptr;
    QCheckBox* m_organise = nullptr;
    QComboBox* m_quality = nullptr;
    QComboBox* m_container = nullptr;
    QComboBox* m_audioFormat = nullptr;
    QSpinBox* m_concurrent = nullptr;
    QSpinBox* m_speedLimit = nullptr;
    QCheckBox* m_cookies = nullptr;
    QCheckBox* m_embedThumb = nullptr;
    QCheckBox* m_embedMeta = nullptr;
    QCheckBox* m_removeSponsors = nullptr;
    QLabel* m_engineSummary = nullptr;
    QPushButton* m_engineButton = nullptr;
    QCheckBox* m_engineAutoUpdate = nullptr;
    QCheckBox* m_engineUseSystem = nullptr;
    QLineEdit* m_engineSystemPath = nullptr;
    // tv
    QCheckBox* m_tvFullscreen = nullptr;
    QCheckBox* m_tvGamepad = nullptr;
    QCheckBox* m_tvHideShorts = nullptr;
    QCheckBox* m_tvUnlock = nullptr;
    QCheckBox* m_tvLowMemory = nullptr;
    // advanced
    QComboBox* m_hwAccel = nullptr;
    QComboBox* m_signInUa = nullptr;
    QLineEdit* m_uaOverride = nullptr;
};

} // namespace red::ui
