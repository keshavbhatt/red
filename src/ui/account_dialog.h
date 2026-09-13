#pragma once

#include <QDialog>

class QLabel;
class QPushButton;

namespace red::core {
class ThemeService;
}
namespace red::services {
class LicenseService;
}

namespace red::ui {

/// Account & licence: the account ID (with copy), the plan badge (Pro /
/// Evaluation with days left / Free), and the purchase, portal and refresh
/// actions. Follows LicenseService live.
class AccountDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AccountDialog)

public:
    AccountDialog(services::LicenseService& license, const core::ThemeService& theme,
                  QWidget* parent = nullptr);
    ~AccountDialog() override = default;

    /// Draws the eye to the purchase button (after a gate prompt).
    void highlightPurchase();

private:
    void refresh();

    services::LicenseService& m_license;
    const core::ThemeService& m_theme;
    QLabel* m_badge = nullptr;
    QLabel* m_plan = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_id = nullptr;
    QPushButton* m_copy = nullptr;
    QPushButton* m_refresh = nullptr;
    QPushButton* m_portal = nullptr;
    QPushButton* m_purchase = nullptr;
};

} // namespace red::ui
