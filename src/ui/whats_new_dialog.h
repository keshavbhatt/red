#pragma once

#include <QDialog>
#include <QString>

namespace red::ui {

/// Release notes for the running version, shown once per version on the
/// first start that has not seen them (FEATURES S12). The notes are the
/// running version's section of the bundled CHANGELOG.md.
class WhatsNewDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WhatsNewDialog)

public:
    WhatsNewDialog(const QString& version, const QString& notesMarkdown, QWidget* parent = nullptr);
    ~WhatsNewDialog() override = default;

    /// The bundled changelog's section for `version`; empty when there is none.
    [[nodiscard]] static QString bundledNotes(const QString& version);
};

} // namespace red::ui
