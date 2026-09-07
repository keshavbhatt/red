#include "ui/side_rail.h"

#include "core/theme/theme_service.h"
#include "ui/actions.h"
#include "ui/icons.h"
#include "ui/red_style.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {
constexpr int kRailWidth = 52;
constexpr int kIconSize = 22;
} // namespace

SideRail::SideRail(Actions& actions, core::ThemeService& theme, QWidget* parent)
    : QFrame(parent)
    , m_actions(actions)
    , m_theme(theme)
{
    setProperty("redRail", true);
    setFixedWidth(kRailWidth);
    setupUi();
    applyIcons();
    connect(&m_theme, &core::ThemeService::effectiveSchemeChanged, this,
            [this](Qt::ColorScheme) { applyIcons(); });
}

QToolButton* SideRail::makeButton(QAction* action, const QString& icon)
{
    auto* button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setProperty("redRailButton", true);
    button->setProperty("redIcon", icon);
    button->setIconSize(QSize(kIconSize, kIconSize));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::NoFocus);
    const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
    button->setToolTip(shortcut.isEmpty() ? action->text() : u"%1  (%2)"_s.arg(action->text(), shortcut));
    return button;
}

void SideRail::setupUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 8, 6, 8);
    layout->setSpacing(4);
    layout->setAlignment(Qt::AlignHCenter);

    m_logo = makeButton(m_actions.home, u"logo"_s);
    m_logo->setIconSize(QSize(28, 28));
    m_logo->setToolTip(tr("Home  (%1)").arg(m_actions.home->shortcut().toString(QKeySequence::NativeText)));
    layout->addWidget(m_logo, 0, Qt::AlignHCenter);
    layout->addSpacing(6);

    m_back = makeButton(m_actions.back, u"back"_s);
    m_forward = makeButton(m_actions.forward, u"forward"_s);
    m_reload = makeButton(m_actions.reload, u"reload"_s);
    layout->addWidget(m_back, 0, Qt::AlignHCenter);
    layout->addWidget(m_forward, 0, Qt::AlignHCenter);
    layout->addWidget(m_reload, 0, Qt::AlignHCenter);

    auto* sep1 = new QFrame(this);
    sep1->setProperty("redSeparator", true);
    sep1->setFixedWidth(24);
    layout->addSpacing(4);
    layout->addWidget(sep1, 0, Qt::AlignHCenter);
    layout->addSpacing(4);

    m_download = makeButton(m_actions.download, u"download"_s);
    layout->addWidget(m_download, 0, Qt::AlignHCenter);

    // Downloads button with a count badge overlaid on its corner.
    auto* downloadsHost = new QWidget(this);
    downloadsHost->setFixedSize(40, 40);
    m_downloads = makeButton(m_actions.downloads, u"downloads"_s);
    m_downloads->setParent(downloadsHost);
    m_downloads->move(0, 0);
    m_badge = new QLabel(downloadsHost);
    m_badge->setAlignment(Qt::AlignCenter);
    m_badge->setFixedHeight(16);
    m_badge->setMinimumWidth(16);
    m_badge->move(22, 2);
    m_badge->hide();
    layout->addWidget(downloadsHost, 0, Qt::AlignHCenter);

    m_miniPlayer = makeButton(m_actions.miniPlayer, u"miniplayer"_s);
    m_tv = makeButton(m_actions.tvMode, u"tv"_s);
    layout->addWidget(m_miniPlayer, 0, Qt::AlignHCenter);
    layout->addWidget(m_tv, 0, Qt::AlignHCenter);

    layout->addStretch(1);

    m_settings = makeButton(m_actions.settings, u"settings"_s);
    layout->addWidget(m_settings, 0, Qt::AlignHCenter);
    m_menu = new QToolButton(this);
    m_menu->setProperty("redRailButton", true);
    m_menu->setProperty("redIcon", u"more"_s);
    m_menu->setIconSize(QSize(kIconSize, kIconSize));
    m_menu->setPopupMode(QToolButton::InstantPopup);
    m_menu->setCursor(Qt::PointingHandCursor);
    m_menu->setFocusPolicy(Qt::NoFocus);
    m_menu->setToolTip(tr("More"));
    layout->addWidget(m_menu, 0, Qt::AlignHCenter);
}

void SideRail::setMenu(QMenu* menu)
{
    m_menu->setMenu(menu);
}

void SideRail::applyIcons()
{
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    const QList<QToolButton*> buttons{m_back,       m_forward, m_reload,   m_downloads,
                                      m_miniPlayer, m_tv,      m_settings, m_menu};
    // A QToolButton re-syncs icon and text from its default action on every
    // action change (checked, enabled…) — so the icon has to live on the action,
    // or a toggle turns the button into its text label.
    const auto setIcon = [](QToolButton* b, const QIcon& icon) {
        if (QAction* action = b->defaultAction(); action != nullptr) {
            action->setIcon(icon);
        }
        b->setIcon(icon);
    };
    for (QToolButton* b : buttons) {
        setIcon(b, icons::themed(b->property("redIcon").toString(), t.text, t.muted));
    }
    setIcon(m_logo, icons::brand());
    setIcon(m_reload, icons::themed(m_loading ? u"stop"_s : u"reload"_s, t.text, t.muted));
    setIcon(m_download, icons::themed(u"download"_s, m_downloadable ? t.accentText : t.text, t.muted));
    m_download->setProperty("redAccent", m_downloadable);
    m_download->style()->unpolish(m_download);
    m_download->style()->polish(m_download);
    m_badge->setStyleSheet(
        u"background:%1;color:%2;border-radius:8px;font-size:10px;font-weight:700;padding:0 4px;"_s.arg(
            t.accent.name(), t.accentText.name()));
}

void SideRail::setDownloadable(bool downloadable)
{
    if (m_downloadable == downloadable) {
        return;
    }
    m_downloadable = downloadable;
    m_download->setToolTip(downloadable
                               ? tr("Download this video  (%1)")
                                     .arg(m_actions.download->shortcut().toString(QKeySequence::NativeText))
                               : tr("Open a video, playlist or channel to download it"));
    applyIcons();
}

void SideRail::setActiveDownloads(int count)
{
    if (count <= 0) {
        m_badge->hide();
        return;
    }
    m_badge->setText(count > 99 ? u"99+"_s : QString::number(count));
    m_badge->adjustSize();
    m_badge->move(40 - m_badge->width() - 1, 1);
    m_badge->show();
    m_badge->raise();
}

void SideRail::setLoading(bool loading)
{
    if (m_loading == loading) {
        return;
    }
    m_loading = loading;
    m_reload->setToolTip(loading ? tr("Stop loading") : tr("Reload  (F5)"));
    applyIcons();
}

} // namespace red::ui
