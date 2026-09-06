#pragma once

#include "services/engine_manager.h"

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;

namespace red::core {
class ThemeService;
}

namespace red::ui {

/// The first-use sheet that provisions the download engine (DESIGN.md §7):
/// three rows (yt-dlp, JavaScript runtime, ffmpeg) with state glyphs, a
/// progress bar and an error line with Retry. Modal to the window only while
/// the user watches it; closing it does not stop the install.
class EngineSetupDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(EngineSetupDialog)

public:
    EngineSetupDialog(services::EngineManager& engine, core::ThemeService& theme, QWidget* parent = nullptr);
    ~EngineSetupDialog() override = default;

private:
    void setupUi();
    void refresh(const services::EngineManager::Status& status);
    QWidget* makeRow(QLabel** glyph, QLabel** text, const QString& title);

    services::EngineManager& m_engine;
    core::ThemeService& m_theme;
    QLabel* m_ytdlpGlyph = nullptr;
    QLabel* m_ytdlpText = nullptr;
    QLabel* m_jsGlyph = nullptr;
    QLabel* m_jsText = nullptr;
    QLabel* m_ffmpegGlyph = nullptr;
    QLabel* m_ffmpegText = nullptr;
    QLabel* m_step = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_error = nullptr;
    QPushButton* m_action = nullptr;
    QPushButton* m_close = nullptr;
};

} // namespace red::ui
