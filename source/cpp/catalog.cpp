#include "catalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>

namespace {

const QString kRepo = QStringLiteral("gplyno/soundhub");
const QString kBranch = QStringLiteral("main");
const QString kRawBase = QStringLiteral("https://raw.githubusercontent.com/gplyno/soundhub/main/");
const QString kManifestUrl = kRawBase + QStringLiteral("catalog.json");
const QString kListingUrl = QStringLiteral("https://api.github.com/repos/gplyno/soundhub/contents/music");

QUrl encodePath(const QString &base, const QString &relative)
{
    QStringList parts = relative.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (QString &part : parts)
        part = QString::fromLatin1(QUrl::toPercentEncoding(part));
    return QUrl(base + parts.join(QLatin1Char('/')));
}

QString prettyName(const QString &fileName)
{
    QString stem = fileName;
    const int dot = stem.lastIndexOf(QLatin1Char('.'));
    if (dot > 0)
        stem.truncate(dot);
    return stem;
}

QString slugify(const QString &text)
{
    QString out;
    for (const QChar ch : text.toLower()) {
        if (ch.isLetterOrNumber())
            out.append(ch);
        else if (!out.isEmpty() && !out.endsWith(QLatin1Char('-')))
            out.append(QLatin1Char('-'));
    }
    while (out.endsWith(QLatin1Char('-')))
        out.chop(1);
    return out.isEmpty() ? QStringLiteral("track") : out;
}

void splitArtistTitle(const QString &clean, QString *artist, QString *title)
{
    const int sep = clean.indexOf(QStringLiteral(" - "));
    if (sep > 0) {
        *artist = clean.left(sep).trimmed();
        *title = clean.mid(sep + 3).trimmed();
    } else {
        artist->clear();
        *title = clean.trimmed();
    }
}

} // namespace

QString Track::displayArtist() const
{
    return artist.isEmpty() ? QStringLiteral("Неизвестный исполнитель") : artist;
}

QString Track::displayTitle() const
{
    return title.isEmpty() ? QStringLiteral("Без названия") : title;
}

Catalog::Catalog(QObject *parent)
    : QObject(parent)
{
}

