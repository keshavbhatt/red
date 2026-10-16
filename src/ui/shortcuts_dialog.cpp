#include "ui/shortcuts_dialog.h"

#include "core/theme/theme_service.h"
#include "ui/actions.h"
#include "ui/red_style.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {

QString keysOf(const QAction* action)
{
    // Qt's names for the media keys are a mouthful.
    QString keys = action->shortcut().toString(QKeySequence::NativeText);
    keys.replace(u"Toggle Media Play/Pause"_s, QObject::tr("Play/Pause key"));
    keys.replace(u"Media Play"_s, QObject::tr("Play key"));
    keys.replace(u"Media Next"_s, QObject::tr("Next key"));
    keys.replace(u"Media Previous"_s, QObject::tr("Previous key"));
    return keys;
}

QString labelOf(const QAction* action)
{
    QString text = action->text();
    text.remove(u'&');
    text.remove(u'…');
    return text.trimmed();
}

} // namespace

ShortcutsDialog::ShortcutsDialog(const Actions& a, const core::ThemeService& theme, QWidget* parent)
    : QDialog(parent)
    , m_theme(theme)
{
    setWindowTitle(tr("Keyboard shortcuts"));
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(16);
    auto* heading = new QLabel(tr("Keyboard shortcuts"), this);
    heading->setProperty("redHeading", true);
    root->addWidget(heading);

    // Two independent columns: rows must not share heights across columns.
    auto* columns = new QHBoxLayout;
    columns->setSpacing(36);
    auto* left = new QVBoxLayout;
    auto* right = new QVBoxLayout;
    left->setSpacing(0);
    right->setSpacing(0);
    columns->addLayout(left, 1);
    columns->addLayout(right, 1);
    auto rows = [](std::initializer_list<const QAction*> actions) {
        QList<Row> out;
        for (const QAction* action : actions) {
            if (!action->shortcut().isEmpty()) {
                out.append({labelOf(action), keysOf(action)});
            }
        }
        return out;
    };
    // Left column: getting around, modes and the window. Right: downloads, media and the app.
    addGroup(left, tr("Navigate"), rows({a.home, a.back, a.forward, a.reload, a.copyLink}));
    addGroup(left, tr("Mode"),
             rows({a.youtubeMode, a.musicMode, a.tvMode}) +
                 QList<Row>{{tr("TV mode on / off"), u"Ctrl+T"_s}, {tr("Music mode on / off"), u"Ctrl+M"_s}});
    addGroup(left, tr("View"),
             rows({a.miniPlayer, a.fullScreen, a.zoomIn, a.zoomOut, a.zoomReset, a.showHide}));
    addGroup(right, tr("Downloads"), rows({a.download, a.downloadUrl, a.downloads}));
    addGroup(right, tr("Playback"), rows({a.playPause, a.next, a.previous}));
    addGroup(right, tr("App"), rows({a.settings, a.shortcuts, a.quit}));
    // YouTube's own player keys work as long as the page has focus.
    addGroup(right, tr("On the page (YouTube)"),
             {{tr("Play / pause"), u"K"_s},
              {tr("Seek 10 s"), u"J, L"_s},
              {tr("Mute"), u"M"_s},
              {tr("Captions"), u"C"_s},
              {tr("Player full screen"), u"F"_s}});
    left->addStretch(1);
    right->addStretch(1);
    root->addLayout(columns);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* close = new QPushButton(tr("Close"), this);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(close);
    root->addLayout(buttons);
    setMinimumWidth(620);
}

void ShortcutsDialog::addGroup(QVBoxLayout* column, const QString& title, const QList<Row>& rows)
{
    if (rows.isEmpty()) {
        return;
    }
    auto* section = new QLabel(title, this);
    section->setProperty("redSection", true);
    section->setContentsMargins(0, column->count() == 0 ? 0 : 18, 0, 6);
    column->addWidget(section);
    for (const Row& r : rows) {
        auto* line = new QWidget(this);
        auto* h = new QHBoxLayout(line);
        h->setContentsMargins(0, 2, 0, 2);
        line->setFixedHeight(28);
        h->setSpacing(12);
        auto* label = new QLabel(r.label, line);
        h->addWidget(label, 1);
        h->addWidget(makeKeys(r.keys), 0, Qt::AlignRight);
        column->addWidget(line);
    }
}

QWidget* ShortcutsDialog::makeKeys(const QString& keys)
{
    // "Ctrl+Shift+M" → [Ctrl] [Shift] [M]; "J, L" → [J] [L]. The key caps are
    // styled like the ones on the page: raised, monospace, a shade lighter.
    const Tokens t = Tokens::forScheme(m_theme.isDark());
    auto* box = new QWidget(this);
    auto* h = new QHBoxLayout(box);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    QStringList caps;
    for (const QString& chord : keys.split(u", "_s)) {
        // Keep "+" itself as a key ("Ctrl++" is Ctrl and Plus).
        QString rest = chord;
        while (!rest.isEmpty()) {
            const qsizetype plus = rest.indexOf(u'+', 1);
            if (plus < 0) {
                caps << rest;
                break;
            }
            caps << rest.left(plus);
            rest = rest.mid(plus + 1);
            if (rest.isEmpty()) {
                caps << u"+"_s;
            }
        }
    }
    for (const QString& cap : caps) {
        auto* key = new QLabel(cap, box);
        key->setAlignment(Qt::AlignCenter);
        key->setMinimumWidth(26);
        key->setFixedHeight(22);
        key->setStyleSheet(u"QLabel{background:%1;color:%2;border:1px solid %3;border-bottom:2px solid %3;"
                           "border-radius:5px;padding:1px 6px;font-weight:600;font-size:12px;}"_s.arg(
                               t.elevated.name(), t.text.name(), t.border.name()));
        h->addWidget(key);
    }
    return box;
}

} // namespace red::ui
