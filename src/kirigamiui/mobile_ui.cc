/*
 * Copyright 2026 Audacious developers and others
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */

#include "mobile_ui.h"

#include <cmath>
#include <functional>
#include <vector>

#include <QBuffer>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>
#include <libaudcore/runtime.h>
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

QString translatedText(const char * text, const char * domain)
{
    if (!text)
        return {};

    return QString::fromUtf8(dgettext(domain, text));
}

QString translatedLabel(const char * text, const char * domain)
{
    QString translated = translatedText(text, domain);

    // Preference strings use GTK-style underscores for mnemonics.  Qt Quick
    // controls do not consume them, so leave only the visible label text.
    QString clean;
    clean.reserve(translated.size());
    for (int i = 0; i < translated.size(); i++)
    {
        if (translated[i] != QLatin1Char('_'))
            clean.append(translated[i]);
        else if (i + 1 < translated.size() &&
                 translated[i + 1] == QLatin1Char('_'))
        {
            clean.append(QLatin1Char('_'));
            i++;
        }
    }

    return clean;
}

QString pluginCategoryName(PluginType type)
{
    switch (type)
    {
    case PluginType::General:
        return QString::fromUtf8(_("General"));
    case PluginType::Effect:
        return QString::fromUtf8(_("Effect"));
    case PluginType::Vis:
        return QString::fromUtf8(_("Visualization"));
    case PluginType::Input:
        return QString::fromUtf8(_("Input"));
    case PluginType::Playlist:
        return QString::fromUtf8(_("Playlist"));
    case PluginType::Transport:
        return QString::fromUtf8(_("Transport"));
    default:
        return {};
    }
}

QVariantList dependenciesVariant(const std::vector<int> & dependencies)
{
    QVariantList list;
    for (int id : dependencies)
        list.append(id);
    return list;
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

MobilePluginModel::MobilePluginModel(QObject * parent)
    : QAbstractListModel(parent)
{
    static constexpr PluginType types[] = {
        PluginType::General, PluginType::Effect,   PluginType::Vis,
        PluginType::Input,   PluginType::Playlist, PluginType::Transport};

    for (PluginType type : types)
    {
        const QString category = pluginCategoryName(type);
        for (PluginHandle * plugin : aud_plugin_list_sorted(type))
        {
            m_entries.push_back({plugin, category});
            aud_plugin_add_watch(plugin, pluginChanged, this);
        }
    }
}

MobilePluginModel::~MobilePluginModel()
{
    for (const Entry & entry : m_entries)
        aud_plugin_remove_watch(entry.plugin, pluginChanged, this);
}

int MobilePluginModel::rowCount(const QModelIndex & parent) const
{
    return parent.isValid() ? 0 : (int)m_entries.size();
}

QVariant MobilePluginModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= (int)m_entries.size())
        return {};

    const Entry & entry = m_entries[index.row()];
    switch (role)
    {
    case NameRole:
        return QString::fromUtf8(aud_plugin_get_name(entry.plugin));
    case BasenameRole:
        return QString::fromUtf8(aud_plugin_get_basename(entry.plugin));
    case CategoryRole:
        return entry.category;
    case EnabledRole:
        return aud_plugin_get_enabled(entry.plugin);
    case HasPreferencesRole:
        return aud_plugin_has_configure(entry.plugin);
    case HasAboutRole:
        return aud_plugin_has_about(entry.plugin);
    default:
        return {};
    }
}

QHash<int, QByteArray> MobilePluginModel::roleNames() const
{
    return {{NameRole, "pluginName"},
            {BasenameRole, "basename"},
            {CategoryRole, "category"},
            {EnabledRole, "pluginEnabled"},
            {HasPreferencesRole, "hasPreferences"},
            {HasAboutRole, "hasAbout"}};
}

bool MobilePluginModel::setEnabled(int row, bool enabled)
{
    if (row < 0 || row >= (int)m_entries.size())
        return false;

    PluginHandle * plugin = m_entries[row].plugin;
    const bool success = aud_plugin_enable(plugin, enabled);
    refreshPlugin(plugin);
    return success;
}

