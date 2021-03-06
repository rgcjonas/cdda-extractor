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

#include "tracklistmodel.h"

#include <QApplication>
#include <QPalette>
#include <QBuffer>

TrackListModel::TrackListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

QVariant TrackListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
        switch (section)
        {
        case COLUMN_TRACKNO:
            return tr("Track");
        case COLUMN_TITLE:
            return tr("Title");
        case COLUMN_ARTIST:
            return tr("Artist");
        case COLUMN_COMPOSER:
            return tr("Composer");
        case COLUMN_LENGTH:
            return tr("Length");
        }
    }

    return QVariant();
}

bool TrackListModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if (value != headerData(section, orientation, role)) {
        // FIXME: Implement me!
        emit headerDataChanged(orientation, section, section);
        return true;
    }
    return false;
}


int TrackListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return int(m_data.size());
}

int TrackListModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return COLUMN_LAST - COLUMN_FIRST + 1;
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int i = index.row();
    if (i < 0 || i >= (int)m_data.size())
        return QVariant();

    if (role == Qt::DisplayRole || role == Qt::ToolTipRole)
    {
        switch (index.column())
        {
        case COLUMN_TRACKNO:
            return m_data[i].trackno;
        case COLUMN_TITLE:
            return trackTitle(i);
        case COLUMN_ARTIST:
            return trackArtist(i);
        case COLUMN_COMPOSER:
            return trackComposer(i);
        case COLUMN_LENGTH:
            return m_data[i].tocdata.length.to_display();
        }
    }
    else if (role == Qt::EditRole)
    {
        switch (index.column())
        {
        case COLUMN_TRACKNO:
            return m_data[i].trackno;
        case COLUMN_TITLE:
            return m_data[i].title;
        case COLUMN_ARTIST:
            return m_data[i].artist;
        case COLUMN_COMPOSER:
            return m_data[i].composer;
        case COLUMN_LENGTH:
            return m_data[i].tocdata.length.to_display();
        }
    }
    else if (role == Qt::CheckStateRole)
    {
        switch (index.column())
        {
        case COLUMN_TRACKNO:
            return m_data[i].selected ? Qt::Checked : Qt::Unchecked;
        }
    }
    else if (role == Qt::TextAlignmentRole)
    {
        switch (index.column())
        {
        case COLUMN_TRACKNO:
            // so for whatever reason, Qt::Alignment cannot be stuffed into a variant, but ints work just fine
            return int(Qt::AlignVCenter) | int(Qt::AlignRight);
        }
    }
    else if (role == Qt::ForegroundRole)
    {
        QColor text = qApp->palette("QTableView").text().color();

        switch (index.column())
        {
        case COLUMN_TITLE:
            if (!m_data[i].title.length())
                text.setAlpha(128);
            return text;
        case COLUMN_ARTIST:
            if (!m_data[i].artist.size())
                text.setAlpha(128);
            return text;
        case COLUMN_COMPOSER:
            if (!m_data[i].composer.size())
                text.setAlpha(128);
            return text;
        }
    }

    return QVariant();
}

bool TrackListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid())
        return false;

    int i = index.row();
    if (i < 0 || i >= (int)m_data.size())
        return false;

    if (role == Qt::CheckStateRole)
    {
        m_data[i].selected = value == Qt::Checked;
    }
    else if (role == Qt::EditRole)
    {
        switch (index.column())
        {
        case COLUMN_TITLE:
            m_data[i].title = value.toString();
            break;
        case COLUMN_ARTIST:
            m_data[i].artist = value.toString();
            break;
        case COLUMN_COMPOSER:
            m_data[i].composer = value.toString();
            break;
        default:
            return false;
        }
    }
    else
    {
        return false;
    }

    emit dataChanged(index, index, QVector<int>() << role);
    return true;
}

Qt::ItemFlags TrackListModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    int i = index.row();
    if (i < 0 || i >= (int)m_data.size())
        return Qt::NoItemFlags;

    if (!m_data[i].tocdata.is_audio())
    {
        return Qt::NoItemFlags;
    }

    switch (index.column())
    {
    case COLUMN_TRACKNO:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    case COLUMN_TITLE:
    case COLUMN_ARTIST:
    case COLUMN_COMPOSER:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    case COLUMN_LENGTH:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    default:
        return Qt::NoItemFlags;
    }
}

void TrackListModel::reset(const std::vector<cdda::toc_track> &tracks)
{
    beginResetModel();

    m_data.clear();
    m_data.resize(tracks.size());

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        m_data[i].selected = tracks[i].is_audio();
        m_data[i].trackno = tracks[i].index;
        m_data[i].tocdata = tracks[i];
        m_data[i].artist = tracks[i].artist;
        m_data[i].title = tracks[i].title;
    }

    endResetModel();
}

