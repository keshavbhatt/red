#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace red::ui {

/// Red's replacement for QMessageBox / QInputDialog: accent badge, title,
/// body, optional muted note and text field, pill buttons. Works modally
/// (`run()`) and asynchronously (`open()` + `finished` → `clickedIndex()`).
class MessageSheet : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(MessageSheet)

public:
    enum class Tone
    {
        Info,     ///< brand accent badge
        Question, ///< brand accent badge
        Warning,  ///< amber badge
        Danger,   ///< red badge, destructive primary
    };
    enum class Role
    {
        Normal,
        Primary,     ///< accent pill, default button
        Destructive, ///< accent pill for an action that removes something
    };

    MessageSheet(QWidget* parent, Tone tone, const QString& title, const QString& body);
    ~MessageSheet() override = default;

    void setNote(const QString& note);
    QLineEdit* addInput(const QString& placeholder, const QString& initial = {});
    QPushButton* addButton(const QString& text, Role role = Role::Normal);

    /// Modal: returns the index of the pressed button, -1 when dismissed.
    int run();
    [[nodiscard]] int clickedIndex() const { return m_clicked; }
    [[nodiscard]] QString inputText() const;

    /// One-liners for the common shapes.
    static void info(QWidget* parent, const QString& title, const QString& body);
    /// True when the user pressed `yesText`.
    static bool confirm(QWidget* parent, Tone tone, const QString& title, const QString& body,
                        const QString& yesText, const QString& noText = {});
    /// Text entry; `ok` false when cancelled.
    static QString askText(QWidget* parent, const QString& title, const QString& body,
                           const QString& placeholder, const QString& initial, const QString& okText,
                           bool* ok);

private:
    QLabel* m_note = nullptr;
    QLineEdit* m_input = nullptr;
    QList<QPushButton*> m_buttons;
    QVBoxLayout* m_text = nullptr;
    QHBoxLayout* m_buttonRow = nullptr;
    int m_clicked = -1;
};

} // namespace red::ui