bool MobilePluginModel::pluginChanged(PluginHandle * plugin, void * data)
{
    static_cast<MobilePluginModel *>(data)->refreshPlugin(plugin);
    return true;
}

void MobilePluginModel::refreshPlugin(PluginHandle * plugin)
{
    for (int row = 0; row < (int)m_entries.size(); row++)
    {
        if (m_entries[row].plugin == plugin)
        {
            emit dataChanged(index(row), index(row),
                             {EnabledRole, HasPreferencesRole, HasAboutRole});
            return;
        }
    }
}

MobilePlaylistModel::MobilePlaylistModel(QObject * parent)
    : QAbstractListModel(parent), m_playlist(Playlist::active_playlist()),
      m_rows(m_playlist.n_entries())
{
}

int MobilePlaylistModel::rowCount(const QModelIndex & parent) const
{
    return parent.isValid() || !m_playlist.exists() ? 0 : m_rows;
}

QVariant MobilePlaylistModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid() || !m_playlist.exists() || index.row() < 0 ||
        index.row() >= m_playlist.n_entries())
        return {};

    const int entry = index.row();
    const Tuple tuple = m_playlist.entry_tuple(entry, Playlist::NoWait);

    switch (role)
    {
    case TitleRole:
        return entryTitle(m_playlist, entry, tuple);
    case ArtistRole:
        return toQString(tuple.get_str(Tuple::Artist));
    case AlbumRole:
        return toQString(tuple.get_str(Tuple::Album));
    case DurationRole:
    {
        const int length = tuple.get_int(Tuple::Length);
        if (length < 0)
            return QStringLiteral("--:--");

        auto duration = str_format_time(length);
        return QString::fromUtf8((const char *)duration);
    }
    case PlayingRole:
        return m_playlist == Playlist::playing_playlist() &&
               entry == m_playlist.get_position();
    case QueuePositionRole:
    {
        const int queue_position = m_playlist.queue_find_entry(entry);
        return queue_position < 0 ? 0 : queue_position + 1;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> MobilePlaylistModel::roleNames() const
{
    return {{TitleRole, "title"},     {ArtistRole, "artist"},
            {AlbumRole, "album"},     {DurationRole, "duration"},
            {PlayingRole, "playing"}, {QueuePositionRole, "queuePosition"}};
}

void MobilePlaylistModel::setPlaylist(Playlist playlist)
{
    if (playlist == m_playlist)
        return;

    beginResetModel();
    m_playlist = playlist;
    m_rows = m_playlist.exists() ? m_playlist.n_entries() : 0;
    endResetModel();
}

void MobilePlaylistModel::refresh()
{
    beginResetModel();
    m_rows = m_playlist.exists() ? m_playlist.n_entries() : 0;
    endResetModel();
}

void MobilePlaylistModel::applyPendingUpdate()
{
    if (!m_playlist.exists())
        return;

    const auto update = m_playlist.update_detail();
    if (update.level == Playlist::NoUpdate)
        return;

    const int rows = m_playlist.n_entries();
    const int changed = rows - update.before - update.after;

    if (update.level == Playlist::Structure)
    {
        const int removed = m_rows - update.before - update.after;
        if (removed > 0)
        {
            beginRemoveRows({}, update.before, update.before + removed - 1);
            m_rows -= removed;
            endRemoveRows();
        }

        if (changed > 0)
        {
            beginInsertRows({}, update.before, update.before + changed - 1);
            m_rows += changed;
            endInsertRows();
        }

        if (m_rows != rows)
            refresh();
    }
    else if (update.level == Playlist::Metadata && changed > 0)
    {
        emit dataChanged(index(update.before),
                         index(update.before + changed - 1));
    }

    if (update.queue_changed && m_rows > 0)
        emit dataChanged(index(0), index(m_rows - 1), {QueuePositionRole});
}

void MobilePlaylistModel::refreshPlayback()
{
    if (rowCount() > 0)
        emit dataChanged(index(0), index(rowCount() - 1), {PlayingRole});
}

void MobilePlaylistModel::refreshEntry(int entry)
{
    if (entry >= 0 && entry < rowCount())
        emit dataChanged(index(entry), index(entry));
}

MobileUiController::MobileUiController(QObject * parent)
    : QObject(parent), m_playlist_model(this), m_plugin_model(this)
{
    refreshPlaylists();
    refreshPlayback();
    refreshVolume();
    refreshSettings();
    m_timer.start();
}

MobileUiController::~MobileUiController()
{
    if (m_plugin_preferences && m_plugin_preferences->cleanup)
        m_plugin_preferences->cleanup();
}

int MobileUiController::activePlaylistIndex() const
{
    return Playlist::active_playlist().index();
}

QString MobileUiController::playlistTitle() const
{
    return toQString(Playlist::active_playlist().get_title());
}

QString MobileUiController::applicationVersion() const
{
    return QStringLiteral(VERSION);
}

QString MobileUiController::copyrightText() const
{
    return QStringLiteral(COPYRIGHT);
}

void MobileUiController::playPause() { aud_drct_play_pause(); }

void MobileUiController::stop() { aud_drct_stop(); }

void MobileUiController::previous() { aud_drct_pl_prev(); }

void MobileUiController::next() { aud_drct_pl_next(); }

void MobileUiController::seek(int position)
{
    if (m_ready && m_duration > 0)
        aud_drct_seek(aud::clamp(position, 0, m_duration));
}

void MobileUiController::setVolume(int volume)
{
    aud_drct_set_volume_main(aud::clamp(volume, 0, 100));
    refreshVolume();
}

void MobileUiController::setRepeat(bool repeat)
{
    aud_set_bool("repeat", repeat);
    refreshSettings();
}

void MobileUiController::setShuffle(bool shuffle)
{
    aud_set_bool("shuffle", shuffle);
    refreshSettings();
}

void MobileUiController::playEntry(int entry)
{
    auto playlist = Playlist::active_playlist();
    if (entry < 0 || entry >= playlist.n_entries())
        return;

    playlist.set_position(entry);
    playlist.start_playback();
}

void MobileUiController::toggleQueued(int entry)
{
    auto playlist = Playlist::active_playlist();
    if (entry < 0 || entry >= playlist.n_entries())
        return;

    const int queue_position = playlist.queue_find_entry(entry);
    if (queue_position >= 0)
        playlist.queue_remove(queue_position);
    else
        playlist.queue_insert(-1, entry);

    m_playlist_model.refreshEntry(entry);
}

void MobileUiController::removeEntry(int entry)
{
    auto playlist = Playlist::active_playlist();
    if (entry >= 0 && entry < playlist.n_entries())
        playlist.remove_entry(entry);
}

void MobileUiController::activatePlaylist(int index)
{
    if (index >= 0 && index < Playlist::n_playlists())
        Playlist::by_index(index).activate();
}

void MobileUiController::newPlaylist() { Playlist::new_playlist(); }

void MobileUiController::openFiles()
{
    audqt::fileopener_show(audqt::FileMode::Open);
}

void MobileUiController::addFiles()
{
    audqt::fileopener_show(audqt::FileMode::Add);
}

void MobileUiController::showPreferences() { emit preferencesRequested(); }

void MobileUiController::hidePreferences() { emit preferencesDismissed(); }

void MobileUiController::showAbout() { emit aboutRequested(); }

void MobileUiController::hideAbout() { emit aboutDismissed(); }

void MobileUiController::requestQuit()
{
    bool handled = false;
    hook_call("window close", &handled);
    if (!handled)
        aud_quit();
}

bool MobileUiController::boolSetting(const QString & name) const
{
    const QByteArray key = name.toUtf8();
    return aud_get_bool(key.constData());
}

int MobileUiController::intSetting(const QString & name) const
{
    const QByteArray key = name.toUtf8();
    return aud_get_int(key.constData());
}

void MobileUiController::setBoolSetting(const QString & name, bool value)
{
    const QByteArray key = name.toUtf8();
    aud_set_bool(key.constData(), value);

    if (name == QStringLiteral("leading_zero") ||
        name == QStringLiteral("show_hours") ||
        name == QStringLiteral("show_numbers_in_pl"))
        hook_call("title change", nullptr);
}

void MobileUiController::setIntSetting(const QString & name, int value)
{
    const QByteArray key = name.toUtf8();
    aud_set_int(key.constData(), value);
}

bool MobileUiController::setPluginEnabled(int row, bool enabled)
{
    return m_plugin_model.setEnabled(row, enabled);
}

QVariantMap MobileUiController::openPluginPreferences(const QString & basename)
{
    if (m_preferences_plugin)
        closePluginPreferences(
            QString::fromUtf8(aud_plugin_get_basename(m_preferences_plugin)),
            false);

    QVariantMap details;
    PluginHandle * plugin =
        aud_plugin_lookup_basename(basename.toUtf8().constData());
    if (!plugin || !aud_plugin_get_enabled(plugin))
        return details;

    auto header = (Plugin *)aud_plugin_get_header(plugin);
    if (!header)
        return details;

    m_preferences_plugin = plugin;
    m_plugin_preferences = header->info.prefs;
    m_preference_bindings.clear();

    if (m_plugin_preferences && m_plugin_preferences->init)
        m_plugin_preferences->init();

    details["name"] = translatedText(header->info.name, header->info.domain);
    details["about"] = translatedText(header->info.about, header->info.domain);
    details["hasPreferences"] = (m_plugin_preferences != nullptr);
    details["requiresApply"] =
        (m_plugin_preferences && m_plugin_preferences->apply);

    QVariantList descriptions;
    const char * domain = header->info.domain;

    auto addDescription = [&descriptions](QVariantMap description) {
        descriptions.append(description);
    };

    std::function<void(ArrayRef<PreferencesWidget>, int, std::vector<int>)>
        describeWidgets;
    describeWidgets = [this, domain, &addDescription, &describeWidgets](
                          ArrayRef<PreferencesWidget> widgets, int indent,
                          std::vector<int> inherited_dependencies) {
        int parent_id = -1;

        for (const PreferencesWidget & widget : widgets)
        {
            std::vector<int> dependencies = inherited_dependencies;
            if (widget.child && parent_id >= 0)
                dependencies.push_back(parent_id);
            else if (!widget.child)
                parent_id = -1;

            QVariantMap description;
            description["indent"] = indent + (widget.child ? 1 : 0);
            description["dependencies"] = dependenciesVariant(dependencies);

            const QString label = translatedLabel(widget.label, domain);

            auto addBinding = [this, &widget](QVariantList choices = {}) {
                const int id = (int)m_preference_bindings.size();
                m_preference_bindings.push_back({&widget, choices});
                return id;
            };

            switch (widget.type)
            {
            case PreferencesWidget::Label:
            {
                const bool heading = label.contains("<b>");
                description["type"] = heading ? "heading" : "label";
                QString clean_label = label;
                if (heading)
                {
                    clean_label.replace("<b>", "", Qt::CaseInsensitive);
                    clean_label.replace("</b>", "", Qt::CaseInsensitive);
                }
                description["label"] = clean_label;
                addDescription(description);
                break;
            }
            case PreferencesWidget::Button:
                description["type"] = "button";
                description["label"] = label;
                description["icon"] = QString::fromUtf8(
                    widget.data.button.icon ? widget.data.button.icon : "");
                description["id"] = addBinding();
                addDescription(description);
                break;
            case PreferencesWidget::CheckButton:
            {
                description["type"] = "check";
                description["label"] = label;
                const int id = addBinding();
                description["id"] = id;
                addDescription(description);
                if (!widget.child)
                    parent_id = id;
                break;
            }
            case PreferencesWidget::RadioButton:
            {
                description["type"] = "radio";
                description["label"] = label;
                const int id = addBinding();
                description["id"] = id;
                addDescription(description);
                if (!widget.child)
                    parent_id = id;
                break;
            }
            case PreferencesWidget::SpinButton:
                description["type"] = widget.cfg.type == WidgetConfig::Float
                                          ? "double"
                                          : "integer";
                description["label"] = label;
                description["minimum"] = widget.data.spin_btn.min;
                description["maximum"] = widget.data.spin_btn.max;
                description["step"] = widget.data.spin_btn.step;
                description["suffix"] =
                    translatedLabel(widget.data.spin_btn.right_label, domain);
                description["id"] = addBinding();
                addDescription(description);
                break;
            case PreferencesWidget::Entry:
                description["type"] = "string";
                description["label"] = label;
                description["password"] = widget.data.entry.password;
                description["id"] = addBinding();
                addDescription(description);
                break;
            case PreferencesWidget::FileEntry:
                description["type"] = "file";
                description["label"] = label;
                description["id"] = addBinding();
                addDescription(description);
                break;
            case PreferencesWidget::FontButton:
                description["type"] = "font";
                description["label"] = label;
                description["id"] = addBinding();
                addDescription(description);
                break;
            case PreferencesWidget::ComboBox:
            {
                QVariantList choices;
                ArrayRef<ComboItem> items = widget.data.combo.elems;
                if (widget.data.combo.fill)
                    items = widget.data.combo.fill();

                for (const ComboItem & item : items)
                {
                    QVariantMap choice;
                    choice["label"] = translatedLabel(item.label, domain);
                    choice["value"] = widget.cfg.type == WidgetConfig::String
                                          ? QVariant(QString::fromUtf8(
                                                item.str ? item.str : ""))
                                          : QVariant(item.num);
                    choices.append(choice);
                }

                description["type"] = "combo";
                description["label"] = label;
                description["choices"] = choices;
                description["id"] = addBinding(choices);
                addDescription(description);
                break;
            }
            case PreferencesWidget::Box:
                describeWidgets(widget.data.box.widgets, indent + 1,
                                dependencies);
                break;
            case PreferencesWidget::Table:
                describeWidgets(widget.data.table.widgets, indent + 1,
                                dependencies);
                break;
            case PreferencesWidget::Notebook:
                for (const NotebookTab & tab : widget.data.notebook.tabs)
                {
                    QVariantMap heading;
                    heading["type"] = "heading";
                    heading["label"] = translatedLabel(tab.name, domain);
                    heading["indent"] = indent;
                    heading["dependencies"] = dependenciesVariant(dependencies);
                    addDescription(heading);
                    describeWidgets(tab.widgets, indent + 1, dependencies);
                }
                break;
            case PreferencesWidget::Separator:
                description["type"] = "separator";
                addDescription(description);
                break;
            case PreferencesWidget::CustomQt:
                description["type"] = "unsupported";
                description["label"] = QString::fromUtf8(
                    _("This plugin uses a custom desktop settings widget."));
                addDescription(description);
                break;
            case PreferencesWidget::CustomGTK:
                // As in the desktop Qt builder, ignore GTK-only widgets.
                break;
            }
        }
    };

    if (m_plugin_preferences)
        describeWidgets(m_plugin_preferences->widgets, 0, {});

    details["preferences"] = descriptions;
    return details;
}

QVariant
MobileUiController::preferenceValue(const PreferenceBinding & binding) const
{
    const PreferencesWidget & widget = *binding.widget;
    switch (widget.type)
    {
    case PreferencesWidget::CheckButton:
        return widget.cfg.get_bool();
    case PreferencesWidget::RadioButton:
        return widget.cfg.get_int() == widget.data.radio_btn.value;
    case PreferencesWidget::SpinButton:
        return widget.cfg.type == WidgetConfig::Float
                   ? QVariant(widget.cfg.get_float())
                   : QVariant(widget.cfg.get_int());
    case PreferencesWidget::Entry:
    case PreferencesWidget::FileEntry:
    case PreferencesWidget::FontButton:
    {
        String value = widget.cfg.get_string();
        return value ? QVariant(QString::fromUtf8((const char *)value))
                     : QVariant(QString());
    }
    case PreferencesWidget::ComboBox:
    {
        QVariant value;
        if (widget.cfg.type == WidgetConfig::String)
        {
            String string = widget.cfg.get_string();
            value = QString::fromUtf8(string ? (const char *)string : "");
        }
        else
            value = widget.cfg.get_int();

        for (int index = 0; index < binding.choices.size(); index++)
        {
            if (binding.choices[index].toMap()["value"] == value)
                return index;
        }
        return -1;
    }
    default:
        return {};
    }
}

QVariantMap MobileUiController::pluginPreferenceValues() const
{
    QVariantMap values;
    for (int id = 0; id < (int)m_preference_bindings.size(); id++)
    {
        QVariant value = preferenceValue(m_preference_bindings[id]);
        if (value.isValid())
            values[QString::number(id)] = value;
    }
    return values;
}

void MobileUiController::setPluginPreference(int id, const QVariant & value)
{
    if (id < 0 || id >= (int)m_preference_bindings.size())
        return;

    const PreferenceBinding & binding = m_preference_bindings[id];
    const PreferencesWidget & widget = *binding.widget;
    switch (widget.type)
    {
    case PreferencesWidget::CheckButton:
        widget.cfg.set_bool(value.toBool());
        break;
    case PreferencesWidget::RadioButton:
        if (value.toBool())
            widget.cfg.set_int(widget.data.radio_btn.value);
        break;
    case PreferencesWidget::SpinButton:
        if (widget.cfg.type == WidgetConfig::Float)
            widget.cfg.set_float(value.toDouble());
        else
            widget.cfg.set_int(value.toInt());
        break;
    case PreferencesWidget::Entry:
    case PreferencesWidget::FileEntry:
    case PreferencesWidget::FontButton:
        widget.cfg.set_string(value.toString().toUtf8().constData());
        break;
    case PreferencesWidget::ComboBox:
    {
        const int index = value.toInt();
        if (index < 0 || index >= binding.choices.size())
            break;
        const QVariant selected = binding.choices[index].toMap()["value"];
        if (widget.cfg.type == WidgetConfig::String)
            widget.cfg.set_string(selected.toString().toUtf8().constData());
        else
            widget.cfg.set_int(selected.toInt());
        break;
    }
    default:
        break;
    }
}

void MobileUiController::activatePluginPreference(int id)
{
    if (id < 0 || id >= (int)m_preference_bindings.size())
        return;

    const PreferencesWidget & widget = *m_preference_bindings[id].widget;
    if (widget.type == PreferencesWidget::Button && widget.data.button.callback)
        widget.data.button.callback();
}

void MobileUiController::closePluginPreferences(const QString & basename,
                                                bool apply)
{
    if (!m_preferences_plugin ||
        basename !=
            QString::fromUtf8(aud_plugin_get_basename(m_preferences_plugin)))
        return;

    const PluginPreferences * preferences = m_plugin_preferences;
    m_preferences_plugin = nullptr;
    m_plugin_preferences = nullptr;
    m_preference_bindings.clear();

    if (apply && preferences && preferences->apply)
        preferences->apply();
    if (preferences && preferences->cleanup)
        preferences->cleanup();
}

void MobileUiController::refreshPlaylists(bool reset_model)
{
    QStringList names;
    for (int i = 0; i < Playlist::n_playlists(); i++)
        names.append(toQString(Playlist::by_index(i).get_title()));

    if (reset_model)
        m_playlist_model.setPlaylist(Playlist::active_playlist());

    if (names != m_playlist_names || reset_model)
    {
        m_playlist_names = names;
        emit playlistsChanged();
    }
}

void MobileUiController::refreshMetadata()
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

void MobileUiController::refreshPlayback()
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

    m_playlist_model.refreshPlayback();
    refreshMetadata();
    refreshPosition();
}

void MobileUiController::refreshPosition()
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

void MobileUiController::refreshVolume()
{
    const int volume = aud_drct_get_volume_main();
    if (volume != m_volume)
    {
        m_volume = volume;
        emit volumeChanged();
    }
}

void MobileUiController::refreshSettings()
{
    const bool repeat = aud_get_bool("repeat");
    const bool shuffle = aud_get_bool("shuffle");
    if (repeat == m_repeat && shuffle == m_shuffle)
        return;

    m_repeat = repeat;
    m_shuffle = shuffle;
    emit settingsChanged();
}

void MobileUiController::playlistActivated()
{
    refreshPlaylists();
    refreshPlayback();
}

void MobileUiController::playlistUpdated(Playlist::UpdateLevel level)
{
    m_playlist_model.applyPendingUpdate();
    if (level >= Playlist::Metadata)
        refreshPlaylists(false);
}

void MobileUiController::playlistPositionChanged(Playlist playlist)
{
    Q_UNUSED(playlist)
    refreshPlayback();
}
