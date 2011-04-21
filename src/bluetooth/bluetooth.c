/*
 * Audacious Bluetooth headset suport plugin
 *
 * Copyright (c) 2008 Paula Stanciu paula.stanciu@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */

#include <glib/gstdio.h>
#include <errno.h>
#include <string.h>
#include "bluetooth.h"
#include "marshal.h"
#include "gui.h"
#include "scan_gui.h"
#include "agent.h"
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>
#define DEBUG 1
GList * current_device = NULL;
gint config = 0;
gint devices_no = 0;
gboolean pcmdev_modified = FALSE;
GStaticMutex mutex = G_STATIC_MUTEX_INIT;
static gchar *current_address=NULL;
static GThread *connect_th;
void bluetooth_init ( void );
void bluetooth_cleanup ( void );
void bt_cfg(void);
void bt_about(void);
static void remote_device_found(DBusGProxy *object, char *address, const unsigned int class, const int rssi, gpointer user_data);
static void discovery_started(DBusGProxy *object, gpointer user_data);
static void remote_name_updated(DBusGProxy *object, const char *address,  char *name, gpointer user_data);
static void print_results(void);
static void discovery_completed(DBusGProxy *object, gpointer user_data);
void discover_devices(void);
void disconnect_dbus_signals(void);
/*static void show_restart_dialog(void); */
static void remove_bonding(gchar* device);

AUD_GENERAL_PLUGIN
(
    .name = "Bluetooth audio support",
    .init = bluetooth_init,
    .about = bt_about,
    .configure = bt_cfg,
    .cleanup = bluetooth_cleanup
)

void bluetooth_init ( void )
{
    bonded_dev = NULL;
    discover_finish = 0;
    pcmdev_modified = FALSE;
    audio_devices = NULL;
    bus = NULL;
    obj = NULL;
    gchar* bonded_addr = NULL;
    discover_devices();
    mcs_handle_t *cfgfile ;
    cfgfile = aud_cfg_db_open();
    if(!aud_cfg_db_get_string(cfgfile, "BLUETOOTH_PLUGIN", "bonded",
                              &bonded_addr)) {
        aud_cfg_db_close(cfgfile);
        return;
    }
    if(bonded_addr!=NULL && g_strcmp0(bonded_addr,"no")!=0)
        {
             remove_bonding(bonded_addr);
        }
    free(bonded_addr);
    aud_cfg_db_close(cfgfile);

}

void bluetooth_cleanup ( void )
{
    if (config ==1 )
    {
        close_window();
        config =0;
    }
    if (bonded_dev) {
        printf("bluetooth: exit\n");
        remove_bonding(bonded_dev);
    }
    if(discover_finish == 2) {
        dbus_g_connection_flush (bus);
        dbus_g_connection_unref(bus);
        disconnect_dbus_signals();

    }
    if (pcmdev_modified) {
    /* switching back to default pcm device at cleanup */
    mcs_handle_t *cfgfile = aud_cfg_db_open();
    aud_cfg_db_set_string(cfgfile,"alsa","pcm", "default");
    aud_cfg_db_close(cfgfile);
    }
}

void bt_about( void )
{
  	static GtkWidget *dialog = NULL;

	audgui_simple_message(&dialog, GTK_MESSAGE_INFO, _("Bluetooth headset support plugin"),
				_("Bluetooth headset support\n"
				"Copyright (c) 2008 Paula Stanciu paula.stanciu@gmail.com\n"
				"This was a GSoC 2008 Project - Many thanks to my mentor Tony Vroon and the Audacious team\n \n"
                "In order to use the AVRCP you need the uinput module loaded into the kernel\n"
                "The headset keys will be recognized as normal mutimedia keys and \n"
                "can be configured using the Audacious Global Hotkey plugin or ohter tools \n"
                "provided by your window manager\n"
                ));
}

void bt_cfg(void)
{
    printf("bt_cfg\n");
    config =1;
    if(discover_finish == 2){
        if (devices_no == 0){
            printf("no devs!\n");
            show_scan(0);
            show_no_devices();
        }else
            results_ui();
    }
    else show_scan(0);
    printf("end of bt_cfg\n");
}

