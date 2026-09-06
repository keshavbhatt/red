#include "core/chromium_flags.h"

#include "core/settings/settings.h"

#include <QHash>

using namespace Qt::StringLiterals;

namespace red::core {

bool useSoftwareGpu(HardwareAcceleration acceleration, bool autoDisabled)
{
    switch (acceleration) {
    case HardwareAcceleration::On:
        return false;
    case HardwareAcceleration::Off:
        return true;
    case HardwareAcceleration::Auto:
        break;
    }
    return autoDisabled;
}

QStringList chromiumFlags(HardwareAcceleration acceleration, bool gpuAutoDisabled, bool hardwareVideoDecode)
{
    QStringList flags{
        u"--disable-translate"_s,
        u"--disable-extensions"_s,
        u"--disable-component-update"_s,
        u"--disable-default-apps"_s,
        // YouTube's player registers a media session; keep Chromium's own
        // handling out of the way so Red's MPRIS service (ADR-007) is the one
        // the desktop sees.
        u"--disable-features=HardwareMediaKeyHandling,MediaSessionService"_s,
        // Let audio keep flowing while the window is hidden (FEATURES D6/D7).
        u"--autoplay-policy=no-user-gesture-required"_s,
    };
    QStringList features;
#ifdef Q_OS_LINUX
    if (hardwareVideoDecode) {
        // Opt-in VA-API decode. Off by default: hardware decode hands the
        // compositor dmabuf-backed NV12 frames that some Mesa/Wayland stacks
        // (notably under snap confinement) cannot import, which blanks the
        // video (observed in whatsie, W-ADR-032 revision).
        features << u"VaapiVideoDecodeLinuxGL"_s << u"VaapiVideoDecoder"_s
                 << u"AcceleratedVideoDecodeLinuxGL"_s;
        flags << u"--enable-accelerated-video-decode"_s;
    } else {
        flags << u"--disable-accelerated-video-decode"_s;
    }
#else
    Q_UNUSED(hardwareVideoDecode)
#endif
    if (!features.isEmpty()) {
        flags << u"--enable-features="_s + features.join(u',');
    }
    if (useSoftwareGpu(acceleration, gpuAutoDisabled)) {
        // Whole GPU process off: the stable choice for broken drivers. Video
        // then composites in software, which is fine up to 1080p on any
        // recent CPU.
        flags << u"--disable-gpu"_s;
    } else if (acceleration == HardwareAcceleration::On) {
        flags << u"--ignore-gpu-blocklist"_s;
    }
    return flags;
}

namespace {

// Chromium keeps only the last --enable-features/--disable-features switch,
// so their comma lists must be merged instead of appended.
bool isFeatureList(const QString& flag, QString* key, QStringList* values)
{
    for (const auto& prefix : {u"--enable-features="_s, u"--disable-features="_s}) {
        if (flag.startsWith(prefix)) {
            *key = prefix;
            *values = flag.mid(prefix.size()).split(u',', Qt::SkipEmptyParts);
            return true;
        }
    }
    return false;
}

} // namespace

QString mergeChromiumFlags(const QString& userFlags, const QStringList& ours)
{
    QStringList merged;
    QHash<QString, QStringList> featureLists; // prefix → values, in first-seen order
    QStringList featureOrder;
    auto absorb = [&](const QString& flag) {
        QString key;
        QStringList values;
        if (isFeatureList(flag, &key, &values)) {
            if (!featureLists.contains(key)) {
                featureOrder << key;
            }
            for (const QString& v : values) {
                if (!featureLists[key].contains(v)) {
                    featureLists[key] << v;
                }
            }
            return;
        }
        if (!merged.contains(flag)) {
            merged << flag;
        }
    };
    for (const QString& flag : userFlags.split(u' ', Qt::SkipEmptyParts)) {
        absorb(flag);
    }
    for (const QString& flag : ours) {
        absorb(flag);
    }
    for (const QString& key : featureOrder) {
        merged << key + featureLists.value(key).join(u',');
    }
    return merged.join(u' ');
}

} // namespace red::core
