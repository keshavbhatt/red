#include "ui/downloads_panel.h"

#include "core/downloads/download_queue.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/red_style.h"
#include "ui/thumbnail_cache.h"

#include <QApplication>
#include <QDir>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
core::DownloadState stateOf(const QModelIndex& index)
{
    return index.data(core::DownloadQueue::StateRole).value<core::DownloadState>();
}
} // namespace

DownloadsPanel::DownloadsPanel(core::DownloadQueue& queue, core::Settings& settings,
                               core::ThemeService& theme, ThumbnailCache& thumbnails, QWidget* parent)
    : QFrame(parent)
    , m_queue(queue)
    , m_settings(settings)
    , m_theme(theme)
    , m_thumbnails(thumbnails)
{
    setProperty("redPanel", true);
    setFixedWidth(kWidth);
    setupUi();
    applyIcons();
    updateEmptyState();
    updateFooter();

    connect(&m_queue, &QAbstractItemModel::rowsInserted, this, [this] { updateEmptyState(); });
    connect(&m_queue, &QAbstractItemModel::rowsRemoved, this, [this] { updateEmptyState(); });
    connect(&m_queue, &QAbstractItemModel::modelReset, this, [this] { updateEmptyState(); });
    connect(&m_settings, &core::Settings::downloadDirectoryChanged, this,
            [this](const QString&) { updateFooter(); });
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { applyIcons(); });
    connect(&m_thumbnails, &ThumbnailCache::ready, this,
            [this](const QString&) { m_list->viewport()->update(); });
    connect(m_delegate, &DownloadCardDelegate::actionTriggered, this, &DownloadsPanel::cardAction);
    // A card that turns failed (or is retried) changes height: relayout just then.
    connect(&m_queue, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex& topLeft, const QModelIndex& bottomRight) {
                for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
                    const QModelIndex index = m_queue.index(row, 0);
                    const quint64 id = index.data(core::DownloadQueue::IdRole).toULongLong();
                    const bool tall =
                        stateOf(index) == core::DownloadState::Failed &&
                        !index.data(core::DownloadQueue::ErrorRole).toString().trimmed().isEmpty();
                    if (m_showsError.value(id) != tall) {
                        m_showsError.insert(id, tall);
                        Q_EMIT m_delegate->sizeHintChanged(index);
                    }
                }
            });
    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!index.data(core::DownloadQueue::FilePathRole).toString().isEmpty()) {
            Q_EMIT cardAction(index.data(core::DownloadQueue::IdRole).toULongLong(),
                              DownloadCardDelegate::Action::Open);
        }
    });
    // Indeterminate bars and speed lines need a repaint while anything runs.
    auto* ticker = new QTimer(this);
    ticker->setInterval(33); // ~30 fps: the bars glide between the engine's reports
    connect(ticker, &QTimer::timeout, this, [this, ticker] {
        if (m_queue.runningCount() == 0) {
            ticker->stop(); // idle: no wake-ups
        } else if (isVisible()) {
            m_list->viewport()->update();
        }
    });
    // Runs only while something transfers: a job changing state starts it.
    connect(&m_queue, &QAbstractItemModel::dataChanged, ticker, [this, ticker] {
        if (m_queue.runningCount() > 0 && !ticker->isActive()) {
            ticker->start();
        }
    });
}