void disconnect_dbus_signals()
{
    dbus_g_proxy_disconnect_signal(obj, "RemoteDeviceFound", G_CALLBACK(remote_device_found), bus);
    dbus_g_proxy_disconnect_signal(obj, "DiscoveryStarted", G_CALLBACK(discovery_started), bus);
    dbus_g_proxy_disconnect_signal(obj, "DiscoveryCompleted", G_CALLBACK(discovery_completed), bus);
    dbus_g_proxy_disconnect_signal(obj, "RemoteNameUpdated", G_CALLBACK(remote_name_updated), NULL);

}

void clean_devices_list()
{
    g_list_free(audio_devices);
    dbus_g_connection_flush (bus);
    dbus_g_connection_unref(bus);
    audio_devices = NULL;
    //g_list_free(current_device);
}
static void remove_bonding(gchar *device)
{
    printf("remove_bonding call\n");
    dbus_g_object_register_marshaller(marshal_VOID__STRING_UINT_INT, G_TYPE_NONE, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_call(obj,"RemoveBonding",NULL,G_TYPE_STRING,device,G_TYPE_INVALID,G_TYPE_INVALID);

}
void refresh_call(void)
{
    printf("refresh function called\n");
    disconnect_dbus_signals();
    clean_devices_list();
    if(discover_finish == 0 ||discover_finish== 2){
        discover_finish = 0;

        discover_devices();
        close_window();
        show_scan(0);
    }
    else
        printf("Scanning please wait!\n");
}

gpointer connect_call_th(void)
{
    //I will have to enable the audio service if necessary
    run_agents();
    dbus_g_object_register_marshaller(marshal_VOID__STRING_UINT_INT, G_TYPE_NONE, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_call(obj,"CreateBonding",NULL,G_TYPE_STRING,current_address,G_TYPE_INVALID,G_TYPE_INVALID);

return NULL;
}

void connect_call(void)
{
    close_call();
    close_window();
    show_scan(1);
    current_address = g_strdup(((DeviceData*)(selected_dev->data))->address);
    connect_th = g_thread_create((GThreadFunc)connect_call_th,NULL,TRUE,NULL) ;
}


void play_call()
{

    FILE *file;
    FILE *temp_file;
    gint prev=0;
    char line[128];
    const gchar *home;
    gchar *device_line;
    gchar *file_name="";
    gchar *temp_file_name;
    home = g_get_home_dir();
    file_name = g_strconcat(home,"/.asoundrc",NULL);
    temp_file_name = g_strconcat(home,"/temp_bt",NULL);
    file = fopen(file_name,"r");
    temp_file = fopen(temp_file_name,"w");
    device_line = g_strdup_printf("device %s\n",current_address);
    if ( file != NULL )
    {
        while ( fgets ( line, sizeof line, file ) != NULL )
        {
            if(!(strcmp(line,"pcm.audacious_bt{\n"))){
                fputs(line,temp_file);
                fgets ( line, sizeof line, file ); /* type bluetooth */
                fputs(line,temp_file);
                fgets ( line, sizeof line, file ); /* device MAC */
                fputs(device_line,temp_file);
                prev = 1;
            } else
                fputs(line,temp_file);
        }

        fclose (file);
    }
    if(!prev){
        fputs("pcm.audacious_bt{\n",temp_file);
        fputs("type bluetooth\n",temp_file);
        fputs(device_line,temp_file);
        fputs("}\n",temp_file);
        prev = 0;
    }

    fclose(temp_file);
    int err = rename(temp_file_name,file_name);
    printf("rename error : %d",err);
    perror("zz");
    g_free(device_line);
    g_free(file_name);
    g_free(temp_file_name);
    pcmdev_modified = TRUE;
    mcs_handle_t *cfgfile = aud_cfg_db_open();
    aud_cfg_db_set_string(cfgfile,"alsa","pcm", "audacious_bt");
    aud_cfg_db_close(cfgfile);

    printf("play callback\n");
    close_window();
    aud_output_plugin_cleanup();
//    aud_drct_stop();
    aud_drct_play();


}
/*static void show_restart_dialog()
{
    static GtkWidget *window = NULL;
    GtkWidget *dialog;
    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Please restart the player to apply the bluetooth audio settings!");
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
}
*/
static void remote_device_found(DBusGProxy *object, char *address, const unsigned int class, const int rssi, gpointer user_data)
{
    int found_in_list=FALSE;
    g_static_mutex_lock(&mutex);
    current_device = audio_devices;
    if((class & 0x200404)==0x200404)
    {
        while(current_device != NULL)
        {
            if(g_str_equal(address,((DeviceData*)(current_device->data))->address))
            {
                found_in_list = TRUE;
                break;
            }
            current_device=g_list_next(current_device);
        }
        if(!found_in_list)
        {
            DeviceData *dev= g_new0(DeviceData, 1);
            dev->class = class;
            dev->address = g_strdup(address);
            dev->name = NULL;
            audio_devices=g_list_prepend(audio_devices, dev);
        }
    }
    g_static_mutex_unlock(&mutex);
}

static void discovery_started(DBusGProxy *object, gpointer user_data)
{
    g_print("Signal: DiscoveryStarted()\n");
    discover_finish = 1;
}

static void remote_name_updated(DBusGProxy *object, const char *address,  char *name, gpointer user_data)
{
    g_static_mutex_lock(&mutex);
    current_device=audio_devices;
    while(current_device != NULL)
    {
        if(g_str_equal(address,((DeviceData*)(current_device->data))->address))
        {
            ((DeviceData*)(current_device->data))->name=g_strdup(name);
            break;
        }
        current_device=g_list_next(current_device);
    }
    g_static_mutex_unlock(&mutex);
}

static void print_results()
{
    int i=0;
    g_print("Final Scan results:\n");
    devices_no = g_list_length(audio_devices);
    g_print("Number of audio devices: %d \n",devices_no);
    if(devices_no==0 ) {
        if(config ==1) show_no_devices();
    } else {
        current_device=audio_devices;
        while(current_device != NULL)
        {
            g_print("Device %d: Name: %s, Class: 0x%x, Address: %s\n",++i,
                    ((DeviceData*)(current_device->data))-> name,
                    ((DeviceData*)(current_device->data))-> class,
                    ((DeviceData*)(current_device->data))-> address);
            current_device=g_list_next(current_device);
        }
        destroy_scan_window();
        if(config==1) {
            destroy_scan_window();
            results_ui();
        }
        //   refresh_tree();
    }
}



static void discovery_completed(DBusGProxy *object, gpointer user_data)
{
    g_print("Signal: DiscoveryCompleted()\n");
    discover_finish =2;
    print_results();
}



void discover_devices(void)
{
    GError *error = NULL;
    //  g_type_init();
    /*    g_log_set_always_fatal (G_LOG_LEVEL_WARNING); */ /* too invasive */
    bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    if (error != NULL)
    {
        g_printerr("Connecting to system bus failed: %s\n", error->message);
        g_error_free(error);
        return;
    }
    obj = dbus_g_proxy_new_for_name(bus, "org.bluez", "/org/bluez/hci0", "org.bluez.Adapter");
    printf("bluetooth plugin - start discovery \n");
    dbus_g_object_register_marshaller(marshal_VOID__STRING_UINT_INT, G_TYPE_NONE, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INT, G_TYPE_INVALID);

    dbus_g_proxy_add_signal(obj, "RemoteDeviceFound", G_TYPE_STRING, G_TYPE_UINT, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(obj, "RemoteDeviceFound", G_CALLBACK(remote_device_found), bus, NULL);

    dbus_g_proxy_add_signal(obj, "DiscoveryStarted", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(obj, "DiscoveryStarted", G_CALLBACK(discovery_started), bus, NULL);

    dbus_g_proxy_add_signal(obj, "DiscoveryCompleted", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(obj, "DiscoveryCompleted", G_CALLBACK(discovery_completed), bus, NULL);

    dbus_g_object_register_marshaller(marshal_VOID__STRING_STRING, G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

    dbus_g_proxy_add_signal(obj, "RemoteNameUpdated", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(obj, "RemoteNameUpdated", G_CALLBACK(remote_name_updated), NULL, NULL);

    dbus_g_proxy_call(obj, "DiscoverDevices", &error, G_TYPE_INVALID, G_TYPE_INVALID);
    if (error != NULL)
    {
        g_printerr("Failed to discover devices: %s\n", error->message);
        g_error_free(error);
    }

}
