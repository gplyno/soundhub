#pragma once

#include <QAudioOutput>
#include <QObject>
#include <QUrl>

class QMediaPlayer;

class Player : public QObject
{
    Q_OBJECT

public:
    explicit Player(QObject *parent = nullptr);

    void play(const QUrl &url);
    void togglePause();
    void stop();
    void seek(qint64 ms);
    void setVolume(int percent);

    bool isPlaying() const;
    qint64 position() const;
    qint64 duration() const;

signals:
    void playingChanged(bool playing);
    void positionChanged(qint64 ms);
    void durationChanged(qint64 ms);
    void finished();
    void errorOccurred(const QString &message);

private:
    QMediaPlayer *m_player;
    QAudioOutput *m_audio;
};
