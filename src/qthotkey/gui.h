#ifndef _GUI_H_INCLUDED_
#define _GUI_H_INCLUDED_

#include <QtCore/QList>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

#include "plugin.h"

struct PluginPreferences;

namespace GlobalHotkeys
{

class LineKeyEdit;

extern const PluginPreferences hotkey_prefs;

struct KeyControls
{
    QComboBox * combobox;
    LineKeyEdit * keytext;
    QToolButton * button;

    HotkeyConfiguration hotkey;

    ~KeyControls();
};

class PrefWidget : public QWidget
{
public:
    explicit PrefWidget(QWidget * parent = nullptr);
    ~PrefWidget();

    QList<HotkeyConfiguration> getConfig() const;

    static void * make_config_widget();
    static void ok_callback();
    static void destroy_callback();

private:
    QVBoxLayout * main_widget_layout;
    QLabel * information_pixmap;
    QLabel * information_label;
    QHBoxLayout * information_layout;
    QGroupBox * group_box;
    QGridLayout * group_box_layout;
    QLabel * action_label;
    QLabel * key_binding_label;
    QPushButton * add_button;
    QHBoxLayout * add_button_layout;

    QList<KeyControls *> controls_list;

    void add_event_control(const HotkeyConfiguration * hotkey);
};

} /* namespace GlobalHotkeys */

#endif
