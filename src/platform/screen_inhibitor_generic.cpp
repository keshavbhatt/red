#include "platform/screen_inhibitor.h"

namespace red::platform {

namespace {

class NullInhibitor : public ScreenInhibitor
{
public:
    using ScreenInhibitor::ScreenInhibitor;
    void setInhibited(bool, const QString&) override {}
    [[nodiscard]] bool isInhibited() const override { return false; }
};

} // namespace

std::unique_ptr<ScreenInhibitor> ScreenInhibitor::create(QObject* parent)
{
    return std::make_unique<NullInhibitor>(parent);
}

} // namespace red::platform
