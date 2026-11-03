#pragma once

#include "core/changelog.h"

#include <QDialog>
#include <QList>
#include <QString>

class QComboBox;
class QLabel;
class QTextBrowser;

namespace red::ui {

/// Release notes from the bundled CHANGELOG.md (FEATURES S12): shown once per version on the
/// first start that has not seen them, and on demand from About. A Version picker browses every
/// release in the changelog; it opens on the running version.
class WhatsNewDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WhatsNewDialog)

public:
    explicit WhatsNewDialog(const QString& runningVersion, QWidget* parent = nullptr);
    ~WhatsNewDialog() override = default;

    /// The bundled changelog (:/text/CHANGELOG.md).
    [[nodiscard]] static QString bundledChangelog();
    /// The bundled changelog's section for `version`; empty when there is none.
    [[nodiscard]] static QString bundledNotes(const QString& version);

    /// Shows `version`'s notes; ignored when the changelog has no such release.
    void showVersion(const QString& version);
    [[nodiscard]] QString shownVersion() const;

private:
    QString m_running;
    QString m_markdown;
    QList<core::ChangelogRelease> m_releases;
    QLabel* m_title = nullptr;
    QLabel* m_subtitle = nullptr;
    QComboBox* m_picker = nullptr;
    QTextBrowser* m_notes = nullptr;
};

} // namespace red::ui
