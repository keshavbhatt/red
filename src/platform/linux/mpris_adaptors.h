#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QStringList>
#include <QVariantMap>

namespace red::platform::linux_ {

class MprisDBusService;

/// org.mpris.MediaPlayer2 — the root interface.
class MprisRootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)
    Q_PROPERTY(bool Fullscreen READ fullscreen WRITE setFullscreen)
    Q_PROPERTY(bool CanSetFullscreen READ canSetFullscreen)

public:
    explicit MprisRootAdaptor(MprisDBusService* service);

    [[nodiscard]] bool canQuit() const { return true; }
    [[nodiscard]] bool canRaise() const { return true; }
    [[nodiscard]] bool hasTrackList() const { return false; }
    [[nodiscard]] QString identity() const;
    [[nodiscard]] QString desktopEntry() const;
    [[nodiscard]] QStringList supportedUriSchemes() const { return {QStringLiteral("https")}; }
    [[nodiscard]] QStringList supportedMimeTypes() const { return {}; }
    [[nodiscard]] bool fullscreen() const { return false; }
    void setFullscreen(bool) {}
    [[nodiscard]] bool canSetFullscreen() const { return false; }

public Q_SLOTS:
    void Raise();
    void Quit();

private:
    MprisDBusService* m_service;
};

/// org.mpris.MediaPlayer2.Player
class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    explicit MprisPlayerAdaptor(MprisDBusService* service);

    [[nodiscard]] QString playbackStatus() const;
    [[nodiscard]] QString loopStatus() const { return QStringLiteral("None"); }
    void setLoopStatus(const QString&) {}
    [[nodiscard]] double rate() const { return 1.0; }
    void setRate(double) {}
    [[nodiscard]] bool shuffle() const { return false; }
    void setShuffle(bool) {}
    [[nodiscard]] QVariantMap metadata() const;
    [[nodiscard]] double volume() const;
    void setVolume(double volume);
    [[nodiscard]] qlonglong position() const;
    [[nodiscard]] double minimumRate() const { return 1.0; }
    [[nodiscard]] double maximumRate() const { return 1.0; }
    [[nodiscard]] bool canGoNext() const;
    [[nodiscard]] bool canGoPrevious() const;
    [[nodiscard]] bool canPlay() const;
    [[nodiscard]] bool canPause() const;
    [[nodiscard]] bool canSeek() const;
    [[nodiscard]] bool canControl() const { return true; }

public Q_SLOTS:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offsetUs);
    void SetPosition(const QDBusObjectPath& trackId, qlonglong positionUs);
    void OpenUri(const QString& uri);

Q_SIGNALS:
    void Seeked(qlonglong positionUs);

private:
    MprisDBusService* m_service;
};

} // namespace red::platform::linux_
