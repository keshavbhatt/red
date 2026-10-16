#include "ui/download_card_delegate.h"

#include "core/downloads/download_queue.h"
#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/red_style.h"
#include "ui/thumbnail_cache.h"

#include <QAbstractItemView>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kMargin = 10;
constexpr int kThumbWidth = 104;
constexpr int kThumbHeight = 58;
constexpr int kButtonSize = 26;
constexpr int kButtonGap = 4;
constexpr int kRadius = 10;
constexpr int kInset = 6;      ///< card to list edge, horizontally
constexpr int kErrorLines = 4; ///< a failed card shows this much of its reason

/// The card's usual row (thumbnail, title, status, buttons): the whole card,
/// or its top on a failed card that also shows its reason.
QRect mainBlock(const QRect& card)
{
    return {card.left(), card.top(), card.width(), DownloadCardDelegate::kCardHeight - 6};
}

QFont smallFont(const QFont& base)
{
    QFont small = base;
    small.setPointSizeF(base.pointSizeF() - 1);
    return small;
}

core::DownloadState stateOf(const QModelIndex& index)
{
    return index.data(core::DownloadQueue::StateRole).value<core::DownloadState>();
}

QColor stateColor(const Tokens& t, core::DownloadState state)
{
    switch (state) {
    case core::DownloadState::Completed:
        return t.success;
    case core::DownloadState::Failed:
        return t.danger;
    case core::DownloadState::Paused:
    case core::DownloadState::Cancelled:
        return t.warning;
    case core::DownloadState::Queued:
        return t.muted;
    case core::DownloadState::Probing:
    case core::DownloadState::Downloading:
    case core::DownloadState::Processing:
        break;
    }
    return t.brand;
}

} // namespace

DownloadCardDelegate::DownloadCardDelegate(ThumbnailCache& thumbnails, core::ThemeService& theme,
                                           QObject* parent)
    : QStyledItemDelegate(parent)
    , m_thumbnails(thumbnails)
    , m_theme(theme)
{}

QString DownloadCardDelegate::errorText(const QModelIndex& index)
{
    return stateOf(index) == core::DownloadState::Failed
               ? index.data(core::DownloadQueue::ErrorRole).toString().trimmed()
               : QString();
}

int DownloadCardDelegate::errorHeight(const QFont& font, const QString& error, int width) const
{
    if (error.isEmpty()) {
        return 0;
    }
    const QFontMetrics fm(smallFont(font));
    const int wrapped = fm.boundingRect(QRect(0, 0, width, 10000), Qt::TextWordWrap, error).height();
    return std::min(wrapped, fm.lineSpacing() * kErrorLines);
}

QSize DownloadCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QString error = errorText(index);
    const int textWidth = m_cardWidth - 2 * kInset - 2 * kMargin;
    const int extra = error.isEmpty() ? 0 : errorHeight(option.font, error, textWidth) + kMargin;
    return {option.rect.width(), kCardHeight + extra};
}

QList<DownloadCardDelegate::HitButton>
DownloadCardDelegate::buttonsFor(const QRect& card, core::DownloadState state, bool hasFile) const
{
    QList<HitButton> buttons;
    auto add = [&](Action action, const QString& icon, const QString& tooltip) {
        buttons.append({QRect(), action, icon, tooltip});
    };
    switch (state) {
    case core::DownloadState::Queued:
    case core::DownloadState::Probing:
    case core::DownloadState::Downloading:
    case core::DownloadState::Processing:
        add(Action::PauseResume, u"pause"_s, tr("Pause"));
        add(Action::Cancel, u"cancel"_s, tr("Cancel"));
        break;
    case core::DownloadState::Paused:
        add(Action::PauseResume, u"play"_s, tr("Resume"));
        add(Action::Cancel, u"cancel"_s, tr("Cancel"));
        break;
    case core::DownloadState::Completed:
        if (hasFile) {
            add(Action::Open, u"open"_s, tr("Open"));
            add(Action::ShowInFolder, u"folder"_s, tr("Show in folder"));
        }
        add(Action::Remove, u"trash"_s, tr("Remove from list"));
        break;
    case core::DownloadState::Failed:
    case core::DownloadState::Cancelled:
        add(Action::Retry, u"retry"_s, tr("Retry"));
        add(Action::Remove, u"trash"_s, tr("Remove from list"));
        break;
    }
    const QRect block = mainBlock(card);
    int x = block.right() - kMargin - kButtonSize;
    const int y = block.bottom() - kMargin - kButtonSize;
    for (int i = static_cast<int>(buttons.size()) - 1; i >= 0; --i) {
        buttons[i].rect = QRect(x, y, kButtonSize, kButtonSize);
        x -= kButtonSize + kButtonGap;
    }
    return buttons;
}

void DownloadCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const core::DownloadState state = stateOf(index);
    const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
    const QRect card = option.rect.adjusted(kInset, 3, -kInset, -3);
    const QRect block = mainBlock(card);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Card surface.
    QPainterPath path;
    path.addRoundedRect(card, kRadius, kRadius);
    painter->fillPath(path, hovered ? t.elevated : t.panel);
    painter->setPen(QPen(t.border, 1));
    painter->drawPath(path);
    // Keyboard use: a dashed ring on the current card (never for the mouse).
    if (m_keyboardFocus && option.state.testFlag(QStyle::State_HasFocus)) {
        QPainterPath ring;
        ring.addRoundedRect(QRectF(card).adjusted(3, 3, -3, -3), kRadius - 3, kRadius - 3);
        painter->setPen(QPen(t.muted, 1, Qt::DashLine));
        painter->drawPath(ring);
    }

    // Thumbnail with the state bar on its left.
    const QRect thumbRect(card.left() + kMargin, card.top() + kMargin, kThumbWidth, kThumbHeight);
    QPainterPath thumbPath;
    thumbPath.addRoundedRect(thumbRect, 6, 6);
    painter->setClipPath(thumbPath);
    painter->fillRect(thumbRect, t.hover);
    const QString thumbUrl = index.data(core::DownloadQueue::ThumbnailRole).toString();
    const QPixmap thumb = m_thumbnails.get(thumbUrl);
    if (!thumb.isNull()) {
        const QPixmap scaled = thumb.scaled(thumbRect.size() * painter->device()->devicePixelRatio(),
                                            Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        const QRect source(
            (scaled.width() - thumbRect.width() * static_cast<int>(painter->device()->devicePixelRatio())) /
                2,
            (scaled.height() - thumbRect.height() * static_cast<int>(painter->device()->devicePixelRatio())) /
                2,
            thumbRect.width() * static_cast<int>(painter->device()->devicePixelRatio()),
            thumbRect.height() * static_cast<int>(painter->device()->devicePixelRatio()));
        painter->drawPixmap(thumbRect, scaled, source);
    } else {
        const QString glyph =
            index.data(core::DownloadQueue::IsPlaylistRole).toBool() ? u"playlist"_s : u"video"_s;
        const QPixmap icon = icons::pixmap(glyph, t.muted, 24, painter->device()->devicePixelRatio());
        painter->drawPixmap(thumbRect.center().x() - 12, thumbRect.center().y() - 12, icon);
    }
    painter->fillRect(QRect(thumbRect.left(), thumbRect.top(), 3, thumbRect.height()), stateColor(t, state));
    painter->setClipping(false);

    // Text block.
    const int textLeft = thumbRect.right() + kMargin + 2;
    const int textRight = card.right() - kMargin;
    QFont titleFont = option.font;
    titleFont.setWeight(QFont::DemiBold);
    painter->setFont(titleFont);
    painter->setPen(t.text);
    const QString title = index.data(core::DownloadQueue::TitleRole).toString();
    const QRect titleRect(textLeft, card.top() + kMargin - 1, textRight - textLeft, 38);
    const QString elidedTitle =
        painter->fontMetrics().elidedText(title, Qt::ElideRight, titleRect.width() * 2 - 20);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, elidedTitle);

    const QFont small = smallFont(option.font);
    painter->setFont(small);
    painter->setPen(state == core::DownloadState::Failed ? t.danger : t.muted);
    const QString error = errorText(index);
    // A failed card spells its reason out below; the status row just says so.
    QString status =
        error.isEmpty() ? index.data(core::DownloadQueue::StatusLineRole).toString() : tr("Failed");
    const QString uploader = index.data(core::DownloadQueue::UploaderRole).toString();
    if (!uploader.isEmpty() && state == core::DownloadState::Completed) {
        status = uploader + u" · "_s + status;
    }
    const QList<HitButton> buttons =
        buttonsFor(card, state, !index.data(core::DownloadQueue::FilePathRole).toString().isEmpty());
    const int buttonsWidth =
        hovered && !buttons.isEmpty() ? static_cast<int>(buttons.size()) * (kButtonSize + kButtonGap) + 4 : 0;
    const QRect statusRect(textLeft, block.bottom() - kMargin - 18, textRight - textLeft - buttonsWidth, 18);
    painter->drawText(statusRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(status, Qt::ElideRight, statusRect.width()));
    if (!error.isEmpty()) {
        const int width = card.width() - 2 * kMargin;
        const QRect errorRect(card.left() + kMargin, block.bottom() + 1, width,
                              errorHeight(option.font, error, width));
        painter->drawText(errorRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, error);
    }

    // Progress bar under the title while active/paused.
    const double progress = index.data(core::DownloadQueue::ProgressRole).toDouble();
    if (state == core::DownloadState::Downloading || state == core::DownloadState::Paused ||
        state == core::DownloadState::Processing || state == core::DownloadState::Probing) {
        const QRect track(textLeft, statusRect.top() - 8, textRight - textLeft, 4);
        QPainterPath trackPath;
        trackPath.addRoundedRect(track, 2, 2);
        painter->fillPath(trackPath, t.hover);
        if (progress >= 0 && state != core::DownloadState::Processing) {
            QRect fill = track;
            fill.setWidth(std::max(4, static_cast<int>(track.width() * progress)));
            QPainterPath fillPath;
            fillPath.addRoundedRect(fill, 2, 2);
            painter->fillPath(fillPath, stateColor(t, state));
        } else {
            // Indeterminate: a short moving segment based on the clock.
            const int span = track.width() / 4;
            const int offset =
                static_cast<int>((QDateTime::currentMSecsSinceEpoch() / 12) % (track.width() + span)) - span;
            QRect seg(track.left() + std::max(0, offset), track.top(),
                      std::min(span, track.width() - std::max(0, offset)), track.height());
            if (seg.width() > 0) {
                QPainterPath segPath;
                segPath.addRoundedRect(seg, 2, 2);
                painter->fillPath(segPath, stateColor(t, state));
            }
        }
    }

    // Hover actions.
    if (hovered) {
        for (const HitButton& b : buttons) {
            const bool over = b.rect.contains(m_hoverPos);
            QPainterPath bp;
            bp.addRoundedRect(b.rect, 6, 6);
            painter->fillPath(bp, over ? t.hover : t.panel);
            const QPixmap icon =
                icons::pixmap(b.icon, over ? t.text : t.muted, 16, painter->device()->devicePixelRatio());
            painter->drawPixmap(b.rect.center().x() - 8, b.rect.center().y() - 8, icon);
        }
    }
    painter->restore();
}

