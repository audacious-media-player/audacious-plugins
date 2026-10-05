/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "playback_controller.h"

#include <cmath>
#include <vector>

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include <libaudcore/drct.h>
#include <libaudcore/tuple.h>
#include <libaudqt/libaudqt.h>

namespace
{

struct LinearColor
{
    double red = 0;
    double green = 0;
    double blue = 0;
};

QString toQString(const String & string)
{
    return string ? QString::fromUtf8((const char *)string) : QString();
}

QString imageDataUrl(const QImage & image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    return QStringLiteral("data:image/png;base64,") +
           QString::fromLatin1(bytes.toBase64());
}

QRect squareCrop(const QImage & image)
{
    const int edge = qMin(image.width(), image.height());
    return {(image.width() - edge) / 2, (image.height() - edge) / 2, edge,
            edge};
}

QString roundedAlbumArtDataUrl(const QImage & image)
{
    if (image.isNull())
        return {};

    const QRect crop = squareCrop(image);
    const int edge = crop.width();
    const QRectF source_rect(crop);
    const QRectF target_rect(0, 0, edge, edge);

    QImage rounded(edge, edge, QImage::Format_ARGB32_Premultiplied);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    const qreal radius = edge * 0.04;
    path.addRoundedRect(target_rect, radius, radius);
    painter.setClipPath(path);
    painter.drawImage(target_rect, image, source_rect);
    painter.end();

    return imageDataUrl(rounded);
}

double srgbToLinear(double value)
{
    return value <= 0.04045 ? value / 12.92
                            : std::pow((value + 0.055) / 1.055, 2.4);
}

int linearToSrgb(double value)
{
    value = qBound(0.0, value, 1.0);
    const double srgb = value <= 0.0031308
                            ? value * 12.92
                            : 1.055 * std::pow(value, 1.0 / 2.4) - 0.055;
    return qRound(srgb * 255.0);
}

QString albumArtBackgroundDataUrl(const QImage & image)
{
    if (image.isNull())
        return {};

    // BlurHash represents an image as a handful of low-frequency cosine
    // components.  We can skip the textual encoding here and render those
    // components directly into a small backdrop texture.
    constexpr int sample_size = 32;
    constexpr int components_x = 4;
    constexpr int components_y = 4;
    constexpr int backdrop_width = 72;
    constexpr int backdrop_height = 128;
    constexpr double pi = 3.14159265358979323846;

    const QImage sample =
        image.copy(squareCrop(image))
            .scaled(sample_size, sample_size, Qt::IgnoreAspectRatio,
                    Qt::SmoothTransformation)
            .convertToFormat(QImage::Format_RGBA8888);

    std::vector<LinearColor> factors(components_x * components_y);
    for (int component_y = 0; component_y < components_y; component_y++)
    {
        for (int component_x = 0; component_x < components_x; component_x++)
        {
            LinearColor factor;
            for (int y = 0; y < sample.height(); y++)
            {
                for (int x = 0; x < sample.width(); x++)
                {
                    const double basis =
                        std::cos(pi * component_x * x / sample.width()) *
                        std::cos(pi * component_y * y / sample.height());
                    const QColor color = sample.pixelColor(x, y);
                    const double alpha = color.alphaF();
                    factor.red += basis * alpha * srgbToLinear(color.redF());
                    factor.green +=
                        basis * alpha * srgbToLinear(color.greenF());
                    factor.blue += basis * alpha * srgbToLinear(color.blueF());
                }
            }

            const double scale =
                (component_x == 0 && component_y == 0 ? 1.0 : 2.0) /
                (sample.width() * sample.height());
            factor.red *= scale;
            factor.green *= scale;
            factor.blue *= scale;
            factors[component_y * components_x + component_x] = factor;
        }
    }

    QImage backdrop(backdrop_width, backdrop_height, QImage::Format_RGB32);
    for (int y = 0; y < backdrop.height(); y++)
    {
        for (int x = 0; x < backdrop.width(); x++)
        {
            LinearColor color;
            for (int component_y = 0; component_y < components_y; component_y++)
            {
                for (int component_x = 0; component_x < components_x;
                     component_x++)
                {
                    const double basis =
                        std::cos(pi * component_x * x / backdrop.width()) *
                        std::cos(pi * component_y * y / backdrop.height());
                    const auto & factor =
                        factors[component_y * components_x + component_x];
                    color.red += factor.red * basis;
                    color.green += factor.green * basis;
                    color.blue += factor.blue * basis;
                }
            }

            backdrop.setPixel(x, y,
                              qRgb(linearToSrgb(color.red),
                                   linearToSrgb(color.green),
                                   linearToSrgb(color.blue)));
        }
    }

    return imageDataUrl(backdrop);
}

} // namespace