void DownloadsPanel::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header.
    auto* header = new QWidget(this);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 12, 8, 8);
    headerLayout->setSpacing(6);
    m_title = new QLabel(tr("Downloads"), header);
    m_title->setProperty("redTitle", true);
    headerLayout->addWidget(m_title);
    m_engineChip = new QToolButton(header);
    m_engineChip->setCursor(Qt::PointingHandCursor);
    m_engineChip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_engineChip->setIconSize(QSize(14, 14));
    m_engineChip->setFocusPolicy(Qt::TabFocus);
    connect(m_engineChip, &QToolButton::clicked, this, &DownloadsPanel::engineRequested);
    headerLayout->addWidget(m_engineChip);
    headerLayout->addStretch(1);
    m_newButton = new QToolButton(header);
    m_newButton->setToolTip(tr("Download from a link  (Ctrl+N)"));
    m_newButton->setAccessibleName(tr("Download from a link"));
    m_newButton->setCursor(Qt::PointingHandCursor);
    m_newButton->setIconSize(QSize(20, 20));
    m_newButton->setFocusPolicy(Qt::TabFocus);
    connect(m_newButton, &QToolButton::clicked, this, &DownloadsPanel::newDownloadRequested);
    headerLayout->addWidget(m_newButton);
    m_moreButton = new QToolButton(header);
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    m_moreButton->setToolTip(tr("More download actions"));
    m_moreButton->setAccessibleName(tr("More download actions"));
    m_moreButton->setCursor(Qt::PointingHandCursor);
    m_moreButton->setIconSize(QSize(20, 20));
    m_moreButton->setFocusPolicy(Qt::TabFocus);
    m_moreMenu = new QMenu(m_moreButton);
    m_moreMenu->addAction(tr("Pause all"), &m_queue, &core::DownloadQueue::pauseAll);
    m_moreMenu->addAction(tr("Resume all"), &m_queue, &core::DownloadQueue::resumeAll);
    m_moreMenu->addSeparator();
    m_moreMenu->addAction(tr("Clear finished"), &m_queue, &core::DownloadQueue::clearFinished);
    m_moreMenu->addAction(tr("Open download folder"), this, &DownloadsPanel::openFolderRequested);
    m_moreMenu->addSeparator();
    m_moreMenu->addAction(tr("Download engine…"), this, &DownloadsPanel::engineRequested);
    m_moreButton->setMenu(m_moreMenu);
    headerLayout->addWidget(m_moreButton);
    m_closeButton = new QToolButton(header);
    m_closeButton->setToolTip(tr("Close panel  (Ctrl+J)"));
    m_closeButton->setAccessibleName(tr("Close the downloads panel"));
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setIconSize(QSize(18, 18));
    m_closeButton->setFocusPolicy(Qt::TabFocus);
    connect(m_closeButton, &QToolButton::clicked, this, &DownloadsPanel::closeRequested);
    headerLayout->addWidget(m_closeButton);
    root->addWidget(header);

    // List / empty state.
    m_stack = new QStackedWidget(this);
    m_list = new QListView(m_stack);
    m_list->setModel(&m_queue);
    m_delegate = new DownloadCardDelegate(m_thumbnails, m_theme, this);
    m_list->setItemDelegate(m_delegate);
    m_list->setMouseTracking(true);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setUniformItemSizes(false);    // a failed card grows to show its reason
    m_delegate->setCardWidth(kWidth - 12); // the panel is fixed; leave room for a scroll bar
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setAccessibleName(tr("Downloads"));
    m_list->installEventFilter(this);
    for (QWidget* w :
         std::initializer_list<QWidget*>{m_engineChip, m_newButton, m_moreButton, m_closeButton}) {
        w->installEventFilter(this); // Esc: back to the page
    }
    m_list->viewport()->installEventFilter(this);
    connect(m_list, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QModelIndex index = m_list->indexAt(pos);
        QPoint at = pos;
        if (m_keyboard || !index.isValid()) { // the Menu key: the current card
            index = m_list->currentIndex();
            at = m_list->visualRect(index).center();
        }
        if (index.isValid()) {
            showCardMenu(index, m_list->viewport()->mapToGlobal(at));
        }
    });
    m_list->setSpacing(0);
    m_list->viewport()->setAttribute(Qt::WA_Hover);
    m_stack->addWidget(m_list);

    auto* empty = new QWidget(m_stack);
    auto* emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(32, 0, 32, 0);
    emptyLayout->setSpacing(8);
    emptyLayout->addStretch(2);
    m_emptyIcon = new QLabel(empty);
    m_emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyIcon);
    m_emptyTitle = new QLabel(tr("No downloads yet"), empty);
    m_emptyTitle->setProperty("redTitle", true);
    m_emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyTitle);
    m_emptyBody = new QLabel(
        tr("Open a video, playlist or channel and press the download button, or paste a link."), empty);
    m_emptyBody->setProperty("redMuted", true);
    m_emptyBody->setWordWrap(true);
    m_emptyBody->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyBody);
    m_emptyButton = new QPushButton(tr("Paste a link…"), empty);
    m_emptyButton->setCursor(Qt::PointingHandCursor);
    connect(m_emptyButton, &QPushButton::clicked, this, &DownloadsPanel::newDownloadRequested);
    emptyLayout->addWidget(m_emptyButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(3);
    m_stack->addWidget(empty);
    root->addWidget(m_stack, 1);

    // Footer.
    auto* sep = new QFrame(this);
    sep->setProperty("redSeparator", true);
    root->addWidget(sep);
    auto* footer = new QWidget(this);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 8, 8, 8);
    footerLayout->setSpacing(6);
    m_folderButton = new QToolButton(footer);
    m_folderButton->setToolTip(tr("Open download folder"));
    m_folderButton->setAccessibleName(tr("Open download folder"));
    m_folderButton->setCursor(Qt::PointingHandCursor);
    m_folderButton->setIconSize(QSize(18, 18));
    m_folderButton->setFocusPolicy(Qt::TabFocus);
    connect(m_folderButton, &QToolButton::clicked, this, &DownloadsPanel::openFolderRequested);
    footerLayout->addWidget(m_folderButton);
    m_folderLabel = new QLabel(footer);
    m_folderLabel->setProperty("redMuted", true);
    footerLayout->addWidget(m_folderLabel, 1);
    m_changeButton = new QToolButton(footer);
    m_changeButton->setText(tr("Change"));
    m_changeButton->setCursor(Qt::PointingHandCursor);
    m_changeButton->setFocusPolicy(Qt::TabFocus);
    connect(m_changeButton, &QToolButton::clicked, this, &DownloadsPanel::changeFolderRequested);
    footerLayout->addWidget(m_changeButton);
    root->addWidget(footer);
    m_folderButton->installEventFilter(this);
    m_changeButton->installEventFilter(this);
}

