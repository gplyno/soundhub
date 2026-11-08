#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>

struct Track
{
    QString id;
    QString artist;
    QString title;
    QString file;      // path inside the repository, e.g. "music/Artist - Title.mp3"
    double duration = 0.0;
    qint64 size = 0;
    QUrl url;          // resolved download url

    QString displayArtist() const;
    QString displayTitle() const;
};

class Catalog : public QObject
{
    Q_OBJECT

public:
    explicit Catalog(QObject *parent = nullptr);

    void reload();

    const QVector<Track> &tracks() const { return m_tracks; }
    bool isLoading() const { return m_loading; }

    // Removes site/channel watermarks ("(Myz.fm)", "_Topic_", ...) from a file name.
    static QString stripWatermark(const QString &text);

signals:
    void loadingChanged(bool loading);
    void loaded(int count);
    void failed(const QString &message);

private:
    void setLoading(bool loading);
    void parseManifest(const QByteArray &data);
    void parseRepoListing(const QByteArray &data);
    void fallbackToListing();
    bool collectLocal(QVector<Track> &tracks);
    void loadLocal();
    void finishWith(const QVector<Track> &tracks);

    QNetworkAccessManager m_nam;
    QVector<Track> m_tracks;
    bool m_loading = false;
    int m_generation = 0;
};
