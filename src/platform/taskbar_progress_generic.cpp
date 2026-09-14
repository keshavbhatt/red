#include "platform/taskbar_progress.h"

namespace red::platform {

namespace {

class NullTaskbarProgress : public TaskbarProgress
{
public:
    using TaskbarProgress::TaskbarProgress;
    void setProgress(double) override {}
    void clear() override {}
};

} // namespace

std::unique_ptr<TaskbarProgress> TaskbarProgress::create(const QString&, QObject* parent)
{
    return std::make_unique<NullTaskbarProgress>(parent);
}

} // namespace red::platform
