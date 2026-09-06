// Drives the queue with a fake yt-dlp (a shell script that speaks the RED:
// protocol) so scheduling, progress, pause/resume, persistence and failure
// handling are tested without the network.

#include "core/downloads/download_queue.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;
using namespace red::core;

class TestDownloadQueue : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_fake = m_dir->filePath(u"fake-yt-dlp"_s);
        QFile script(m_fake);
        QVERIFY(script.open(QIODevice::WriteOnly));
        // Emits an item line, three progress lines, a postprocess pair and the
        // final path; sleeps between lines so pause() can interrupt. Fails when
        // the URL contains "fail".
        script.write(
            QByteArray() + "#!/bin/sh\n" + "url=$(eval echo \\${$#})\n" +
            "out=\"$(dirname \"$0\")/result.mp4\"\n" +
            "case \"$url\" in *fail*) echo \"ERROR: [youtube] x: Video unavailable\" 1>&2; exit 1;; esac\n" +
            "echo 'REDITEM:{\"id\":\"vid\",\"title\":\"Fake "
            "video\",\"index\":0,\"count\":0,\"thumbnail\":\"\",\"duration\":19,\"uploader\":\"U\",\"url\":"
            "\"u\"}'\n" +
            "for n in 100 200 300; do\n" +
            "  echo "
            "\"RED:{\\\"status\\\":\\\"downloading\\\",\\\"downloaded\\\":$n,\\\"total\\\":300,"
            "\\\"estimate\\\":0,\\\"speed\\\":50,\\\"eta\\\":1,\\\"filename\\\":\\\"$out\\\",\\\"index\\\":0,"
            "\\\"id\\\":\\\"vid\\\",\\\"count\\\":0}\"\n" +
            "  sleep 0.15\n" + "done\n" +
            "echo "
            "'RED:{\"status\":\"finished\",\"downloaded\":300,\"total\":300,\"estimate\":0,\"speed\":0,"
            "\"eta\":0,\"filename\":\"x\",\"index\":0,\"id\":\"vid\",\"count\":0}'\n" +
            "echo 'REDPP:{\"status\":\"started\",\"postprocessor\":\"Merger\",\"id\":\"vid\"}'\n" +
            "echo 'REDPP:{\"status\":\"finished\",\"postprocessor\":\"Merger\",\"id\":\"vid\"}'\n" +
            "printf 'abc' > \"$out\"\n" + "echo \"REDFILE:$out\"\n" + "\n");
        script.close();
        QVERIFY(QFile::setPermissions(m_fake, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }

    DownloadJob job(const QString& url)
    {
        DownloadJob j;
        j.url = url;
        j.title = u"T"_s;
        j.options.outputDirectory = m_dir->path();
        return j;
    }

    void runsAJobToCompletion()
    {
        DownloadQueue queue;
        QSignalSpy finished(&queue, &DownloadQueue::jobFinished);
        const quint64 id = queue.add(job(u"https://www.youtube.com/watch?v=aaaaaaaaaaa"_s));
        QCOMPARE(queue.job(id)->state, DownloadState::Queued); // no engine yet
        EnginePaths paths;
        paths.ytdlp = m_fake;
        queue.setEnginePaths(paths);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        const DownloadJob done = *queue.job(id);
        QCOMPARE(done.state, DownloadState::Completed);
        QCOMPARE(done.outputFiles.size(), 1);
        QVERIFY(done.outputFiles.first().endsWith(u"result.mp4"_s));
        QCOMPARE(done.totalBytes, 3); // size of the file on disk
        QCOMPARE(done.videoId, u"vid"_s);
        QCOMPARE(done.uploader, u"U"_s);
        QCOMPARE(queue.activeCount(), 0);
    }

    void reportsFailuresFriendly()
    {
        DownloadQueue queue;
        EnginePaths paths;
        paths.ytdlp = m_fake;
        queue.setEnginePaths(paths);
        QSignalSpy finished(&queue, &DownloadQueue::jobFinished);
        const quint64 id = queue.add(job(u"https://www.youtube.com/watch?v=fail"_s));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
        QCOMPARE(queue.job(id)->state, DownloadState::Failed);
        QCOMPARE(queue.job(id)->error, u"This video is unavailable."_s);
        queue.retry(id);
        QCOMPARE(queue.job(id)->state, DownloadState::Probing); // started again immediately
    }

    void respectsConcurrencyAndPause()
    {
        DownloadQueue queue;
        queue.setMaxConcurrent(1);
        EnginePaths paths;
        paths.ytdlp = m_fake;
        queue.setEnginePaths(paths);
        const quint64 a = queue.add(job(u"https://www.youtube.com/watch?v=aaaaaaaaaaa"_s));
        const quint64 b = queue.add(job(u"https://www.youtube.com/watch?v=bbbbbbbbbbb"_s));
        QCOMPARE(queue.runningCount(), 1);
        QCOMPARE(queue.job(b)->state, DownloadState::Queued);
        QCOMPARE(queue.rowOf(b), 0); // newest first
        queue.pause(a);
        QCOMPARE(queue.job(a)->state, DownloadState::Paused);
        QTRY_COMPARE(queue.job(b)->state, DownloadState::Probing); // b took the slot
        queue.cancel(b);
        QCOMPARE(queue.job(b)->state, DownloadState::Cancelled);
        queue.resume(a);
        QSignalSpy finished(&queue, &DownloadQueue::jobFinished);
        QTRY_VERIFY_WITH_TIMEOUT(queue.job(a)->state == DownloadState::Completed, 10000);
        // Duplicate active URL is folded into the existing job.
        const quint64 c = queue.add(job(u"https://www.youtube.com/watch?v=ccccccccccc"_s));
        QCOMPARE(queue.add(job(u"https://www.youtube.com/watch?v=ccccccccccc"_s)), c);
        queue.clearFinished();
        QVERIFY(!queue.job(a));
        QVERIFY(!queue.job(b));
        QVERIFY(queue.job(c));
    }

    void persistsAndInterruptsOnLoad()
    {
        const QString file = m_dir->filePath(u"downloads.json"_s);
        {
            DownloadQueue queue;
            EnginePaths paths;
            paths.ytdlp = m_fake;
            queue.setEnginePaths(paths);
            queue.add(job(u"https://www.youtube.com/watch?v=aaaaaaaaaaa"_s));
            QTRY_COMPARE(queue.job(1)->state, DownloadState::Probing);
            QVERIFY(queue.save(file));
        }
        DownloadQueue loaded;
        QVERIFY(loaded.load(file));
        QCOMPARE(loaded.rowCount(), 1);
        QCOMPARE(loaded.job(1)->state, DownloadState::Paused); // was active → resumable
        QCOMPARE(loaded.job(1)->title, u"T"_s);
        QCOMPARE(loaded.data(loaded.index(0), DownloadQueue::TitleRole).toString(), u"T"_s);
        QVERIFY(!DownloadQueue().load(m_dir->filePath(u"missing.json"_s)));
    }

private:
    std::unique_ptr<QTemporaryDir> m_dir;
    QString m_fake;
};

QTEST_GUILESS_MAIN(TestDownloadQueue)
#include "tst_download_queue.moc"
