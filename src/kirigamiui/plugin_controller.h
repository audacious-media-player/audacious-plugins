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

#ifndef AUDACIOUS_KIRIGAMI_PLUGIN_CONTROLLER_H
#define AUDACIOUS_KIRIGAMI_PLUGIN_CONTROLLER_H

#include <vector>

#include <QAbstractListModel>
#include <QPointer>
#include <QQuickItem>
#include <QVariant>

#include <libaudcore/preferences.h>

class PluginHandle;
class QWidget;

class MobilePluginModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        NameRole = Qt::UserRole + 1,
        BasenameRole,
        CategoryRole,
        EnabledRole,
        HasPreferencesRole,
        HasAboutRole
    };

    explicit MobilePluginModel(QObject * parent = nullptr);
    ~MobilePluginModel() override;

    int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    QVariant data(const QModelIndex & index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool setEnabled(int row, bool enabled);

private:
    struct Entry
    {
        PluginHandle * plugin;
        QString category;
    };

    static bool pluginChanged(PluginHandle * plugin, void * data);
    void refreshPlugin(PluginHandle * plugin);

    std::vector<Entry> m_entries;
};

class MobilePluginController : public QObject
{
public:
    explicit MobilePluginController(QObject * parent = nullptr);
    ~MobilePluginController() override;

    QAbstractItemModel * model() { return &m_model; }
    bool setEnabled(int row, bool enabled);
    QVariantMap openPreferences(const QString & basename);
    QVariantMap preferenceValues() const;
    void setPreference(int id, const QVariant & value);
    void activatePreference(int id);
    void closePreferences(const QString & basename, bool apply);
    QQuickItem * createNativePreferenceItem(QQuickItem * parent, int id);

private:
    struct PreferenceBinding
    {
        const PreferencesWidget * widget;
        QVariantList choices;
        QPointer<QWidget> native_widget;
    };

    QVariant preferenceValue(const PreferenceBinding & binding) const;
    void destroyNativePreferenceWidgets();

    MobilePluginModel m_model;
    PluginHandle * m_preferences_plugin = nullptr;
    const PluginPreferences * m_plugin_preferences = nullptr;
    std::vector<PreferenceBinding> m_preference_bindings;
};

#endif
