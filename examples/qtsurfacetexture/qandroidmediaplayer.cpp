#include "qandroidmediaplayer.h"
#include "qsurfacetexture.h"

#include <QAndroidJniEnvironment>
#include <QtAndroid>
#include <QAndroidJniObject>

QAndroidMediaPlayer::QAndroidMediaPlayer(QObject *parent)
    : QObject(parent)
    , m_mediaPlayer()
{


    QtAndroid::runOnAndroidThreadSync([=](){
        m_mediaPlayer = QAndroidJniObject::callStaticObjectMethod(
                    "com/google/android/exoplayer2/ExoPlayerFactory",
                    "newSimpleInstance",
                    "(Landroid/content/Context;)Lcom/google/android/exoplayer2/SimpleExoPlayer;",
                    QtAndroid::androidContext().object<jobject>());
    });
}

QAndroidMediaPlayer::~QAndroidMediaPlayer()
{
    QtAndroid::runOnAndroidThreadSync([=](){
        QAndroidJniEnvironment env;
        m_mediaPlayer.callMethod<void>("stop");
        m_mediaPlayer.callMethod<void>("reset");
        m_mediaPlayer.callMethod<void>("release");
    });
}

QSurfaceTexture *QAndroidMediaPlayer::videoOut() const
{
    return m_videoOut;
}

void QAndroidMediaPlayer::setVideoOut(QSurfaceTexture *videoOut)
{
    if (m_videoOut == videoOut)
        return;
    if (m_videoOut)
        m_videoOut->disconnect(this);
    m_videoOut = videoOut;

    auto setSurfaceTexture = [=]{
        // Create a new Surface object from our SurfaceTexture
        QAndroidJniObject surface("android/view/Surface",
                                  "(Landroid/graphics/SurfaceTexture;)V",
                                  m_videoOut->surfaceTexture().object());

        QtAndroid::runOnAndroidThreadSync([=](){
            // Set the new surface to m_mediaPlayer object
            m_mediaPlayer.callMethod<void>("setVideoSurface",
                                           "(Landroid/view/Surface;)V",
                                           surface.object());
        });
    };

    if (videoOut->surfaceTexture().isValid())
        setSurfaceTexture();
    else
        connect(m_videoOut.data(), &QSurfaceTexture::surfaceTextureChanged, this, setSurfaceTexture);
    emit videoOutChanged();
}

void QAndroidMediaPlayer::playFile(const QString &file)
{ 
    QAndroidJniObject applicationName = QAndroidJniObject::fromString("com.example.qtapp");
    QAndroidJniObject userAgent = QAndroidJniObject::callStaticObjectMethod("com/google/android/exoplayer2/util/Util",
                                                                            "getUserAgent",
                                                                            "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
                                                                            QtAndroid::androidContext().object<jobject>(),
                                                                            applicationName.object<jstring>());



    QAndroidJniObject dataSourceFactory("com/google/android/exoplayer2/upstream/DefaultDataSourceFactory",
                                        "(Landroid/content/Context;Ljava/lang/String;)V",
                                        QtAndroid::androidContext().object<jobject>(),
                                        userAgent.object<jstring>());


    QAndroidJniObject extractorFactory("com/google/android/exoplayer2/source/ExtractorMediaSource$Factory",
                                       "(Lcom/google/android/exoplayer2/upstream/DataSource$Factory;)V",
                                       dataSourceFactory.object<jobject>());

    QAndroidJniObject uriTest = QAndroidJniObject::callStaticObjectMethod("android/net/Uri",
                                                                          "parse",
                                                                          "(Ljava/lang/String;)Landroid/net/Uri;",
                                                                          QAndroidJniObject::fromString(file).object<jstring>());

    QAndroidJniObject mediaSource = extractorFactory.callObjectMethod("createMediaSource",
                                                                      "(Landroid/net/Uri;)Lcom/google/android/exoplayer2/source/ExtractorMediaSource;",

                                                                      uriTest.object<jobject>());

    QtAndroid::runOnAndroidThreadSync([=](){
        m_mediaPlayer.callMethod<void>("prepare",
                                       "(Lcom/google/android/exoplayer2/source/MediaSource;)V",
                                       mediaSource.object<jobject>());

        // start playing
        m_mediaPlayer.callMethod<void>("setPlayWhenReady", "(Z)V", true);
    });
}
