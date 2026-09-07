#include "ui/permission_prompt.h"

#include "ui/icons.h"
#include "ui/red_style.h"
#include "web/permission_controller.h"

#include <QDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace red::ui {

namespace {

QString titleFor(QWebEnginePermission::PermissionType type)
{
    using T = QWebEnginePermission::PermissionType;
    switch (type) {
    case T::MediaAudioCapture:
        return QObject::tr("Use your microphone?");
    case T::MediaVideoCapture:
        return QObject::tr("Use your camera?");
    case T::MediaAudioVideoCapture:
        return QObject::tr("Use your camera and microphone?");
    case T::Geolocation:
        return QObject::tr("Know your location?");
    case T::Notifications:
        return QObject::tr("Show notifications?");
    default:
        return QObject::tr("Allow this?");
    }
}

} // namespace

void askPermission(QWidget* parent, const QWebEnginePermission& permission,
                   std::function<void(bool, bool)> answer)
{
    // The dialog inherits the app palette; dark or light follows from it.
    const bool dark = parent != nullptr && parent->palette().window().color().lightness() < 128;
    const Tokens t = Tokens::forScheme(dark);

    auto* dialog = new QDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setModal(true);
    dialog->setWindowTitle(QObject::tr("Permission request"));
    dialog->setMinimumWidth(440);

    auto* root = new QVBoxLayout(dialog);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);

    auto* top = new QHBoxLayout;
    top->setSpacing(16);
    auto* icon = new QLabel(dialog);
    icon->setFixedSize(44, 44);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(u"background:%1;border-radius:22px;"_s.arg(t.accent.name()));
    icon->setPixmap(icons::pixmap(u"shield"_s, t.accentText, 24, dialog->devicePixelRatioF()));
    top->addWidget(icon, 0, Qt::AlignTop);
    auto* text = new QVBoxLayout;
    text->setSpacing(6);
    auto* title = new QLabel(titleFor(permission.permissionType()), dialog);
    title->setProperty("redTitle", true);
    text->addWidget(title);
    const QString origin = permission.origin().host();
    auto* body = new QLabel(QObject::tr("%1 wants to %2.")
                                .arg(origin.isEmpty() ? u"YouTube"_s : origin,
                                     web::PermissionController::describe(permission.permissionType())),
                            dialog);
    body->setWordWrap(true);
    text->addWidget(body);
    auto* note = new QLabel(
        QObject::tr("Allow and Don't allow are remembered; Settings → Advanced → Storage can reset them. "
                    "Closing this asks again next time."),
        dialog);
    note->setProperty("redMuted", true);
    note->setWordWrap(true);
    text->addWidget(note);
    top->addLayout(text, 1);
    root->addLayout(top);

    auto* buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto* deny = new QPushButton(QObject::tr("Don't allow"), dialog);
    auto* allow = new QPushButton(QObject::tr("Allow"), dialog);
    allow->setProperty("redPrimary", true);
    allow->setDefault(true);
    buttons->addWidget(deny);
    buttons->addWidget(allow);
    root->addLayout(buttons);

    // Exactly one answer: dismissing the sheet is "not now", not a stored no.
    auto* answered = new bool(false);
    auto reply = [dialog, answer = std::move(answer), answered](bool allowIt, bool remember) {
        if (!*answered) {
            *answered = true;
            answer(allowIt, remember);
        }
        dialog->close();
    };
    QObject::connect(allow, &QPushButton::clicked, dialog, [reply] { reply(true, true); });
    QObject::connect(deny, &QPushButton::clicked, dialog, [reply] { reply(false, true); });
    QObject::connect(dialog, &QDialog::finished, dialog, [reply] { reply(false, false); });
    QObject::connect(dialog, &QObject::destroyed, [answered] { delete answered; });
    dialog->show();
}

} // namespace red::ui
