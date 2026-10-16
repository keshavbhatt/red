#pragma once

#include "core/downloads/download_job.h"

#include <QHash>
#include <QStyledItemDelegate>

namespace red::core {
class ThemeService;
}

namespace red::ui {

class ThumbnailCache;

/// Paints one download as a card (DESIGN.md §2): thumbnail with a state bar,
/// title, status line, progress and hover actions. Actions are hit-tested in
/// editorEvent and reported through the signals.
class DownloadCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DownloadCardDelegate)

public:
    enum class Action
    {
        None,
        PauseResume,
        Cancel,
        Retry,
        Open,
        ShowInFolder,
        Remove,
        CopyLink,  ///< the video's or list's link to the clipboard
        CopyError, ///< a failed download's full reason to the clipboard
    };

    DownloadCardDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent = nullptr);
    ~DownloadCardDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    static constexpr int kCardHeight = 92;

    /// The list's viewport width, for the height of a failed card's wrapped error.
    void setCardWidth(int width) { m_cardWidth = width; }
    /// A dashed ring on the current card while the list is used from the keyboard.
    void setKeyboardFocus(bool on) { m_keyboardFocus = on; }

Q_SIGNALS:
    void actionTriggered(quint64 jobId, red::ui::DownloadCardDelegate::Action action);
    void thumbnailNeeded();

private:
    struct HitButton
    {
        QRect rect;
        Action action;
        QString icon;
        QString tooltip;
    };
    [[nodiscard]] QList<HitButton> buttonsFor(const QRect& card, core::DownloadState state,
                                              bool hasFile) const;
    /// A failed card's reason, wrapped under the card's usual row (empty otherwise).
    [[nodiscard]] static QString errorText(const QModelIndex& index);
    [[nodiscard]] int errorHeight(const QFont& font, const QString& error, int width) const;
    /// The bar's drawn fraction: glides between the engine's reports (one per
    /// chunk read, often a second apart) instead of jumping (see the .cpp).
    [[nodiscard]] double shownProgress(quint64 id, double actual) const;

    struct Glide
    {
        double actual = -1;    ///< the last reported fraction
        qint64 reportedAt = 0; ///< ms, when it arrived
        double rate = 0;       ///< fraction per ms, smoothed over reports
        double step = 0;       ///< the last report's increase: how far ahead the bar may run
        double shown = 0;      ///< what was drawn last
        qint64 shownAt = 0;
    };
    mutable QHash<quint64, Glide> m_glides;

    ThumbnailCache& m_thumbnails;
    core::ThemeService& m_theme;
    QPersistentModelIndex m_hoverIndex;
    QPoint m_hoverPos;
    int m_cardWidth = 368;
    bool m_keyboardFocus = false;
};

} // namespace red::ui
