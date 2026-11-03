// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#include "core/licensing/legacy_account.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestLegacyAccount : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void idShape()
    {
        QVERIFY(looksLikeAccountId(u"3e4bdb050f784b01b1f1"_s));
        QVERIFY(looksLikeAccountId(u"winAbC123xyz"_s));
        QVERIFY(!looksLikeAccountId(u"short"_s));
        QVERIFY(!looksLikeAccountId(u"has space 12345"_s));
        QVERIFY(!looksLikeAccountId(QString()));
    }

    void readsRed9Settings()
    {
        QTemporaryDir home;
        QDir().mkpath(home.filePath(u".config/org.keshavnrj.ubuntu"_s));
        QFile conf(home.filePath(u".config/org.keshavnrj.ubuntu/Red.conf"_s));
        QVERIFY(conf.open(QIODevice::WriteOnly));
        conf.write("[General]\naccountId=3e4bdb050f784b01b1f1\nRed=YWN0aXZhdGVk\n");
        conf.close();
        QCOMPARE(legacyRedAccountId({home.path()}), u"3e4bdb050f784b01b1f1"_s);
    }

    void fallsBackToDownloadsFile()
    {
        QTemporaryDir home;
        QDir().mkpath(home.filePath(u"Downloads"_s));
        QFile id(home.filePath(u"Downloads/.red.id"_s));
        QVERIFY(id.open(QIODevice::WriteOnly));
        id.write("abcdef1234567890\nZW1pdA==\n");
        id.close();
        QCOMPARE(legacyRedAccountId({home.path()}), u"abcdef1234567890"_s);
    }

    void firstHomeWinsAndMissingIsEmpty()
    {
        QTemporaryDir a;
        QTemporaryDir b;
        QDir().mkpath(b.filePath(u"Downloads"_s));
        QFile id(b.filePath(u"Downloads/.red.id"_s));
        QVERIFY(id.open(QIODevice::WriteOnly));
        id.write("bbbbbbbb1111\n");
        id.close();
        QCOMPARE(legacyRedAccountId({a.path(), b.path()}), u"bbbbbbbb1111"_s);
        QVERIFY(legacyRedAccountId({a.path()}).isEmpty());
        QVERIFY(!legacyAccountSearchHomes().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestLegacyAccount)
#include "tst_legacy_account.moc"
