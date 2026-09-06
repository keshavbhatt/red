#pragma once

#include <QString>

#include <optional>
#include <variant>

// Parses the lines yt-dlp prints when driven by downloadArguments() (ADR-005).
namespace red::core {

struct ProgressEvent
{
    QString status; ///< "downloading" | "finished" | "error"
    qint64 downloaded = 0;
    qint64 total = 0;    ///< exact total, 0 unknown
    qint64 estimate = 0; ///< estimated total, 0 unknown
    double speed = 0;    ///< bytes/s
    int eta = 0;         ///< seconds
    QString filename;
    int index = 0; ///< playlist index, 0 for single
    int count = 0; ///< playlist count, 0 for single
    QString id;

    [[nodiscard]] qint64 totalOrEstimate() const { return total > 0 ? total : estimate; }
};

struct PostprocessEvent
{
    QString status;        ///< "started" | "finished"
    QString postprocessor; ///< "Merger", "EmbedThumbnail", "Metadata", "MoveFiles", …
    QString id;
    /// A human label ("Merging", "Embedding thumbnail", …).
    [[nodiscard]] QString label() const;
};

struct ItemEvent
{
    QString id;
    QString title;
    int index = 0;
    int count = 0;
    QString thumbnail;
    double duration = 0;
    QString uploader;
    QString url;
};

struct FileEvent
{
    QString path;
};

struct MessageEvent
{
    enum class Level
    {
        Info,
        Warning,
        Error,
    };
    Level level = Level::Info;
    QString text;
};

using OutputEvent = std::variant<ProgressEvent, PostprocessEvent, ItemEvent, FileEvent, MessageEvent>;

/// Parses one line of stdout/stderr. Returns nullopt for empty lines.
[[nodiscard]] std::optional<OutputEvent> parseOutputLine(const QString& line);

/// Reduces yt-dlp's error text to something a user can act on
/// ("Sign in to confirm you're not a bot" → suggests signing in, etc.).
[[nodiscard]] QString friendlyError(const QString& stderrTail, int exitCode);

} // namespace red::core
