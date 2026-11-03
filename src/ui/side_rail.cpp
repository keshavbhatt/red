#include "ui/side_rail.h"

#include "core/theme/theme_service.h"
#include "ui/actions.h"
#include "ui/icons.h"
#include "ui/rail_button.h"
#include "ui/red_style.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kRailWidth = 52;
constexpr int kExpandedWidth = 200;
constexpr int kSideMargin = 6;
constexpr int kButtonSide = 40;
constexpr int kModeButtonSide = 36;
constexpr int kIconSize = 22;
constexpr int kModeIconSize = 20;
constexpr int kModeInset = 4; // inside the mode switch's outline
constexpr int kModeGap = 3;   // between its buttons
constexpr int kSeparatorWidth = 24;

QString withShortcut(const QString& text, const QAction* action)
{
    const QString keys = action->shortcut().toString(QKeySequence::NativeText);
    return keys.isEmpty() ? text : u"%1  (%2)"_s.arg(text, keys);
}
} // namespace

SideRail::SideRail(Actions& actions, core::ThemeService& theme, QWidget* parent)
    : QFrame(parent)
    , m_actions(actions)
    , m_theme(theme)
{
    setProperty("redRail", true);
    setupUi();
    applyWidths();
    applyIcons();
    setDownloadable(false);
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { applyIcons(); });
}

RailButton* SideRail::makeButton(QAction* action, const QString& icon, int side)
{
    auto* button = new RailButton(side, this);
    if (action != nullptr) {
        button->setDefaultAction(action);
        // On the action, not the button: the button re-copies the action's
        // tooltip on every action change and would otherwise drop the shortcut.
        if (action->toolTip().isEmpty() || action->toolTip() == action->text()) {
            QString text = action->text();
            action->setToolTip(withShortcut(text.remove(u'…').replace(u"&&"_s, u"&"_s), action));
        }
        button->setToolTip(action->toolTip());
    }
    button->setProperty("redRailButton", true);
    button->setProperty("redIcon", icon);
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setCursor(Qt::PointingHandCursor);
    // Tab and F6 reach it; a click does not take focus from the page, whose
    // own keys (K, J, L, F…) need it.
    button->setFocusPolicy(Qt::TabFocus);
    button->installEventFilter(this);
    return button;
}

/// The three modes stacked in one outlined group, the active one lit: always
/// says where Red is, and the way back to YouTube is the top button.
QWidget* SideRail::makeModeSwitch()
{
    m_modeSwitch = new QFrame(this);
    m_modeSwitch->setProperty("redModeSwitch", true);
    auto* layout = new QVBoxLayout(m_modeSwitch);
    layout->setContentsMargins(1, kModeInset, 1, kModeInset); // sides: applyWidths()
    layout->setSpacing(kModeGap);
    const auto add = [this, layout](QAction* action, const QString& icon) {
        RailButton* button = makeButton(action, icon, kModeButtonSide);
        button->setParent(m_modeSwitch);
        button->setProperty("redModeButton", true);
        button->setIconSize(QSize(kModeIconSize, kModeIconSize));
        layout->addWidget(button, 0, Qt::AlignHCenter);
        return button;
    };
    m_youtube = add(m_actions.youtubeMode, u"youtube"_s);
    m_music = add(m_actions.musicMode, u"music"_s);
    m_tv = add(m_actions.tvMode, u"tv"_s);
    return m_modeSwitch;
}

