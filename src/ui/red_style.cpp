#include "ui/red_style.h"

#include "ui/icons.h"

#include <QHash>

using namespace Qt::StringLiterals;

namespace red::ui {

Tokens Tokens::forScheme(bool dark)
{
    Tokens t;
    if (dark) {
        t.accent = QColor(0xFF, 0x00, 0x00);
        t.accentHover = QColor(0xFF, 0x3D, 0x3D);
        t.accentText = QColor(0xFF, 0xFF, 0xFF);
        t.bg = QColor(0x0F, 0x0F, 0x0F);
        t.rail = QColor(0x0F, 0x0F, 0x0F);
        t.panel = QColor(0x1F, 0x1F, 0x1F);
        t.elevated = QColor(0x28, 0x28, 0x28);
        t.hover = QColor(0x3F, 0x3F, 0x3F);
        t.input = QColor(0x12, 0x12, 0x12);
        t.border = QColor(0x30, 0x30, 0x30);
        t.text = QColor(0xF1, 0xF1, 0xF1);
        t.muted = QColor(0xAA, 0xAA, 0xAA);
        t.link = QColor(0x3E, 0xA6, 0xFF);
        t.success = QColor(0x2B, 0xA6, 0x40);
        t.warning = QColor(0xF9, 0xAB, 0x00);
        t.danger = QColor(0xFF, 0x4E, 0x45);
        return t;
    }
    t.accent = QColor(0xFF, 0x00, 0x00);
    t.accentHover = QColor(0xE6, 0x00, 0x00);
    t.accentText = QColor(0xFF, 0xFF, 0xFF);
    t.bg = QColor(0xFF, 0xFF, 0xFF);
    t.rail = QColor(0xFF, 0xFF, 0xFF);
    t.panel = QColor(0xF9, 0xF9, 0xF9);
    t.elevated = QColor(0xFF, 0xFF, 0xFF);
    t.hover = QColor(0xE5, 0xE5, 0xE5);
    t.input = QColor(0xFF, 0xFF, 0xFF);
    t.border = QColor(0xE0, 0xE0, 0xE0);
    t.text = QColor(0x0F, 0x0F, 0x0F);
    t.muted = QColor(0x60, 0x60, 0x60);
    t.link = QColor(0x06, 0x5F, 0xD4);
    t.success = QColor(0x1E, 0x8E, 0x3E);
    t.warning = QColor(0xE3, 0x74, 0x00);
    t.danger = QColor(0xD9, 0x30, 0x25);
    return t;
}

namespace {

// The sheet, with {{token}} placeholders. Backgrounds are set only on the
// containers and controls that need the Red look; the palette handles the
// rest, so nothing paints an opaque block over the web view.
const char* kSheet = R"qss(
QDialog, QMainWindow { background: {{bg}}; }
QWidget { color: {{text}}; }
QLabel, QCheckBox, QRadioButton, QGroupBox, QTabWidget, QTabBar { background: transparent; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }
QToolTip {
    background: {{elevated}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 6px; padding: 5px 8px;
}
QStatusBar { background: {{bg}}; border-top: 1px solid {{border}}; }
QLabel[redMuted="true"] { color: {{muted}}; }
QLabel[redTitle="true"] { font-size: 16px; font-weight: 600; }
QLabel[redHeading="true"] { font-size: 20px; font-weight: 600; }
QLabel[redSection="true"] { font-size: 12px; font-weight: 600; color: {{muted}}; text-transform: uppercase; letter-spacing: 1px; }
QLabel[redLink="true"] { color: {{link}}; }

/* Cards */
QFrame[redCard="true"] {
    background: {{panel}}; border: 1px solid {{border}}; border-radius: 12px;
}
QFrame[redPanel="true"] { background: {{panel}}; border-left: 1px solid {{border}}; }
QFrame[redRail="true"] { background: {{rail}}; border-right: 1px solid {{border}}; }
QFrame[redSeparator="true"] { background: {{border}}; max-height: 1px; min-height: 1px; border: none; }
QFrame[redVSeparator="true"] { background: {{border}}; max-width: 1px; min-width: 1px; border: none; }

/* Tabs: YouTube underlines the active section. */
QTabWidget::pane { border: none; background: transparent; }
QTabBar::tab {
    background: transparent; color: {{muted}};
    padding: 8px 16px 9px 16px; margin-right: 2px;
    border: none; border-bottom: 2px solid transparent; font-weight: 600;
}
QTabBar::tab:hover { color: {{text}}; }
QTabBar::tab:selected { color: {{text}}; border-bottom: 2px solid {{text}}; }

/* Grouping panels. */
QGroupBox {
    background: {{panel}}; border: 1px solid {{border}}; border-radius: 12px;
    margin-top: 18px; padding: 18px 14px 14px 14px; font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin; subcontrol-position: top left;
    left: 12px; padding: 0 4px; color: {{muted}};
}

/* Text inputs, combos and spin boxes share the rounded shape. */
QLineEdit, QComboBox, QAbstractSpinBox, QPlainTextEdit, QTextEdit {
    background: {{input}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 8px;
    padding: 6px 10px; min-height: 22px;
    selection-background-color: {{accent}}; selection-color: #ffffff;
}
QLineEdit:hover, QComboBox:hover, QAbstractSpinBox:hover { border: 1px solid {{muted}}; }
QLineEdit:focus, QComboBox:focus, QAbstractSpinBox:focus { border: 1px solid {{text}}; }
QLineEdit:read-only { color: {{muted}}; }
QLineEdit:disabled, QComboBox:disabled, QAbstractSpinBox:disabled { color: {{muted}}; border-color: {{border}}; }
QComboBox::drop-down, QComboBox::drop-down:editable {
    subcontrol-origin: padding; subcontrol-position: center right;
    width: 26px; border: none; background: transparent;
}
QComboBox::down-arrow { image: url({{chevronDown}}); width: 14px; height: 14px; }
/* List-style popup: the menu-style one shows every row and overflows the screen
   when the list is long (subtitle languages); this one scrolls past maxVisibleItems. */
QComboBox { combobox-popup: 0; }
QComboBox QAbstractItemView {
    background: {{elevated}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 8px; padding: 4px;
    selection-background-color: {{hover}}; selection-color: {{text}}; outline: none;
}
QComboBox QAbstractItemView::item { padding: 6px 8px; border-radius: 6px; min-height: 22px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { width: 18px; border: none; background: transparent; }
QAbstractSpinBox::up-arrow { image: url({{chevronUp}}); width: 10px; height: 10px; }
QAbstractSpinBox::down-arrow { image: url({{chevronDown}}); width: 10px; height: 10px; }

/* Buttons: neutral pills; redPrimary turns one brand red; redFlat is icon-only. */
QPushButton {
    background: {{elevated}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 18px;
    padding: 7px 18px; min-height: 22px; font-weight: 600;
}
QPushButton:hover { background: {{hover}}; }
QPushButton:pressed { background: {{border}}; }
QPushButton:disabled { color: {{muted}}; background: transparent; }
QPushButton[redPrimary="true"] { background: {{accent}}; color: {{accentText}}; border: 1px solid {{accent}}; }
QPushButton[redPrimary="true"]:hover { background: {{accentHover}}; border-color: {{accentHover}}; }
QPushButton[redPrimary="true"]:disabled { background: {{hover}}; color: {{muted}}; border-color: transparent; }
QPushButton[redFlat="true"], QToolButton[redFlat="true"] {
    background: transparent; border: none; border-radius: 8px; padding: 6px 10px; min-height: 22px;
}
QPushButton[redFlat="true"]:hover, QToolButton[redFlat="true"]:hover { background: {{hover}}; }
QPushButton[redFlat="true"]:pressed, QToolButton[redFlat="true"]:pressed { background: {{border}}; }
/* Release notes in the What's new sheet: plain text on the card, not an input box. */
QTextBrowser[redNotes="true"] { background: transparent; border: none; padding: 0; }
QToolButton { background: transparent; border: none; border-radius: 8px; padding: 4px; }
QToolButton:hover { background: {{hover}}; }
QToolButton:checked { background: {{hover}}; }
QToolButton::menu-indicator { image: none; }

/* Rail buttons: square, rounded, the active one carries the accent bar. */
/* Sizes come from ui::RailButton: 40 px square, or the rail's width with labels. */
QToolButton[redRailButton="true"] {
    background: transparent; border: none; border-radius: 10px; padding: 0; margin: 0;
}
QToolButton[redRailButton="true"]::menu-indicator { image: none; width: 0; }
QToolButton[redRailButton="true"]:hover { background: {{hover}}; }
QToolButton[redRailButton="true"]:checked { background: {{hover}}; }
QToolButton[redRailButton="true"][redAccent="true"] { background: {{accent}}; }
QToolButton[redRailButton="true"][redAccent="true"]:hover { background: {{accentHover}}; }
QToolButton[redRailButton="true"]:disabled { background: transparent; }
/* The mode switch: three smaller rail buttons in one outlined group. */
QFrame[redModeSwitch="true"] { border: 1px solid {{border}}; border-radius: 12px; background: transparent; }
QToolButton[redRailButton="true"][redModeButton="true"] { border-radius: 10px; }

/* Check boxes and radios. */
QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator, QRadioButton::indicator { width: 18px; height: 18px; }
QCheckBox::indicator {
    border: 2px solid {{muted}}; border-radius: 4px; background: transparent;
}
QCheckBox::indicator:hover { border-color: {{text}}; }
QCheckBox::indicator:checked { background: {{accent}}; border-color: {{accent}}; image: url({{checkWhite}}); }
QCheckBox::indicator:disabled { border-color: {{border}}; }
QCheckBox::indicator:checked:disabled { background: {{hover}}; border-color: {{hover}}; }
/* Both radio states add up to 18px: QSS sizes the indicator per state and a
   toggle would otherwise shift the label. */
QRadioButton::indicator { width: 14px; height: 14px; border: 2px solid {{muted}}; border-radius: 9px; background: transparent; }
QListView::indicator, QTreeView::indicator, QTableView::indicator {
    width: 16px; height: 16px; border: 2px solid {{muted}}; border-radius: 4px; background: transparent;
}
QListView::indicator:hover, QTreeView::indicator:hover { border-color: {{text}}; }
QListView::indicator:checked, QTreeView::indicator:checked, QTableView::indicator:checked {
    background: {{accent}}; border-color: {{accent}}; image: url({{checkWhite}});
}
QRadioButton::indicator:hover { border-color: {{text}}; }
QRadioButton::indicator:checked { border: 6px solid {{accent}}; background: {{accentText}}; width: 6px; height: 6px; }

/* Menus. */
QMenu {
    background: {{elevated}}; color: {{text}};
    border: 1px solid {{border}}; border-radius: 10px; padding: 6px;
}
QMenu::item { padding: 7px 28px 7px 12px; border-radius: 6px; }
QMenu::item:selected { background: {{hover}}; }
QMenu::item:disabled { color: {{muted}}; }
QMenu::separator { height: 1px; background: {{border}}; margin: 6px 4px; }
QMenu::icon { padding-left: 6px; }
QMenu::indicator { width: 16px; height: 16px; margin-left: 6px; }
QMenu::indicator:checked { image: url({{checkText}}); }

/* Scrollbars: thin, rounded. */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: {{hover}}; border-radius: 3px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: {{muted}}; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: {{hover}}; border-radius: 3px; min-width: 30px; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }

/* Lists / tables. */
QListView, QTreeView, QTableView, QListWidget, QTreeWidget, QTableWidget {
    background: transparent; border: none; outline: none;
    selection-background-color: {{hover}}; selection-color: {{text}};
    alternate-background-color: transparent;
}
QListView::item, QTreeView::item { padding: 4px; border-radius: 6px; }
QListView::item:selected, QTreeView::item:selected, QTableView::item:selected { background: {{hover}}; color: {{text}}; }
QListView::item:hover, QTreeView::item:hover { background: {{hover}}; }
QHeaderView::section {
    background: transparent; color: {{muted}}; border: none; border-bottom: 1px solid {{border}};
    padding: 6px 8px; font-weight: 600;
}
QListWidget[redNav="true"] { background: {{bg}}; padding: 8px; }
QListWidget[redNav="true"]::item { padding: 9px 12px; border-radius: 8px; margin: 1px 0; }
QListWidget[redNav="true"]::item:selected { background: {{hover}}; font-weight: 600; }

/* Progress bars. */
QProgressBar {
    background: {{hover}}; border: none; border-radius: 3px; min-height: 6px; max-height: 6px; text-align: center;
}
QProgressBar::chunk { background: {{accent}}; border-radius: 3px; }

/* Sliders. */
QSlider::groove:horizontal { height: 4px; background: {{hover}}; border-radius: 2px; }
QSlider::sub-page:horizontal { background: {{accent}}; border-radius: 2px; }
QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: {{text}}; border-radius: 7px; }

/* Splitter handle between the page and the downloads panel. */
QSplitter::handle { background: {{border}}; }
QSplitter::handle:horizontal { width: 1px; }

/* Message boxes. */
QMessageBox { background: {{panel}}; }
QMessageBox QLabel { min-width: 320px; }
)qss";

QHash<QString, QString> tokenMap(const Tokens& t)
{
    return {
        {u"accent"_s, t.accent.name()},
        {u"accentHover"_s, t.accentHover.name()},
        {u"accentText"_s, t.accentText.name()},
        {u"bg"_s, t.bg.name()},
        {u"rail"_s, t.rail.name()},
        {u"panel"_s, t.panel.name()},
        {u"elevated"_s, t.elevated.name()},
        {u"hover"_s, t.hover.name()},
        {u"input"_s, t.input.name()},
        {u"border"_s, t.border.name()},
        {u"text"_s, t.text.name()},
        {u"muted"_s, t.muted.name()},
        {u"link"_s, t.link.name()},
        {u"success"_s, t.success.name()},
        {u"warning"_s, t.warning.name()},
        {u"danger"_s, t.danger.name()},
    };
}

} // namespace

QString redStyleSheet(bool dark)
{
    QString sheet = QString::fromLatin1(kSheet);
    const Tokens t = Tokens::forScheme(dark);
    QHash<QString, QString> map = tokenMap(t);
    // Style sheets can only reference files, so the tinted glyphs are written
    // to the cache directory once per colour.
    map.insert(u"chevronDown"_s, icons::tintedFile(u"chevron-down"_s, t.muted));
    map.insert(u"chevronUp"_s, icons::tintedFile(u"chevron-up"_s, t.muted));
    map.insert(u"checkWhite"_s, icons::tintedFile(u"check"_s, t.accentText));
    map.insert(u"checkText"_s, icons::tintedFile(u"check"_s, t.text));
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        sheet.replace(u"{{"_s + it.key() + u"}}"_s, it.value());
    }
    return sheet;
}

} // namespace red::ui