QString Catalog::stripWatermark(const QString &text)
{
    static const QRegularExpression topicTag(
        QStringLiteral(R"(_-\s*Topic\s*-_|_\s*Topic\s*_)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression domainInBrackets(
        QStringLiteral(R"([\(\[]\s*[A-Za-z0-9_.\-]+\.(?:fm|net|com|ru|org|io|me|cc|tv|info|biz|xyz|top|site|online|pro)\s*[\)\]])"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression bareDomain(
        QStringLiteral(R"(\b[A-Za-z0-9][A-Za-z0-9\-]*\.(?:fm|net|com|ru|org|io|me|cc|tv|info|biz|xyz|top|site|online|pro)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression trailingTopic(
        QStringLiteral(R"(\s*-\s*Topic\s*$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression edges(
        QStringLiteral(R"(^\s*[-–—]+\s*|\s*[-–—]+\s*$)"));

    QString s = text;
    s.replace(topicTag, QStringLiteral(" - "));
    s.replace(QLatin1Char('_'), QLatin1Char(' '));
    s.remove(domainInBrackets);
    s.remove(bareDomain);
    s = s.simplified();
    s.remove(trailingTopic);
    s = s.simplified();
    s.remove(edges);
    return s.trimmed();
}

void Catalog::reload()
{
    ++m_generation;
    const int generation = m_generation;
    m_tracks.clear();

    // Local library wins: a "music" folder next to the executable.
    QVector<Track> local;
    if (collectLocal(local)) {
        finishWith(local);
        return;
    }

    setLoading(true);

    QNetworkRequest request{QUrl(kManifestUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SoundHub/0.1"));

    QNetworkReply *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            fallbackToListing();
            return;
        }
        parseManifest(reply->readAll());
    });
}

void Catalog::fallbackToListing()
{
    const int generation = m_generation;

    QNetworkRequest request{QUrl(kListingUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SoundHub/0.1"));
    request.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *reply = m_nam.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            loadLocal();
            return;
        }
        parseRepoListing(reply->readAll());
    });
}

void Catalog::parseManifest(const QByteArray &data)
{
    QJsonParseError error{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        fallbackToListing();
        return;
    }

    const QJsonObject root = doc.object();
    QString base = root.value(QStringLiteral("base")).toString();
    if (base.isEmpty())
        base = kRawBase;
    if (!base.endsWith(QLatin1Char('/')))
        base.append(QLatin1Char('/'));

    QVector<Track> tracks;
    const QJsonArray items = root.value(QStringLiteral("tracks")).toArray();
    tracks.reserve(items.size());

    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        Track track;
        track.file = object.value(QStringLiteral("file")).toString();
        if (track.file.isEmpty())
            continue;

        track.id = object.value(QStringLiteral("id")).toString();
        track.artist = object.value(QStringLiteral("artist")).toString();
        track.title = object.value(QStringLiteral("title")).toString();
        track.duration = object.value(QStringLiteral("duration")).toDouble();
        track.size = static_cast<qint64>(object.value(QStringLiteral("size")).toDouble());
        if (track.size > 0 && track.size < 10240)
            continue;
        track.url = encodePath(base, track.file);

        track.artist = stripWatermark(track.artist);
        track.title = stripWatermark(track.title);
        if (track.id.isEmpty())
            track.id = slugify(track.artist + QLatin1Char(' ') + track.title);

        tracks.append(track);
    }

    finishWith(tracks);
}

void Catalog::parseRepoListing(const QByteArray &data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        loadLocal();
        return;
    }

    QVector<Track> tracks;
    const QJsonArray items = doc.array();
    tracks.reserve(items.size());

    for (const QJsonValue &value : items) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("type")).toString() != QStringLiteral("file"))
            continue;

        const QString name = object.value(QStringLiteral("name")).toString();
        if (object.value(QStringLiteral("size")).toDouble() < 10240)
            continue;
        if (!name.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive)
            && !name.endsWith(QStringLiteral(".m4a"), Qt::CaseInsensitive)
            && !name.endsWith(QStringLiteral(".ogg"), Qt::CaseInsensitive))
            continue;

        Track track;
        const QString clean = stripWatermark(prettyName(name));
        splitArtistTitle(clean, &track.artist, &track.title);
        track.id = slugify(clean);
        track.file = QStringLiteral("music/") + name;
        track.size = static_cast<qint64>(object.value(QStringLiteral("size")).toDouble());
        track.url = QUrl(object.value(QStringLiteral("download_url")).toString());
        if (track.url.isEmpty())
            track.url = encodePath(kRawBase, track.file);

        tracks.append(track);
    }

    if (tracks.isEmpty()) {
        loadLocal();
        return;
    }
    finishWith(tracks);
}

bool Catalog::collectLocal(QVector<Track> &tracks)
{
    QStringList roots;
    const QString env = qEnvironmentVariable("SOUNDHUB_MUSIC_DIR");
    if (!env.isEmpty())
        roots << env;

    const QString appDir = QCoreApplication::applicationDirPath();
    roots << appDir + QStringLiteral("/music")
          << appDir + QStringLiteral("/../music")
          << appDir + QStringLiteral("/../../music")
          << QDir::currentPath() + QStringLiteral("/music");

    const QStringList filters{QStringLiteral("*.mp3"), QStringLiteral("*.m4a"),
                              QStringLiteral("*.ogg"), QStringLiteral("*.wav"),
                              QStringLiteral("*.flac"), QStringLiteral("*.aac")};

    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists())
            continue;

        const QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
        for (const QFileInfo &info : files) {
            if (info.size() < 10240)
                continue;

            Track track;
            const QString clean = stripWatermark(prettyName(info.fileName()));
            splitArtistTitle(clean, &track.artist, &track.title);
            track.id = slugify(clean);
            track.file = info.fileName();
            track.size = info.size();
            track.url = QUrl::fromLocalFile(info.absoluteFilePath());
            tracks.append(track);
        }

        if (!tracks.isEmpty())
            break;
    }

    return !tracks.isEmpty();
}

void Catalog::loadLocal()
{
    QVector<Track> tracks;
    if (!collectLocal(tracks)) {
        setLoading(false);
        emit failed(tr("Не удалось загрузить каталог. Проверьте интернет или папку music."));
        return;
    }
    finishWith(tracks);
}

void Catalog::finishWith(const QVector<Track> &tracks)
{
    m_tracks = tracks;
    setLoading(false);
    emit loaded(m_tracks.size());
}

void Catalog::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged(loading);
}
