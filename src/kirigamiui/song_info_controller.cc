/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "song_info_controller.h"

#include <limits>

#include <QDateTime>
#include <QLocale>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/playlist.h>
#include <libaudcore/probe.h>

namespace
{

struct SongInfoField
{
    const char * label;
    Tuple::Field field;
    bool editable;
    bool multiline;
};

constexpr SongInfoField metadata_fields[] = {
    {N_("Title"), Tuple::Title, true, false},
    {N_("Artist"), Tuple::Artist, true, false},
    {N_("Album"), Tuple::Album, true, false},
    {N_("Album Artist"), Tuple::AlbumArtist, true, false},
    {N_("Track Number"), Tuple::Track, true, false},
    {N_("Genre"), Tuple::Genre, true, false},
    {N_("Comment"), Tuple::Comment, true, true},
    {N_("Description"), Tuple::Description, true, true},
    {N_("Composer"), Tuple::Composer, true, false},
    {N_("Performer"), Tuple::Performer, true, false},
    {N_("Recording Year"), Tuple::Year, true, false},
    {N_("Recording Date"), Tuple::Date, true, false},
    {N_("Publisher"), Tuple::Publisher, false, false},
    {N_("Catalog Number"), Tuple::CatalogNum, false, false},
    {N_("Disc Number"), Tuple::Disc, true, false}};

constexpr SongInfoField technical_fields[] = {
    {N_("Length"), Tuple::Length, false, false},
    {N_("Codec"), Tuple::Codec, false, false},
    {N_("Quality"), Tuple::Quality, false, false},
    {N_("Bitrate"), Tuple::Bitrate, false, false},
    {N_("Channels"), Tuple::Channels, false, false},
    {N_("MusicBrainz ID"), Tuple::MusicBrainzID, false, false},
    {N_("File Created"), Tuple::FileCreated, false, false},
    {N_("File Modified"), Tuple::FileModified, false, false}};

QString toQString(const String & string)
{
    return string ? QString::fromUtf8((const char *)string) : QString();
}

QString fieldValue(const Tuple & tuple, Tuple::Field field,
                   bool display_value)
{
    switch (tuple.get_value_type(field))
    {
    case Tuple::String:
        return toQString(tuple.get_str(field));
    case Tuple::Int:
    {
        const int value = tuple.get_int(field);
        if (display_value && field == Tuple::Length)
            return value > 0
                       ? QString::fromUtf8((const char *)str_format_time(value))
                       : QString();
        if (display_value && field == Tuple::Bitrate)
            return value > 0 ? QStringLiteral("%1 kbps").arg(value)
                             : QString();
        return QString::number(value);
    }
    case Tuple::DateTime:
    {
        const int64_t value = tuple.get_int64(field);
        return value > 0
                   ? QLocale().toString(
                         QDateTime::fromSecsSinceEpoch(value).toLocalTime(),
                         QLocale::ShortFormat)
                   : QString();
    }
    default:
        return {};
    }
}

void setFieldValue(Tuple & tuple, Tuple::Field field, const QString & value)
{
    if (value.isEmpty())
        tuple.unset(field);
    else if (Tuple::field_get_type(field) == Tuple::String)
        tuple.set_str(field, value.toUtf8().constData());
    else if (Tuple::field_get_type(field) == Tuple::Int)
        tuple.set_int(field, value.toInt());
}

QString entryTitle(Playlist playlist, int entry, const Tuple & tuple)
{
    auto title = tuple.get_str(Tuple::Title);
    if (!title)
        title = tuple.get_str(Tuple::FormattedTitle);
    if (title)
        return QString::fromUtf8((const char *)title);

    auto filename = playlist.entry_filename(entry);
    auto basename = uri_get_display_base(filename);
    return QString::fromUtf8((const char *)basename);
}

} // namespace

QVariantMap MobileSongInfoController::open(int entry)
{
    QVariantMap details;
    const Playlist playlist = entry < 0 ? Playlist::playing_playlist()
                                        : Playlist::active_playlist();
    if (entry < 0 && playlist.exists())
        entry = playlist.get_position();

    if (entry < 0 || entry >= playlist.n_entries())
    {
        details["error"] = QString::fromUtf8(
            _("The selected song is no longer available."));
        return details;
    }

    const String filename = playlist.entry_filename(entry);
    String error;
    PluginHandle * decoder =
        filename ? playlist.entry_decoder(entry, Playlist::Wait, &error)
                 : nullptr;
    Tuple tuple = decoder ? playlist.entry_tuple(entry, Playlist::Wait, &error)
                          : Tuple();

    if (!filename || !decoder || !tuple.valid())
    {
        details["error"] =
            error ? toQString(error)
                  : QString::fromUtf8(
                        _("Song information could not be loaded."));
        return details;
    }

    tuple.delete_fallbacks();

    if (m_next_session == std::numeric_limits<int>::max())
        m_next_session = 1;
    else
        m_next_session++;

    m_session = m_next_session;
    m_filename = QString::fromUtf8((const char *)filename);
    m_decoder = decoder;
    m_tuple = tuple.ref();
    m_can_write = aud_file_can_write_tuple(filename, decoder) &&
                  !tuple.is_set(Tuple::StartTime);

    QVariantList fields;
    auto append_fields = [&fields, &tuple, this](const char * section,
                                                  const SongInfoField * specs,
                                                  int count) {
        QVariantMap heading;
        heading["type"] = "section";
        heading["label"] = QString::fromUtf8(_(section));
        fields.append(heading);

        for (int i = 0; i < count; i++)
        {
            const SongInfoField & spec = specs[i];
            QVariantMap field;
            field["type"] = "field";
            field["id"] = (int)spec.field;
            field["label"] = QString::fromUtf8(_(spec.label));
            field["value"] = fieldValue(tuple, spec.field, !spec.editable);
            field["editable"] = m_can_write && spec.editable;
            field["multiline"] = spec.multiline;
            field["numeric"] =
                Tuple::field_get_type(spec.field) == Tuple::Int;
            fields.append(field);
        }
    };

    append_fields(N_("Metadata"), metadata_fields,
                  aud::n_elems(metadata_fields));
    append_fields(N_("Technical"), technical_fields,
                  aud::n_elems(technical_fields));

    details["session"] = m_session;
    details["title"] = entryTitle(playlist, entry, tuple);
    details["canWrite"] = m_can_write;
    details["fields"] = fields;
    return details;
}

bool MobileSongInfoController::save(int session, const QVariantMap & values)
{
    if (session != m_session || !m_can_write || !m_decoder ||
        m_filename.isEmpty())
        return false;

    Tuple updated = m_tuple.ref();
    for (const SongInfoField & spec : metadata_fields)
    {
        if (!spec.editable)
            continue;

        const auto value = values.constFind(QString::number((int)spec.field));
        if (value != values.constEnd())
            setFieldValue(updated, spec.field, value->toString());
    }

    const QByteArray filename = m_filename.toUtf8();
    if (!aud_file_write_tuple(filename.constData(), m_decoder, updated))
        return false;

    m_tuple = updated.ref();
    return true;
}

void MobileSongInfoController::close(int session)
{
    if (session != m_session)
        return;

    m_session = 0;
    m_filename.clear();
    m_decoder = nullptr;
    m_tuple = Tuple();
    m_can_write = false;
}