void SideRail::setupUi()
{
    // In a window shorter than the rail's buttons the rail scrolls (wheel)
    // instead of squeezing them on top of each other.
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(u"railScroll"_s);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    scroll->viewport()->setAutoFillBackground(false);
    auto* content = new QWidget(scroll);
    content->setAutoFillBackground(false);
    scroll->setWidget(content);
    scroll->setStyleSheet(
        u"QScrollArea#railScroll, QScrollArea#railScroll > QWidget > QWidget { background: transparent; }"_s);
    outer->addWidget(scroll);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(kSideMargin, 8, kSideMargin, 8);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignHCenter);
    const auto separator = [this, layout] {
        auto* line = new QFrame(this);
        line->setProperty("redSeparator", true);
        m_separators << line;
        layout->addSpacing(4);
        layout->addWidget(line, 0, Qt::AlignHCenter);
        layout->addSpacing(4);
    };

    m_logo = makeButton(m_actions.home, u"logo"_s);
    m_logo->setIconSize(QSize(28, 28));
    m_logo->setToolTip(withShortcut(tr("Home"), m_actions.home));
    layout->addWidget(m_logo, 0, Qt::AlignHCenter);
    layout->addSpacing(6);
    layout->addWidget(makeModeSwitch(), 0, Qt::AlignHCenter);
    layout->addSpacing(6);

    m_back = makeButton(m_actions.back, u"back"_s);
    m_forward = makeButton(m_actions.forward, u"forward"_s);
    m_reload = makeButton(m_actions.reload, u"reload"_s);
    layout->addWidget(m_back, 0, Qt::AlignHCenter);
    layout->addWidget(m_forward, 0, Qt::AlignHCenter);
    layout->addWidget(m_reload, 0, Qt::AlignHCenter);
    separator();

    m_download = makeButton(m_actions.download, u"download"_s);
    layout->addWidget(m_download, 0, Qt::AlignHCenter);
    // The count badge rides on the Downloads button: its top right corner
    // when collapsed, the end of the label row when expanded.
    m_downloads = makeButton(m_actions.downloads, u"downloads"_s);
    m_badge = new QLabel(m_downloads);
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->setFixedHeight(16);
    m_badge->setMinimumWidth(16);
    m_badge->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_badge->hide();
    layout->addWidget(m_downloads, 0, Qt::AlignHCenter);
    m_miniPlayer = makeButton(m_actions.miniPlayer, u"miniplayer"_s);
    layout->addWidget(m_miniPlayer, 0, Qt::AlignHCenter);

    layout->addStretch(1);

    m_settings = makeButton(m_actions.settings, u"settings"_s);
    layout->addWidget(m_settings, 0, Qt::AlignHCenter);
    m_menu = makeButton(nullptr, u"more"_s);
    m_menu->setLabel(tr("More"));
    m_menu->setToolTip(tr("More  (F10)"));
    m_menu->setAccessibleName(tr("More"));
    m_menu->setPopupMode(QToolButton::InstantPopup);
    layout->addWidget(m_menu, 0, Qt::AlignHCenter);
    m_labels = makeButton(nullptr, u"sidebar"_s);
    m_labels->setLabel(tr("Hide labels"));
    connect(m_labels, &QToolButton::clicked, this, [this] { Q_EMIT expandRequested(!m_expanded); });
    layout->addWidget(m_labels, 0, Qt::AlignHCenter);
}

QList<RailButton*> SideRail::buttons() const
{
    return {m_logo,      m_back,       m_forward,  m_reload, m_download,
            m_downloads, m_miniPlayer, m_settings, m_menu,   m_labels};
}

QList<RailButton*> SideRail::keyboardOrder() const
{
    return {m_logo,     m_youtube,   m_music,      m_tv,       m_back, m_forward, m_reload,
            m_download, m_downloads, m_miniPlayer, m_settings, m_menu, m_labels};
}

bool SideRail::hasFocusInside() const
{
    const QWidget* focus = QApplication::focusWidget();
    return focus != nullptr && isAncestorOf(focus);
}

void SideRail::focusFirst()
{
    for (RailButton* b : keyboardOrder()) {
        if (b->isVisible() && b->isEnabled()) {
            b->setKeyboardFocus();
            b->setFocus(Qt::ShortcutFocusReason);
            return;
        }
    }
}

/// On a focused rail button: Up / Down walk the rail, Enter activates (Space
/// does already), Esc goes back to the page.
bool SideRail::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress) {
        return QFrame::eventFilter(watched, event);
    }
    auto* button = qobject_cast<RailButton*>(watched);
    if (button == nullptr) {
        return QFrame::eventFilter(watched, event);
    }
    const int key = static_cast<QKeyEvent*>(event)->key();
    if (key == Qt::Key_Escape) {
        Q_EMIT leaveRequested();
        return true;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (button->menu() != nullptr) {
            button->showMenu();
        } else {
            button->click();
        }
        return true;
    }
    if (key == Qt::Key_Up || key == Qt::Key_Down) {
        QList<RailButton*> order;
        for (RailButton* b : keyboardOrder()) {
            if (b->isVisible() && b->isEnabled()) {
                order << b;
            }
        }
        const qsizetype at = order.indexOf(button);
        if (at >= 0 && !order.isEmpty()) {
            const qsizetype next = (at + (key == Qt::Key_Down ? 1 : order.size() - 1)) % order.size();
            RailButton* target = order.at(next);
            target->setKeyboardFocus();
            target->setFocus(key == Qt::Key_Down ? Qt::TabFocusReason : Qt::BacktabFocusReason);
            if (auto* scroll = findChild<QScrollArea*>()) {
                scroll->ensureWidgetVisible(target); // a short window scrolls the rail
            }
        }
        return true;
    }
    return QFrame::eventFilter(watched, event);
}

void SideRail::setExpanded(bool expanded)
{
    if (expanded == m_expanded) {
        return;
    }
    m_expanded = expanded;
    applyWidths();
}

