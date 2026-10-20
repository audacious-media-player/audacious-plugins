/*
 * Copyright 2026 Audacious developers and others
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "plugin_controller.h"

#include <functional>

#include <QWidget>

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/plugins.h>

#include "native_widget_item.h"

namespace
{

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

MobilePluginController::MobilePluginController(QObject * parent)
    : QObject(parent), m_model(this)
{
}

MobilePluginController::~MobilePluginController()
{
    destroyNativePreferenceWidgets();
    if (m_plugin_preferences && m_plugin_preferences->cleanup)
        m_plugin_preferences->cleanup();
}

bool MobilePluginController::setEnabled(int row, bool enabled)
{
    return m_model.setEnabled(row, enabled);
}

QVariantMap MobilePluginController::openPreferences(const QString & basename)
{
    if (m_preferences_plugin)
        closePreferences(
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

            auto addBinding = [this, &widget](
                                  QVariantList choices = {},
                                  QWidget * native_widget = nullptr) {
                const int id = (int)m_preference_bindings.size();
                m_preference_bindings.push_back(
                    {&widget, choices, native_widget});
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
                    heading["dependencies"] =
                        dependenciesVariant(dependencies);
                    addDescription(heading);
                    describeWidgets(tab.widgets, indent + 1, dependencies);
                }
                break;
            case PreferencesWidget::Separator:
                description["type"] = "separator";
                addDescription(description);
                break;
            case PreferencesWidget::CustomQt:
                if (widget.data.populate)
                {
                    auto native_widget =
                        static_cast<QWidget *>(widget.data.populate());
                    if (native_widget)
                    {
                        description["type"] = "native";
                        description["id"] = addBinding({}, native_widget);
                        addDescription(description);
                    }
                }
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

QQuickItem * MobilePluginController::createNativePreferenceItem(
    QQuickItem * parent, int id)
{
    if (id < 0 || id >= (int)m_preference_bindings.size())
        return nullptr;

    QWidget * widget = m_preference_bindings[id].native_widget.data();
    if (!widget)
        return nullptr;

    auto item = new NativeWidgetItem(parent);
    item->setWidget(widget);
    return item;
}

QVariant MobilePluginController::preferenceValue(
    const PreferenceBinding & binding) const
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

QVariantMap MobilePluginController::preferenceValues() const
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

void MobilePluginController::setPreference(int id, const QVariant & value)
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

void MobilePluginController::activatePreference(int id)
{
    if (id < 0 || id >= (int)m_preference_bindings.size())
        return;

    const PreferencesWidget & widget = *m_preference_bindings[id].widget;
    if (widget.type == PreferencesWidget::Button && widget.data.button.callback)
        widget.data.button.callback();
}

void MobilePluginController::closePreferences(const QString & basename,
                                              bool apply)
{
    if (!m_preferences_plugin ||
        basename !=
            QString::fromUtf8(aud_plugin_get_basename(m_preferences_plugin)))
        return;

    const PluginPreferences * preferences = m_plugin_preferences;
    if (apply && preferences && preferences->apply)
        preferences->apply();

    destroyNativePreferenceWidgets();

    m_preferences_plugin = nullptr;
    m_plugin_preferences = nullptr;
    m_preference_bindings.clear();

    if (preferences && preferences->cleanup)
        preferences->cleanup();
}

void MobilePluginController::destroyNativePreferenceWidgets()
{
    for (PreferenceBinding & binding : m_preference_bindings)
    {
        if (binding.native_widget)
            delete binding.native_widget.data();
    }
}
