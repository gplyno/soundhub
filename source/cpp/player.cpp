#include "player.h"

#include <QMediaPlayer>

Player::Player(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_audio(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audio);
    m_audio->setVolume(0.8f);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        emit playingChanged(state == QMediaPlayer::PlayingState);
    });
    connect(m_player, &QMediaPlayer::positionChanged, this, &Player::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &Player::durationChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia)
            emit finished();
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &errorString) {
                if (!errorString.isEmpty())
                    emit errorOccurred(errorString);
            });
}

void Player::play(const QUrl &url)
{
    m_player->setSource(url);
    m_player->play();
}

void Player::togglePause()
{
    if (m_player->playbackState() == QMediaPlayer::PlayingState)
        m_player->pause();
    else
        m_player->play();
}

void Player::stop()
{
    m_player->stop();
}

void Player::seek(qint64 ms)
{
    m_player->setPosition(ms);
}

void Player::setVolume(int percent)
{
    const float volume = qBound(0, percent, 100) / 100.0f;
    m_audio->setVolume(volume);
}

bool Player::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

qint64 Player::position() const
{
    return m_player->position();
}

qint64 Player::duration() const
{
    return m_player->duration();
}
