#pragma once

#include <QDialog>

namespace red::ui {

/// The first start of a new install: what Red adds to YouTube and where it
/// lives (the modes, downloads, the mini player, blocking), in four short
/// rows. Shown once instead of the release notes; "Show me around" in the
/// rail's menu opens it again.
class WelcomeDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WelcomeDialog)

public:
    explicit WelcomeDialog(QWidget* parent = nullptr);
    ~WelcomeDialog() override = default;

Q_SIGNALS:
    void shortcutsRequested();
};

} // namespace red::ui
