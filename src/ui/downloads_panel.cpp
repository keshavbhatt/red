#include "ui/downloads_panel.h"

#include "core/downloads/download_queue.h"
#include "core/settings/settings.h"
#include "core/theme/theme_service.h"
#include "ui/icons.h"
#include "ui/red_style.h"
#include "ui/thumbnail_cache.h"

#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

DownloadsPanel::DownloadsPanel(core::DownloadQueue& queue, core::Settings& settings, core::ThemeService& theme,
                               ThumbnailCache& thumbnails, QWidget* parent)
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
    connect(&m_settings, &core::Settings::downloadDirectoryChanged, this, [this](const QString&) { updateFooter(); });
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this, [this](Qt::ColorScheme) { applyIcons(); });
    connect(&m_thumbnails, &ThumbnailCache::ready, this, [this](const QString&) { m_list->viewport()->update(); });
    connect(m_delegate, &DownloadCardDelegate::actionTriggered, this, &DownloadsPanel::cardAction);
    connect(m_list, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!index.data(core::DownloadQueue::FilePathRole).toString().isEmpty()) {
            Q_EMIT cardAction(index.data(core::DownloadQueue::IdRole).toULongLong(), DownloadCardDelegate::Action::Open);
        }
    });
    // Indeterminate bars and speed lines need a repaint while anything runs.
    auto* ticker = new QTimer(this);
    ticker->setInterval(120);
    connect(ticker, &QTimer::timeout, this, [this] {
        if (m_queue.runningCount() > 0 && isVisible()) {
            m_list->viewport()->update();
        }
    });
    ticker->start();
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
    m_engineChip->setFocusPolicy(Qt::NoFocus);
    connect(m_engineChip, &QToolButton::clicked, this, &DownloadsPanel::engineRequested);
    headerLayout->addWidget(m_engineChip);
    headerLayout->addStretch(1);
    m_newButton = new QToolButton(header);
    m_newButton->setToolTip(tr("Download from a link  (Ctrl+N)"));
    m_newButton->setCursor(Qt::PointingHandCursor);
    m_newButton->setIconSize(QSize(20, 20));
    m_newButton->setFocusPolicy(Qt::NoFocus);
    connect(m_newButton, &QToolButton::clicked, this, &DownloadsPanel::newDownloadRequested);
    headerLayout->addWidget(m_newButton);
    m_moreButton = new QToolButton(header);
    m_moreButton->setPopupMode(QToolButton::InstantPopup);
    m_moreButton->setCursor(Qt::PointingHandCursor);
    m_moreButton->setIconSize(QSize(20, 20));
    m_moreButton->setFocusPolicy(Qt::NoFocus);
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
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setIconSize(QSize(18, 18));
    m_closeButton->setFocusPolicy(Qt::NoFocus);
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
    m_list->setUniformItemSizes(true);
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
    m_emptyBody = new QLabel(tr("Open a video, playlist or channel and press the download button, or paste a link."), empty);
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
    m_folderButton->setCursor(Qt::PointingHandCursor);
    m_folderButton->setIconSize(QSize(18, 18));
    m_folderButton->setFocusPolicy(Qt::NoFocus);
    connect(m_folderButton, &QToolButton::clicked, this, &DownloadsPanel::openFolderRequested);
    footerLayout->addWidget(m_folderButton);
    m_folderLabel = new QLabel(footer);
    m_folderLabel->setProperty("redMuted", true);
    footerLayout->addWidget(m_folderLabel, 1);
    m_changeButton = new QToolButton(footer);
    m_changeButton->setText(tr("Change"));
    m_changeButton->setCursor(Qt::PointingHandCursor);
    m_changeButton->setFocusPolicy(Qt::NoFocus);
    connect(m_changeButton, &QToolButton::clicked, this, &DownloadsPanel::changeFolderRequested);
    footerLayout->addWidget(m_changeButton);
    root->addWidget(footer);
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
        text = status.updateAvailable ? tr("Update available") : (status.systemYtdlp ? tr("yt-dlp (system)") : u"yt-dlp "_s + status.ytdlpVersion);
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
    m_engineChip->setToolTip(status.error.isEmpty() ? tr("Download engine: yt-dlp %1 · %2 · ffmpeg %3")
                                                         .arg(status.ytdlpVersion.isEmpty() ? tr("missing") : status.ytdlpVersion,
                                                              status.jsRuntime.isEmpty() ? tr("no JavaScript runtime") : core::jsRuntimeLabel(status.jsRuntime),
                                                              status.ffmpegPath.isEmpty() ? tr("missing") : tr("ready"))
                                                   : status.error);
    m_engineChip->setStyleSheet(u"QToolButton{border:1px solid %1;border-radius:11px;padding:2px 8px 2px 6px;color:%2;font-size:11px;font-weight:600;}QToolButton:hover{background:%3;}"_s
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

} // namespace red::ui