void TrackListModel::clear()
{
    beginResetModel();

    m_data.clear();

    m_device.clear();

    m_albumArtist.clear();
    m_albumComposer.clear();
    m_albumTitle.clear();
    m_albumGenre.clear();
    m_albumYear.clear();
    m_albumDiscNo.clear();
    m_albumCover = QImage();
    m_albumCoverPng.clear();

    endResetModel();
}

void TrackListModel::setDevice(const QString &device)
{
    m_device = device;
}

void TrackListModel::setAlbumArtist(const QString &artist)
{
    m_albumArtist = artist;
    for (int i = 0; i < (int)m_data.size(); ++i)
    {
        if (m_data[i].artist.size())
            continue;

        QModelIndex index = this->createIndex(i, COLUMN_ARTIST);
        emit dataChanged(index, index);
    }
}

void TrackListModel::setAlbumComposer(const QString &composer)
{
    m_albumComposer = composer;
    for (int i = 0; i < (int)m_data.size(); ++i)
    {
        if (m_data[i].composer.size())
            continue;

        QModelIndex index = this->createIndex(i, COLUMN_COMPOSER);
        emit dataChanged(index, index);
    }
}

void TrackListModel::setAlbumTitle(const QString &title)
{
    m_albumTitle = title;
}

void TrackListModel::setAlbumGenre(const QString &genre)
{
    m_albumGenre = genre;
}

void TrackListModel::setAlbumYear(const QString &year)
{
    m_albumYear = year;
}

void TrackListModel::setAlbumDiscNo(const QString &no)
{
    m_albumDiscNo = no;
}

void TrackListModel::setAlbumCover(const QImage &image)
{
    m_albumCover = image;
    m_albumCoverPng.clear();

    QBuffer buffer(&m_albumCoverPng);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
}

void TrackListModel::setAlbumCatalogNo(const QString &no)
{
    m_albumCatalogNo = no;
}

QString TrackListModel::trackISRC(int i) const
{
    return m_data[i].tocdata.isrc;
}

QString TrackListModel::trackTitle(int i) const
{
    if (!m_data[i].tocdata.is_audio())
        return tr("(Data Track %1)").arg(m_data[i].trackno);
    if (m_data[i].title.size())
        return m_data[i].title;
    if (m_data[i].trackno < 1)
        return tr("Hidden track %1").arg(m_data[i].trackno);
    return tr("Track %1").arg(m_data[i].trackno);
}

QString TrackListModel::trackArtist(int i) const
{
    if (!m_data[i].tocdata.is_audio())
        return QString();

    return m_data[i].artist.size() ? m_data[i].artist :
            (m_albumArtist.size() ? m_albumArtist : tr("Unknown Artist"));
}

QString TrackListModel::trackComposer(int i) const
{
    if (!m_data[i].tocdata.is_audio())
        return QString();

    return m_data[i].composer.size() ? m_data[i].composer :
            (m_albumComposer.size() ? m_albumComposer : tr("Unknown Composer"));
}

cdda::track_metadata TrackListModel::trackMetadata(int i) const
{
    cdda::track_metadata m;
    m.trackno = trackNo(i);
    m.tracktotal = (int)m_data.size();
    m.album = m_albumTitle;
    m.artist = m_data[i].artist.size() ? m_data[i].artist : m_albumArtist;
    m.composer = m_data[i].composer.size() ? m_data[i].composer : m_albumComposer;
    m.discNo = m_albumDiscNo;
    m.genre = m_albumGenre;
    m.title = m_data[i].title;
    m.year = m_albumYear;
    m.isrc = m_data[i].isrc;
    m.coverPng = m_albumCoverPng;

    return m;
}

int TrackListModel::trackIndexForTrackno(int trackno) const
{
    for (int i = 0; i < int(m_data.size()); ++i)
    {
        if (m_data[i].trackno == trackno)
            return i;
    }

    return -1;
}

void TrackListModel::setTrackTitle(int i, const QString &title)
{
    Q_ASSERT(i >= 0 && i < int(m_data.size()));
    m_data[i].title = title;

    auto modelIndex = createIndex(i, COLUMN_TITLE);
    emit dataChanged(modelIndex, modelIndex);
}

void TrackListModel::setTrackArtist(int i, const QString &artist)
{
    Q_ASSERT(i >= 0 && i < int(m_data.size()));
    m_data[i].artist = artist;

    auto modelIndex = createIndex(i, COLUMN_ARTIST);
    emit dataChanged(modelIndex, modelIndex);
}

void TrackListModel::setTrackComposer(int i, const QString &composer)
{
    Q_ASSERT(i >= 0 && i < int(m_data.size()));
    m_data[i].composer = composer;

    auto modelIndex = createIndex(i, COLUMN_COMPOSER);
    emit dataChanged(modelIndex, modelIndex);
}

QString TrackListModel::musicbrainzDiscId() const
{
    return cdda::calculate_musicbrainz_discid(toc());
}

cdda::toc TrackListModel::toc() const
{
    cdda::toc toc;
    for (auto t: m_data) {
        toc.tracks.push_back(t.tocdata);
    }
    return toc;
}
