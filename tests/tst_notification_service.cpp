#include "core/notifications/notification_service.h"
#include "core/settings/settings.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class FakeNotifier : public INotifier
{
    Q_OBJECT
public:
    using INotifier::INotifier;
    QString name() const override { return u"fake"_s; }
    bool isAvailable() const override { return available; }
    void show(quint64 id, const Notification& n) override
    {
        shown.append({id, n});
        if (failNext) {
            failNext = false;
            Q_EMIT failed(id, u"boom"_s);
        }
    }
    void close(quint64 id) override { closedIds.append(id); }

    bool available = true;
    bool failNext = false;
    QList<QPair<quint64, Notification>> shown;
    QList<quint64> closedIds;
};

namespace {
Notification note(const QString& title)
{
    Notification n;
    n.title = title;
    return n;
}
} // namespace

class TestNotificationService : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_settings = std::make_unique<Settings>(m_dir->filePath(u"s.ini"_s));
        m_primary = std::make_unique<FakeNotifier>();
        m_fallback = std::make_unique<FakeNotifier>();
        m_service = std::make_unique<NotificationService>(*m_settings, m_primary.get(), m_fallback.get());
    }

    void assignsIdsAndKeepsImage()
    {
        QImage thumb(8, 8, QImage::Format_ARGB32);
        thumb.fill(Qt::red);
        Notification n;
        n.title = u"Download finished"_s;
        n.body = u"Big Buck Bunny"_s;
        n.image = thumb;
        n.actions.append({u"folder"_s, u"Show in folder"_s});
        const quint64 id = m_service->notify(n);
        QVERIFY(id != 0);
        QCOMPARE(m_primary->shown.size(), 1);
        const Notification& sent = m_primary->shown.first().second;
        QCOMPARE(sent.image, thumb);
        QCOMPARE(sent.actions.size(), 1);
        QCOMPARE(m_service->activeCount(), 1);
        const quint64 next = m_service->notify(n);
        QVERIFY(next != id);
    }

    void actionInvokedOnlyForKnownIds()
    {
        QSignalSpy spy(m_service.get(), &NotificationService::actionInvoked);
        const quint64 id = m_service->notify(note(u"x"_s));
        Q_EMIT m_primary->actionInvoked(999, u"folder"_s);
        QCOMPARE(spy.count(), 0);
        Q_EMIT m_primary->actionInvoked(id, u"folder"_s);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(1).toString(), u"folder"_s);
    }

    void activationOnlyForKnownIds()
    {
        QSignalSpy spy(m_service.get(), &NotificationService::activated);
        const quint64 id = m_service->notify(note(u"x"_s));
        Q_EMIT m_primary->activated(999); // foreign id must be ignored
        QCOMPARE(spy.count(), 0);
        Q_EMIT m_primary->activated(id);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toULongLong(), id);
    }

    void closeForwardsToOwnerAndEmitsOnce()
    {
        QSignalSpy spy(m_service.get(), &NotificationService::closed);
        const quint64 id = m_service->notify(note(u"x"_s));
        m_service->close(id);
        QCOMPARE(m_primary->closedIds, QList<quint64>{id});
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m_service->activeCount(), 0);
        Q_EMIT m_primary->closed(id); // late backend echo: no second signal
        QCOMPARE(spy.count(), 1);
    }

    void fallsBackWhenPrimaryFails()
    {
        m_primary->failNext = true;
        const quint64 id = m_service->notify(note(u"x"_s));
        QCOMPARE(m_primary->shown.size(), 1);
        QCOMPARE(m_fallback->shown.size(), 1);
        QCOMPARE(m_fallback->shown.first().first, id);
        QCOMPARE(m_service->activeCount(), 1);
        // Activation now comes from the fallback.
        QSignalSpy spy(m_service.get(), &NotificationService::activated);
        Q_EMIT m_fallback->activated(id);
        QCOMPARE(spy.count(), 1);
    }

    void usesFallbackWhenPrimaryUnavailable()
    {
        m_primary->available = false;
        QCOMPARE(m_service->backendName(), u"fake"_s);
        m_service->notify(note(u"x"_s));
        QCOMPARE(m_primary->shown.size(), 0);
        QCOMPARE(m_fallback->shown.size(), 1);
    }

    void dropsWhenNothingAvailable()
    {
        m_primary->available = false;
        m_fallback->available = false;
        QCOMPARE(m_service->notify(note(u"x"_s)), quint64{0});
        QCOMPARE(m_service->backendName(), u"none"_s);
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<FakeNotifier> m_primary;
    std::unique_ptr<FakeNotifier> m_fallback;
    std::unique_ptr<NotificationService> m_service;
};

QTEST_MAIN(TestNotificationService)
#include "tst_notification_service.moc"
