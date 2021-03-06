// Copyright © 2020 Jonas Kümmerlin <jonas@kuemmerlin.eu>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include "releasefinder.h"

#include "../tasklib/syncurldownload.h"

#include <QXmlStreamReader>

namespace {

QString parseArtistCredits(QXmlStreamReader *xml)
{
    QStringList names;

    while (xml->readNextStartElement())
    {
        if (xml->name() == QStringLiteral("name-credit"))
        {
            while (xml->readNextStartElement())
            {
                if (xml->name() == QStringLiteral("artist"))
                {
                    while (xml->readNextStartElement())
                    {
                        if (xml->name() == QStringLiteral("name"))
                        {
                            names << xml->readElementText();
                        }
                        else
                        {
                            xml->skipCurrentElement();
                        }
                    }
                }
                else
                {
                    xml->skipCurrentElement();
                }
            }
        }
        else
        {
            xml->skipCurrentElement();
        }
    }

    return names.join(QStringLiteral(", "));
}

QString parseComposerFromWork(QXmlStreamReader *xml)
{
    QStringList composers;

    while (xml->readNextStartElement())
    {
        if (xml->name() == QStringLiteral("relation-list")
                && xml->attributes().value(QStringLiteral("target-type")) == QStringLiteral("artist"))
        {
            while (xml->readNextStartElement())
            {
                if (xml->name() == QStringLiteral("relation")
                        && xml->attributes().value(QStringLiteral("type")) == QStringLiteral("composer"))
                {
                    while (xml->readNextStartElement())
                    {
                        if (xml->name() == QStringLiteral("artist"))
                        {
                            while (xml->readNextStartElement())
                            {
                                if (xml->name() == QStringLiteral("name"))
                                {
                                    composers << xml->readElementText();
                                }
                                else
                                {
                                    xml->skipCurrentElement();
                                }
                            }
                        }
                        else
                        {
                            xml->skipCurrentElement();
                        }
                    }
                }
                else
                {
                    xml->skipCurrentElement();
                }
            }
        }
        else
        {
            xml->skipCurrentElement();
        }
    }

    return composers.join(QStringLiteral(", "));
}

MusicBrainz::TrackMetadata parseRecording(QXmlStreamReader *xml)
{
    MusicBrainz::TrackMetadata meta;

    while (xml->readNextStartElement())
    {
        if (xml->name() == QStringLiteral("title"))
        {
            meta.title = xml->readElementText();
        }
        else if (xml->name() == QStringLiteral("artist-credit"))
        {
            meta.artist = parseArtistCredits(xml);
        }
        else if (xml->name() == QStringLiteral("relation-list")
                 && xml->attributes().value(QStringLiteral("target-type")) == QStringLiteral("work"))
        {
            while (xml->readNextStartElement())
            {
                if (xml->name() == QStringLiteral("relation"))
                {
                    while (xml->readNextStartElement())
                    {
                        if (xml->name() == QStringLiteral("work"))
                        {
                            meta.composer = parseComposerFromWork(xml);
                        }
                        else
                        {
                            xml->skipCurrentElement();
                        }
                    }
                }
                else
                {
                    xml->skipCurrentElement();
                }
            }
        }
        else
        {
            xml->skipCurrentElement();
        }
    }

    return meta;
}

MusicBrainz::TrackMetadata parseTrack(QXmlStreamReader *xml)
{
    MusicBrainz::TrackMetadata metadata;
    while (xml->readNextStartElement())
    {
        if (xml->name() == QStringLiteral("number"))
        {
            metadata.trackno = xml->readElementText().toInt();
        }
        else if (xml->name() == QStringLiteral("recording"))
        {
            auto r = parseRecording(xml);
            metadata.artist = r.artist;
            metadata.title = r.title;
            metadata.composer = r.composer;
        }
        else
        {
            xml->skipCurrentElement();
        }
    }

    return metadata;
}

void parseMedium(QXmlStreamReader *xml, const QString &discid, MusicBrainz::ReleaseMetadata &metadata)
{
    // ugly: MusicBrainz gives us a list of media, one of these is the CD currently inserted
    // but we can only check that when we at some point stumble upon a matching disc id
    bool isCorrect = false;
    QString discNo;
    std::vector<MusicBrainz::TrackMetadata> trackMeta;

    while (xml->readNextStartElement())
    {
        if (xml->name() == QStringLiteral("position"))
        {
            discNo = xml->readElementText();
        }
        else if (xml->name() == QStringLiteral("disc-list"))
        {
            while (xml->readNextStartElement())
            {
                if (xml->name() == QStringLiteral("disc"))
                {
                    if (xml->attributes().value(QStringLiteral("id")) == discid)
                        isCorrect = true;
                }

                xml->skipCurrentElement();
            }
        }
        else if (xml->name() == QStringLiteral("track-list"))
        {
            while (xml->readNextStartElement())
            {
                if (xml->name() == QStringLiteral("track"))
                {
                    trackMeta.push_back(parseTrack(xml));
                }
                else
                {
                    xml->skipCurrentElement();
                }
            }
        }
        else if (xml->name() == QStringLiteral("pregap"))
        {
            trackMeta.push_back(parseTrack(xml));
        }
        else
        {
            xml->skipCurrentElement();
        }
    }

    if (isCorrect)
    {
        metadata.discNo = discNo;
        metadata.tracks = trackMeta;
    }
}

void parseRelease(QXmlStreamReader *xml, const QString &discid, MusicBrainz::ReleaseMetadata &metadata)
{
    Q_ASSERT(xml->name() == QStringLiteral("release"));
    metadata.releaseId = xml->attributes().value(QStringLiteral("id")).toString();

    while (xml->readNextStartElement())
    {
        if (xml->name() == QStringLiteral("title"))
        {
            metadata.title = xml->readElementText();
        }
        else if (xml->name() == QStringLiteral("date"))
        {
            QString date = xml->readElementText();
            int pos = 0;
            while (pos < date.size() && date[pos] >= QLatin1Char('0') && date[pos] <= QLatin1Char('9'))
                pos++;
            metadata.year = date.left(pos);
        }
        else if (xml->name() == QStringLiteral("artist-credit"))
        {
            metadata.artist = parseArtistCredits(xml);
        }
        else if (xml->name() == QStringLiteral("medium-list"))
        {
            while (xml->readNextStartElement())
            {
                if (xml->name() == QStringLiteral("medium"))
                {
                    parseMedium(xml, discid, metadata);
                }
                else
                {
                    xml->skipCurrentElement();
                }
            }
        }
        else
        {
            xml->skipCurrentElement();
        }
    }
}

QString getReleaseIdForDiscId(const QString &discid, const QString &mcn, const TaskRunner::CancelToken &cancelToken)
{
    QString url = QStringLiteral("https://musicbrainz.org/ws/2/discid/%1?inc=recordings+isrcs+labels").arg(discid);
    QByteArray discIdData = TaskRunner::downloadUrl(url, cancelToken);

    struct Release
    {
        QString id;
        QStringList catalogNrs;
    };
    std::vector<Release> availableReleases;

    QXmlStreamReader xml(discIdData);

    while (!xml.atEnd())
    {
        if (xml.readNextStartElement())
        {
            if (xml.name() == QStringLiteral("release"))
            {
                Release r;
                r.id = xml.attributes().value(QStringLiteral("id")).toString();
                if (!r.id.size())
                {
                    qWarning() << "WTF: <release> without id attribute?";
                    continue;
                }

                while (xml.readNextStartElement())
                {
                    if (xml.name() == QStringLiteral("barcode"))
                    {
                        r.catalogNrs << xml.readElementText();
                    }
                    // TODO: else if label-info-list
                    else
                    {
                        xml.skipCurrentElement();
                    }
                }

                availableReleases.push_back(r);
            }
        }
    }

    if (xml.hasError())
    {
        qWarning() << "XML parse error:" << xml.errorString();
    }

    if (availableReleases.size())
    {
        QStringRef mcnWithoutLeadingZeroes(&mcn);
        while (mcnWithoutLeadingZeroes.startsWith(QLatin1Char('0')))
            mcnWithoutLeadingZeroes = mcnWithoutLeadingZeroes.right(mcnWithoutLeadingZeroes.length()-1);

        // check if we have a release with the right catalog nr
        for (const Release &r : availableReleases)
        {
            if (r.catalogNrs.contains(mcn) || r.catalogNrs.contains(mcnWithoutLeadingZeroes.toString()))
            {
                return r.id;
            }
        }

        // otherwise just return the first
        return availableReleases[0].id;
    }
    else
    {
        return QString();
    }
}

MusicBrainz::ReleaseMetadata getMetadataForRelease(const QString &releaseId, const QString &discid, const TaskRunner::CancelToken &cancelToken)
{
    MusicBrainz::ReleaseMetadata metadata;

    QString url = QStringLiteral("https://musicbrainz.org/ws/2/release/%1"
        "?inc=recordings+artist-credits+recording-rels+work-rels+recording-level-rels+work-level-rels+discids+artist-rels").arg(releaseId);

    QByteArray data = TaskRunner::downloadUrl(url, cancelToken);
    QXmlStreamReader xml(data);
    while (!xml.atEnd())
    {
        if (xml.readNextStartElement())
        {
            if (xml.name() == QStringLiteral("release"))
            {
                if (xml.attributes().value(QStringLiteral("id")) != releaseId)
                    qWarning() << "WTF: got different release id";

                parseRelease(&xml, discid, metadata);
            }
        }
    }

    if (xml.hasError())
    {
        qWarning() << "XML parse error:" << xml.errorString();
    }

    return metadata;
}

QImage getCoverForRelease(const QString &releaseId, const TaskRunner::CancelToken &cancelToken)
{
    QString url = QStringLiteral("https://coverartarchive.org/release/%1/front-500").arg(releaseId);
    QByteArray data = TaskRunner::downloadUrl(url, cancelToken);

    if (data.size())
    {
        return QImage::fromData(data);
    }
    else
    {
        qWarning() << "No cover found";
    }

    return QImage();
}

} // namespace

