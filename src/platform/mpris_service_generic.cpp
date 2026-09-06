#include "platform/mpris_service.h"

namespace red::platform {

namespace {

class NullMpris : public MprisService
{
public:
    using MprisService::MprisService;
    void setMediaState(const core::MediaState&) override {}
    [[nodiscard]] bool isRegistered() const override { return false; }
};

} // namespace

std::unique_ptr<MprisService> MprisService::create(const QString&, const QString&, QObject* parent)
{
    return std::make_unique<NullMpris>(parent);
}

} // namespace red::platform
