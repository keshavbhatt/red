#pragma once

#include <QDialog>

class QVBoxLayout;

namespace red::core {
class ThemeService;
}

namespace red::ui {

class Actions;

/// Read-only list of keyboard shortcuts, generated from the shared Actions so
/// it can never drift from the real bindings (FEATURES S26). Grouped by what
/// the keys do, with the page's own player keys listed for completeness.
class ShortcutsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ShortcutsDialog)

public:
    ShortcutsDialog(const Actions& actions, const core::ThemeService& theme, QWidget* parent = nullptr);
    ~ShortcutsDialog() override = default;

private:
    struct Row
    {
        QString label;
        QString keys; ///< native text, "+"-separated chords
    };
    void addGroup(QVBoxLayout* column, const QString& title, const QList<Row>& rows);
    [[nodiscard]] QWidget* makeKeys(const QString& keys);

    const core::ThemeService& m_theme;
};

} // namespace red::ui
