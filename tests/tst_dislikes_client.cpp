#include "services/dislikes_client.h"

#include <QTest>

using namespace red::services;

class TestDislikesClient : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void parsesVotes()
    {
        QCOMPARE(DislikesClient::parseDislikes("{\"id\":\"x\",\"likes\":19000000,\"dislikes\":518123,\"rating\":4.9}"), 518123.0);
        QCOMPARE(DislikesClient::parseDislikes("{}"), -1.0);
        QCOMPARE(DislikesClient::parseDislikes("nope"), -1.0);
    }
};

QTEST_GUILESS_MAIN(TestDislikesClient)
#include "tst_dislikes_client.moc"
