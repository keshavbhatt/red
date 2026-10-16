#include "ui/message_sheet.h"

#include "ui/icons.h"
#include "ui/red_style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {

QString iconFor(MessageSheet::Tone tone)
{
    switch (tone) {
    case MessageSheet::Tone::Warning:
    case MessageSheet::Tone::Danger:
        return u"warning"_s;
    case MessageSheet::Tone::Question:
        return u"shield"_s;
    case MessageSheet::Tone::Info:
        break;
    }
    return u"info"_s;
}

} // namespace

MessageSheet::MessageSheet(QWidget* parent, Tone tone, const QString& title, const QString& body)
    : QDialog(parent)
{
    // Dark or light follows the palette the parent already carries.
    const bool dark = parent != nullptr && parent->palette().window().color().lightness() < 128;
    const Tokens t = Tokens::forScheme(dark);
    setModal(true);
    setWindowTitle(title);
    setMinimumWidth(440);
    setMaximumWidth(560);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);
    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* badge = new QLabel(this);
    badge->setFixedSize(44, 44);
    badge->setAlignment(Qt::AlignCenter);
    const QColor badgeColor = tone == Tone::Warning ? t.warning : t.brand;
    badge->setStyleSheet(u"background:%1;border-radius:22px;"_s.arg(badgeColor.name()));
    badge->setPixmap(icons::pixmap(iconFor(tone), t.brandText, 24, devicePixelRatioF()));
    top->addWidget(badge, 0, Qt::AlignTop);
    m_text = new QVBoxLayout;
    m_text->setSpacing(6);
    auto* heading = new QLabel(title, this);
    heading->setProperty("redTitle", true);
    heading->setWordWrap(true);
    m_text->addWidget(heading);
    auto* bodyLabel = new QLabel(body, this);
    bodyLabel->setWordWrap(true);
    bodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_text->addWidget(bodyLabel);
    top->addLayout(m_text, 1);
    root->addLayout(top);

    m_buttonRow = new QHBoxLayout;
    m_buttonRow->addStretch(1);
    root->addLayout(m_buttonRow);
}

void MessageSheet::setNote(const QString& note)
{
    if (m_note == nullptr) {
        m_note = new QLabel(this);
        m_note->setProperty("redMuted", true);
        m_note->setWordWrap(true);
        m_text->addWidget(m_note);
    }
    m_note->setText(note);
}

QLineEdit* MessageSheet::addInput(const QString& placeholder, const QString& initial)
{
    m_input = new QLineEdit(initial, this);
    m_input->setPlaceholderText(placeholder);
    m_input->selectAll();
    m_text->addSpacing(4);
    m_text->addWidget(m_input);
    connect(m_input, &QLineEdit::returnPressed, this, [this] {
        for (QPushButton* b : m_buttons) {
            if (b->isDefault()) {
                b->click();
                return;
            }
        }
    });
    return m_input;
}

QPushButton* MessageSheet::addButton(const QString& text, Role role)
{
    auto* button = new QPushButton(text, this);
    const int index = static_cast<int>(m_buttons.size());
    if (role != Role::Normal) {
        button->setProperty("redPrimary", true);
        button->setDefault(true);
    }
    connect(button, &QPushButton::clicked, this, [this, index] {
        m_clicked = index;
        accept();
    });
    m_buttons.append(button);
    m_buttonRow->addWidget(button);
    return button;
}

int MessageSheet::run()
{
    if (m_input != nullptr) {
        m_input->setFocus();
    }
    exec();
    return m_clicked;
}

QString MessageSheet::inputText() const
{
    return m_input != nullptr ? m_input->text() : QString();
}

void MessageSheet::info(QWidget* parent, const QString& title, const QString& body)
{
    MessageSheet sheet(parent, Tone::Info, title, body);
    sheet.addButton(QObject::tr("OK"), Role::Primary);
    sheet.run();
}

bool MessageSheet::confirm(QWidget* parent, Tone tone, const QString& title, const QString& body,
                           const QString& yesText, const QString& noText)
{
    MessageSheet sheet(parent, tone, title, body);
    sheet.addButton(noText.isEmpty() ? QObject::tr("Cancel") : noText);
    sheet.addButton(yesText, tone == Tone::Danger ? Role::Destructive : Role::Primary);
    return sheet.run() == 1;
}

QString MessageSheet::askText(QWidget* parent, const QString& title, const QString& body,
                              const QString& placeholder, const QString& initial, const QString& okText,
                              bool* ok)
{
    MessageSheet sheet(parent, Tone::Info, title, body);
    sheet.addInput(placeholder, initial);
    sheet.addButton(QObject::tr("Cancel"));
    sheet.addButton(okText, Role::Primary);
    const bool accepted = sheet.run() == 1;
    if (ok != nullptr) {
        *ok = accepted;
    }
    return accepted ? sheet.inputText() : QString();
}

} // namespace red::ui