void DownloadsPanel::applyIcons()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    m_newButton->setIcon(icons::themed(u"plus"_s, t.text));
    m_moreButton->setIcon(icons::themed(u"more"_s, t.text));
    m_closeButton->setIcon(icons::themed(u"stop"_s, t.text));
    m_folderButton->setIcon(icons::themed(u"folder"_s, t.muted));
    m_emptyIcon->setPixmap(icons::pixmap(u"downloads"_s, t.muted, 48, devicePixelRatioF()));
    setEngineStatus(m_engine);
}

void DownloadsPanel::setEngineStatus(const services::EngineManager::Status& status)
{
    m_engine = status;
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    using State = services::EngineManager::State;
    QString text;
    QColor color = t.muted;
    QString icon = u"bolt"_s;
    switch (status.state) {
    case State::Unknown:
        text = tr("Engine…");
        break;
    case State::NotInstalled:
        text = tr("Set up engine");
        color = t.warning;
        icon = u"warning"_s;
        break;
    case State::Installing:
    case State::Updating:
        text = status.stepLabel.isEmpty() ? tr("Setting up…") : status.stepLabel;
        if (status.progress >= 0) {
            text += u" %1%"_s.arg(static_cast<int>(status.progress * 100));
        }
        color = t.link;
        break;
    case State::Ready:
        text = status.updateAvailable
                   ? tr("Update available")
                   : (status.systemYtdlp ? tr("Engine (system)") : tr("Engine %1").arg(status.ytdlpVersion));
        color = status.updateAvailable ? t.warning : t.success;
        icon = status.updateAvailable ? u"warning"_s : u"check"_s;
        break;
    case State::Error:
        text = tr("Engine error");
        color = t.danger;
        icon = u"warning"_s;
        break;
    }
    m_engineChip->setText(text);
    m_engineChip->setIcon(icons::themed(icon, color));
    m_engineChip->setToolTip(
        status.error.isEmpty() ? tr("Download engine %1 · media converter %2")
                                     .arg(status.ytdlpVersion.isEmpty() ? tr("missing") : status.ytdlpVersion,
                                          status.ffmpegPath.isEmpty() ? tr("missing") : tr("ready"))
                               : status.error);
    m_engineChip->setStyleSheet(
        u"QToolButton{border:1px solid %1;border-radius:11px;padding:2px 8px 2px 6px;color:%2;font-size:11px;font-weight:600;}QToolButton:hover{background:%3;}QToolButton:focus{border-style:dashed;}"_s
            .arg(t.border.name(), color.name(), t.hover.name()));
}

void DownloadsPanel::updateEmptyState()
{
    m_stack->setCurrentIndex(m_queue.rowCount() == 0 ? 1 : 0);
}

void DownloadsPanel::updateFooter()
{
    const QString dir = m_settings.downloadDirectory();
    QString shown = dir;
    const QString home = QDir::homePath();
    if (shown.startsWith(home)) {
        shown = u"~"_s + shown.mid(home.size());
    }
    m_folderLabel->setText(m_folderLabel->fontMetrics().elidedText(shown, Qt::ElideMiddle, kWidth - 130));
    m_folderLabel->setToolTip(dir);
}

void DownloadsPanel::emitAction(const QModelIndex& index, DownloadCardDelegate::Action action)
{
    Q_EMIT cardAction(index.data(core::DownloadQueue::IdRole).toULongLong(), action);
}

