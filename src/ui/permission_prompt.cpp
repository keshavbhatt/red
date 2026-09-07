#include "ui/permission_prompt.h"

#include "ui/message_sheet.h"
#include "web/permission_controller.h"

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
    const QString origin = permission.origin().host();
    auto* sheet =
        new MessageSheet(parent, MessageSheet::Tone::Question, titleFor(permission.permissionType()),
                         QObject::tr("%1 wants to %2.")
                             .arg(origin.isEmpty() ? u"YouTube"_s : origin,
                                  web::PermissionController::describe(permission.permissionType())));
    sheet->setAttribute(Qt::WA_DeleteOnClose);
    sheet->setNote(
        QObject::tr("Allow and Don't allow are remembered; Settings → Advanced → Storage can reset "
                    "them. Closing this asks again next time."));
    sheet->addButton(QObject::tr("Don't allow"));
    sheet->addButton(QObject::tr("Allow"), MessageSheet::Role::Primary);
    // Exactly one answer: a button is remembered, dismissing is "not now".
    QObject::connect(sheet, &QDialog::finished, sheet, [sheet, answer = std::move(answer)] {
        const int index = sheet->clickedIndex();
        answer(index == 1, index >= 0);
    });
    sheet->open();
}

} // namespace red::ui