namespace MusicBrainz {

ReleaseMetadata findRelease(const QString &discid, const QString &mcn, const TaskRunner::CancelToken &cancelToken)
{
    QString releaseId = getReleaseIdForDiscId(discid, mcn, cancelToken);

    if (!releaseId.size() || cancelToken.isCanceled())
        return ReleaseMetadata();

    ReleaseMetadata metadata = getMetadataForRelease(releaseId, discid, cancelToken);

    if (cancelToken.isCanceled())
        return metadata;

    metadata.cover = getCoverForRelease(releaseId, cancelToken);

    return metadata;
}

QFuture<ReleaseMetadata> findReleaseOnThread(const QString &discid, const QString &mcn)
{
    return TaskRunner::run([=](const TaskRunner::CancelToken &cancelToken, const TaskRunner::ProgressToken &) {
        return findRelease(discid, mcn, cancelToken);
    });
}

QString buildTocAttachUrl(const cdda::toc &toc)
{
    QString discid = cdda::calculate_musicbrainz_discid(toc);

    std::vector<cdda::toc_track> tracks;
    if (toc.tracks[0].index == 0) {
        // musicbrainz does not count the hidden track
        std::copy_if(toc.tracks.begin() + 1, toc.tracks.end(), std::back_inserter(tracks), [](const cdda::toc_track &t) { return t.is_audio();});
    } else {
        std::copy_if(toc.tracks.begin(), toc.tracks.end(), std::back_inserter(tracks), [](const cdda::toc_track &t) { return t.is_audio();});
    }

    QStringList tocSpec;
    tocSpec << QString::number(tracks[0].index);
    tocSpec << QString::number(tracks.size());
    tocSpec << QString::number((tracks[tracks.size()-1].start + tracks[tracks.size()-1].length).block + 150);
    for (const cdda::toc_track &track: tracks) {
        tocSpec << QString::number(track.start.block + 150);
    }

    return QStringLiteral("https://musicbrainz.org/cdtoc/attach?id=%1&tracks=%2&toc=%3")
            .arg(discid)
            .arg(tracks.size())
            .arg(tocSpec.join(QLatin1Char('+')));
}



} // namespace MusicBrainz
