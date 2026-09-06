#pragma once

#include "core/downloads/download_job.h"

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
    };

    DownloadCardDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme, QObject* parent = nullptr);
    ~DownloadCardDelegate() override = default;

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    static constexpr int kCardHeight = 92;

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
    [[nodiscard]] QList<HitButton> buttonsFor(const QRect& card, core::DownloadState state, bool hasFile) const;

    ThumbnailCache& m_thumbnails;
    core::ThemeService& m_theme;
    QPersistentModelIndex m_hoverIndex;
    QPoint m_hoverPos;
};

} // namespace red::ui