void SideRail::applyWidths()
{
    setFixedWidth(m_expanded ? kExpandedWidth : kRailWidth);
    const int inner = kExpandedWidth - 2 * kSideMargin;
    for (RailButton* b : buttons()) {
        b->setExpanded(m_expanded, inner);
    }
    m_modeSwitch->setFixedWidth(m_expanded ? inner : kButtonSide);
    // With labels the buttons sit kModeInset inside the outline all round; icons only, the
    // 40 px group has room for the inset above and below only.
    const int side = m_expanded ? kModeInset : 1;
    const QMargins m = m_modeSwitch->layout()->contentsMargins();
    m_modeSwitch->layout()->setContentsMargins(side, m.top(), side, m.bottom());
    for (RailButton* b : {m_youtube, m_music, m_tv}) {
        b->setExpanded(m_expanded, inner - 2 * kModeInset - 2); // inside the inset and the border
    }
    for (QFrame* line : m_separators) {
        line->setFixedWidth(m_expanded ? inner - 16 : kSeparatorWidth);
    }
    const QString labelsTip = m_expanded ? tr("Hide labels") : tr("Show labels");
    m_labels->setToolTip(labelsTip);
    m_labels->setAccessibleName(labelsTip);
    placeBadge();
}

void SideRail::setMenu(QMenu* menu)
{
    m_menu->setMenu(menu);
}

void SideRail::showMenu()
{
    m_menu->showMenu();
}

void SideRail::applyIcons()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    // A QToolButton re-syncs icon and text from its default action on every
    // action change (checked, enabled…), so the icon has to live on the action,
    // or a toggle turns the button into its text label.
    const auto setIcon = [](QToolButton* b, const QIcon& icon) {
        if (QAction* action = b->defaultAction(); action != nullptr) {
            action->setIcon(icon);
        }
        b->setIcon(icon);
    };
    for (RailButton* b : buttons()) {
        setIcon(b, icons::themed(b->property("redIcon").toString(), t.text, t.muted));
        b->setColors(t.text, t.muted);
    }
    // The active mode's glyph turns brand red (the action's checked state).
    for (RailButton* b : {m_youtube, m_music, m_tv}) {
        QIcon icon = icons::themed(b->property("redIcon").toString(), t.text, t.muted);
        for (const int size : {16, 20, 24, 32}) {
            for (const qreal dpr : {1.0, 2.0}) {
                icon.addPixmap(icons::pixmap(b->property("redIcon").toString(), t.brand, size, dpr),
                               QIcon::Normal, QIcon::On);
            }
        }
        setIcon(b, icon);
        b->setColors(t.text, t.muted);
    }
    setIcon(m_logo, icons::brand());
    setIcon(m_reload, icons::themed(m_loading ? u"stop"_s : u"reload"_s, t.text, t.muted));
    // Something to download here: the glyph turns brand red, like the active
    // mode's. No filled pill: a solid square was the loudest thing on the rail.
    setIcon(m_download, icons::themed(u"download"_s, m_downloadable ? t.brand : t.text, t.muted));
    m_badge->setStyleSheet(
        u"background:%1;color:%2;border-radius:8px;font-size:10px;font-weight:700;padding:0 4px;"_s.arg(
            t.brand.name(), t.brandText.name()));
}

/// The button says what a click does: download this page, or (nothing to
/// download here) paste a link.
void SideRail::setDownloadable(bool downloadable)
{
    m_downloadable = downloadable;
    m_download->setLabel(downloadable ? tr("Download this") : tr("Download from link"));
    m_actions.download->setToolTip(downloadable
                                       ? withShortcut(tr("Download this page"), m_actions.download)
                                       : withShortcut(tr("Download from a link"), m_actions.downloadUrl));
    applyIcons();
}

void SideRail::setActiveDownloads(int count)
{
    if (count <= 0) {
        m_badge->hide();
        m_downloads->setAccessibleName(tr("Downloads"));
        return;
    }
    m_badge->setText(count > 99 ? u"99+"_s : QString::number(count));
    m_badge->adjustSize();
    m_downloads->setAccessibleName(tr("Downloads, %n active", nullptr, count));
    placeBadge();
    m_badge->show();
    m_badge->raise();
}

void SideRail::placeBadge()
{
    const int x =
        m_expanded ? m_downloads->width() - m_badge->width() - 10 : kButtonSide - m_badge->width() - 1;
    const int y = m_expanded ? (m_downloads->height() - m_badge->height()) / 2 : 1;
    m_badge->move(x, y);
}

void SideRail::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    m_reload->setLabel(loading ? tr("Stop") : QString());
    m_reload->setToolTip(loading ? tr("Stop loading") : withShortcut(tr("Reload"), m_actions.reload));
    applyIcons();
}

void SideRail::setMiniPlayerHint(const QString& hint)
{
    m_miniPlayer->setHint(hint);
}

} // namespace red::ui