MobilePlaybackController::MobilePlaybackController(QObject * parent)
    : QObject(parent)
{
    refreshPlayback();
    refreshVolume();
    refreshSettings();
    m_timer.start();
}

void MobilePlaybackController::playPause() { aud_drct_play_pause(); }

void MobilePlaybackController::stop() { aud_drct_stop(); }

void MobilePlaybackController::previous() { aud_drct_pl_prev(); }

void MobilePlaybackController::next() { aud_drct_pl_next(); }

void MobilePlaybackController::seek(int position)
{
    if (m_ready && m_duration > 0)
        aud_drct_seek(aud::clamp(position, 0, m_duration));
}

void MobilePlaybackController::setVolume(int volume)
{
    aud_drct_set_volume_main(aud::clamp(volume, 0, 100));
    refreshVolume();
}

void MobilePlaybackController::setRepeat(bool repeat)
{
    aud_set_bool("repeat", repeat);
    refreshSettings();
}

void MobilePlaybackController::setShuffle(bool shuffle)
{
    aud_set_bool("shuffle", shuffle);
    refreshSettings();
}

void MobilePlaybackController::refreshMetadata()
{
    QString title;
    QString artist;
    QString album;
    QString album_art;
    QString album_art_background;

    if (aud_drct_get_playing())
    {
        const Tuple tuple = aud_drct_get_tuple();
        title = toQString(tuple.get_str(Tuple::Title));
        if (title.isEmpty())
            title = toQString(aud_drct_get_title());
        artist = toQString(tuple.get_str(Tuple::Artist));
        album = toQString(tuple.get_str(Tuple::Album));

        if (aud_drct_get_ready())
        {
            const QImage image =
                audqt::art_request_current(600, 600, true).toImage();
            album_art = roundedAlbumArtDataUrl(image);
            album_art_background = albumArtBackgroundDataUrl(image);
        }
    }

    if (title == m_title && artist == m_artist && album == m_album &&
        album_art == m_album_art &&
        album_art_background == m_album_art_background)
        return;

    m_title = title;
    m_artist = artist;
    m_album = album;
    m_album_art = album_art;
    m_album_art_background = album_art_background;
    emit metadataChanged();
}

void MobilePlaybackController::refreshPlayback()
{
    const bool playing = aud_drct_get_playing();
    const bool ready = aud_drct_get_ready();
    const bool paused = aud_drct_get_paused();
    const Playlist active_playlist = Playlist::active_playlist();
    const int playing_entry =
        playing && active_playlist == Playlist::playing_playlist()
            ? active_playlist.get_position()
            : -1;
    const bool changed =
        playing != m_playing || ready != m_ready || paused != m_paused;
    const bool entry_changed = playing_entry != m_playing_entry;

    m_playing = playing;
    m_ready = ready;
    m_paused = paused;
    m_playing_entry = playing_entry;

    if (changed)
        emit playbackChanged();
    if (entry_changed)
        emit playingEntryChanged();

    refreshMetadata();
    refreshPosition();
}

void MobilePlaybackController::refreshPosition()
{
    const int position = m_ready ? aud_drct_get_time() : 0;
    const int duration = m_ready ? aud_drct_get_length() : 0;
    const int safe_duration = duration < 0 ? 0 : duration;

    if (position != m_position || safe_duration != m_duration)
    {
        m_position = position;
        m_duration = safe_duration;
        emit positionChanged();
    }

    refreshVolume();
}

void MobilePlaybackController::refreshVolume()
{
    const int volume = aud_drct_get_volume_main();
    if (volume != m_volume)
    {
        m_volume = volume;
        emit volumeChanged();
    }
}

void MobilePlaybackController::refreshSettings()
{
    const bool repeat = aud_get_bool("repeat");
    const bool shuffle = aud_get_bool("shuffle");
    if (repeat == m_repeat && shuffle == m_shuffle)
        return;

    m_repeat = repeat;
    m_shuffle = shuffle;
    emit settingsChanged();
}

void MobilePlaybackController::playlistPositionChanged(Playlist playlist)
{
    Q_UNUSED(playlist)
    refreshPlayback();
}