void DownloadsPanel::showCardMenu(const QModelIndex& index, const QPoint& globalPos)
{
    using Action = DownloadCardDelegate::Action;
    const core::DownloadState state = stateOf(index);
    const bool hasFile = !index.data(core::DownloadQueue::FilePathRole).toString().isEmpty();
    const bool hasError = !index.data(core::DownloadQueue::ErrorRole).toString().trimmed().isEmpty();
    const QPersistentModelIndex card(index);
    QMenu menu(this);
    const auto add = [&](Action action, const QString& text) {
        menu.addAction(text, this, [this, card, action, globalPos] {
            if (!card.isValid()) {
                return; // removed while the menu was open
            }
            emitAction(card, action);
            if (action == Action::CopyLink || action == Action::CopyError) {
                QToolTip::showText(globalPos, tr("Copied"), m_list);
            }
        });
    };
    bool finished = false;
    switch (state) {
    case core::DownloadState::Completed:
        if (hasFile) {
            add(Action::Open, tr("Open"));
        }
        add(Action::ShowInFolder, tr("Show in folder"));
        finished = true;
        break;
    case core::DownloadState::Queued:
    case core::DownloadState::Probing:
    case core::DownloadState::Downloading:
    case core::DownloadState::Processing:
        add(Action::PauseResume, tr("Pause"));
        add(Action::Cancel, tr("Cancel"));
        add(Action::ShowInFolder, tr("Show in folder"));
        break;
    case core::DownloadState::Paused:
        add(Action::PauseResume, tr("Resume"));
        add(Action::Cancel, tr("Cancel"));
        add(Action::ShowInFolder, tr("Show in folder"));
        break;
    case core::DownloadState::Failed:
    case core::DownloadState::Cancelled:
        add(Action::Retry, tr("Retry"));
        if (state == core::DownloadState::Failed && hasError) {
            add(Action::CopyError, tr("Copy error"));
        }
        finished = true;
        break;
    }
    menu.addSeparator();
    add(Action::CopyLink, tr("Copy link"));
    if (finished) {
        menu.addSeparator();
        add(Action::Remove, tr("Remove from list"));
    }
    menu.exec(globalPos);
}

void DownloadsPanel::runPrimaryAction(const QModelIndex& index)
{
    using Action = DownloadCardDelegate::Action;
    switch (stateOf(index)) {
    case core::DownloadState::Completed:
        if (!index.data(core::DownloadQueue::FilePathRole).toString().isEmpty()) {
            emitAction(index, Action::Open);
        }
        break;
    case core::DownloadState::Failed:
    case core::DownloadState::Cancelled:
        emitAction(index, Action::Retry);
        break;
    case core::DownloadState::Paused:
        emitAction(index, Action::PauseResume);
        break;
    case core::DownloadState::Queued:
    case core::DownloadState::Probing:
    case core::DownloadState::Downloading:
    case core::DownloadState::Processing:
        break;
    }
}

/// Keyboard use of the card list: Enter for the card's main action, Delete to
/// remove a finished card, Space to pause or resume; the dashed focus ring is
/// shown only while the keyboard is in use.
bool DownloadsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape &&
        watched->isWidgetType() && isAncestorOf(static_cast<QWidget*>(watched))) {
        Q_EMIT leaveRequested();
        return true;
    }
    if (watched == m_list->viewport() && event->type() == QEvent::MouseButtonPress && m_keyboard) {
        m_keyboard = false;
        m_delegate->setKeyboardFocus(false);
        m_list->viewport()->update();
    }
    if (watched == m_list && event->type() == QEvent::FocusIn) {
        const auto* focus = static_cast<QFocusEvent*>(event);
        if (focus->reason() == Qt::TabFocusReason || focus->reason() == Qt::BacktabFocusReason ||
            focus->reason() == Qt::ShortcutFocusReason) {
            m_keyboard = true;
            m_delegate->setKeyboardFocus(true);
            if (!m_list->currentIndex().isValid() && m_queue.rowCount() > 0) {
                m_list->setCurrentIndex(m_queue.index(0, 0));
            }
        }
    }
    if (watched == m_list && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (!m_keyboard) {
            m_keyboard = true;
            m_delegate->setKeyboardFocus(true);
            m_list->viewport()->update();
        }
        const QModelIndex current = m_list->currentIndex();
        if (current.isValid()) {
            const core::DownloadState state = stateOf(current);
            const bool finished = state == core::DownloadState::Completed ||
                                  state == core::DownloadState::Failed ||
                                  state == core::DownloadState::Cancelled;
            switch (key->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
                runPrimaryAction(current);
                return true;
            case Qt::Key_Delete:
                if (finished) {
                    emitAction(current, DownloadCardDelegate::Action::Remove);
                }
                return true;
            case Qt::Key_Space:
                if (!finished) {
                    emitAction(current, DownloadCardDelegate::Action::PauseResume);
                }
                return true;
            default:
                break;
            }
        }
    }
    return QFrame::eventFilter(watched, event);
}

void DownloadsPanel::focusFirst()
{
    if (m_stack->currentWidget() == m_list && m_queue.rowCount() > 0) {
        m_list->setFocus(Qt::ShortcutFocusReason);
    } else {
        m_newButton->setFocus(Qt::ShortcutFocusReason);
    }
}

bool DownloadsPanel::hasFocusInside() const
{
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && isAncestorOf(focus);
}

} // namespace red::ui
