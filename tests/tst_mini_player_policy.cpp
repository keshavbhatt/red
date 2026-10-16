#include "core/mini_player_policy.h"

#include <QTest>

using namespace red::core;
using namespace Qt::StringLiterals;

class TestMiniPlayerPolicy : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void shapeFollowsTheContent()
    {
        QCOMPARE(miniPlayerShape(AppMode::Desktop, QUrl(u"https://www.youtube.com/watch?v=aqz-KE-bpKQ"_s)),
                 MiniPlayerShape::Wide);
        QCOMPARE(miniPlayerShape(AppMode::Desktop, QUrl(u"https://www.youtube.com/shorts/FMJD9h-dlBU"_s)),
                 MiniPlayerShape::Shorts);
        // Music mode is always YouTube Music's tall player page, whatever the URL.
        QCOMPARE(miniPlayerShape(AppMode::Music, QUrl(u"https://music.youtube.com/watch?v=dQw4w9WgXcQ"_s)),
                 MiniPlayerShape::Music);
        QCOMPARE(miniPlayerShape(AppMode::Desktop, QUrl(u"https://www.youtube.com/@shortsfan"_s)),
                 MiniPlayerShape::Wide);
    }

    void sizesKeepTheirShape()
    {
        QCOMPARE(miniPlayerSize(MiniPlayerSize::Medium, MiniPlayerShape::Wide), QSize(480, 270));
        QCOMPARE(miniPlayerSize(MiniPlayerSize::Small, MiniPlayerShape::Wide), QSize(400, 225));
        QCOMPARE(miniPlayerSize(MiniPlayerSize::Medium, MiniPlayerShape::Shorts), QSize(270, 480));
        QCOMPARE(miniPlayerSize(MiniPlayerSize::Medium, MiniPlayerShape::Music), QSize(360, 640));
        for (const MiniPlayerSize size :
             {MiniPlayerSize::Small, MiniPlayerSize::Medium, MiniPlayerSize::Large}) {
            const QSize wide = miniPlayerSize(size, MiniPlayerShape::Wide);
            const QSize shorts = miniPlayerSize(size, MiniPlayerShape::Shorts);
            const QSize music = miniPlayerSize(size, MiniPlayerShape::Music);
            QVERIFY(wide.width() > wide.height());
            QVERIFY(shorts.height() > shorts.width());
            QVERIFY(music.height() > music.width());
        }
        QVERIFY(miniPlayerSize(MiniPlayerSize::Large, MiniPlayerShape::Music).height() >
                miniPlayerSize(MiniPlayerSize::Small, MiniPlayerShape::Music).height());
    }
};

QTEST_GUILESS_MAIN(TestMiniPlayerPolicy)
#include "tst_mini_player_policy.moc"