bool DownloadCardDelegate::editorEvent(QEvent* event, QAbstractItemModel*, const QStyleOptionViewItem& option,
                                       const QModelIndex& index)
{
    const auto* mouse = dynamic_cast<QMouseEvent*>(event);
    if (mouse == nullptr) {
        return false;
    }
    m_hoverPos = mouse->pos();
    m_hoverIndex = index;
    if (event->type() != QEvent::MouseButtonRelease || mouse->button() != Qt::LeftButton) {
        return false;
    }
    const QRect card = option.rect.adjusted(kInset, 3, -kInset, -3);
    const core::DownloadState state = stateOf(index);
    const bool hasFile = !index.data(core::DownloadQueue::FilePathRole).toString().isEmpty();
    for (const HitButton& b : buttonsFor(card, state, hasFile)) {
        if (b.rect.contains(mouse->pos())) {
            Q_EMIT actionTriggered(index.data(core::DownloadQueue::IdRole).toULongLong(), b.action);
            return true;
        }
    }
    return false;
}

bool DownloadCardDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
                                     const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (event->type() != QEvent::ToolTip || !index.isValid()) {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
    const QRect card = option.rect.adjusted(kInset, 3, -kInset, -3);
    const core::DownloadState state = stateOf(index);
    const bool hasFile = !index.data(core::DownloadQueue::FilePathRole).toString().isEmpty();
    for (const HitButton& b : buttonsFor(card, state, hasFile)) {
        if (b.rect.contains(event->pos())) {
            QToolTip::showText(event->globalPos(), b.tooltip, view);
            return true;
        }
    }
    QString tip = index.data(core::DownloadQueue::TitleRole).toString();
    const QString file = index.data(core::DownloadQueue::FilePathRole).toString();
    if (!file.isEmpty()) {
        tip += u"\n"_s + file;
    }
    const QString error = index.data(core::DownloadQueue::ErrorRole).toString();
    if (!error.isEmpty()) {
        tip += u"\n"_s + error;
    }
    QToolTip::showText(event->globalPos(), tip, view);
    return true;
}

} // namespace red::ui
