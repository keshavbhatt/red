#pragma once

#include <QDialog>

namespace red::core {
class ThemeService;
}
namespace red::services {
class LicenseService;
}

namespace red::ui {

/// Free / Evaluation / Pro side by side: which features each plan includes,
/// with the user's current plan marked. Opened from the account sheet.
class PlansDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PlansDialog)

public:
    PlansDialog(const services::LicenseService& license, const core::ThemeService& theme,
                QWidget* parent = nullptr);
    ~PlansDialog() override = default;
};

} // namespace red::ui
