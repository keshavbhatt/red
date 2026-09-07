#pragma once

#include <QAbstractButton>
#include <QString>

namespace red::core {
class ThemeService;
}

namespace red::ui {

/// One selectable card of a segmented choice (icon, title, one-line hint).
/// Exclusive selection comes from the QButtonGroup the owner puts it in.
class KindCard : public QAbstractButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(KindCard)

public:
    KindCard(const QString& icon, const QString& title, const QString& hint, core::ThemeService& theme,
             QWidget* parent = nullptr);
    ~KindCard() override;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    QString m_icon;
    QString m_hint;
    core::ThemeService& m_theme;
    bool m_hover = false;
};

} // namespace red::ui
