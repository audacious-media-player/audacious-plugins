#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib-object.h>
#include <stdio.h>
#include "bluetooth.h"
#include "marshal.h"
#include "gui.h"

#define DEBUG 1
static gboolean plugin_active = FALSE,exiting=FALSE;
GList * current_device = NULL;
DBusGConnection * bus = NULL;
gint discover_finish =0;
GStaticMutex mutex = G_STATIC_MUTEX_INIT;

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



GeneralPlugin bluetooth_gp =
{
    .description = "Bluetooth audio suport",
    .init = bluetooth_init,
    .about = bt_about,
    .configure = bt_cfg,
    .cleanup = bluetooth_cleanup
};
GeneralPlugin *bluetooth_gplist[] = { &bluetooth_gp, NULL };
DECLARE_PLUGIN(bluetooth_gp, NULL, NULL, NULL, NULL, NULL, bluetooth_gplist, NULL, NULL)
void bluetooth_init ( void )
{
    audio_devices = NULL;
    discover_devices();
}

void bluetooth_cleanup ( void )
{
    printf("bluetooth: exit\n");
    dbus_g_connection_flush (bus);
    dbus_g_connection_unref(bus);

}
/*void bt_cfg( void )
  {

  }
  */
void bt_about( void )
{

}


void refresh_call(void){
    discover_devices()
    printf("refresh function called\n");
}

void connect_call(void){
  printf("connect function \n");
}


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
    g_print("Number of audio devices: %d \n",g_list_length(audio_devices));
    current_device=audio_devices;
    while(current_device != NULL)
    {
        g_print("Device %d: Name: %s, Class: 0x%x, Address: %s\n",++i,
                ((DeviceData*)(current_device->data))-> name,
                ((DeviceData*)(current_device->data))-> class,
                ((DeviceData*)(current_device->data))-> address);

        current_device=g_list_next(current_device);
    }

    refresh_tree();
}

static void discovery_completed(DBusGProxy *object, gpointer user_data)
{
    g_print("Signal: DiscoveryCompleted()\n");
    print_results();


}


void discover_devices(void){
    GError *error = NULL;
    DBusGProxy * obj = NULL;
    g_type_init();
    g_log_set_always_fatal (G_LOG_LEVEL_WARNING);

    bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    if (error != NULL)
    {
        g_printerr("Connecting to system bus failed: %s\n", error->message);
        g_error_free(error);
        exit(EXIT_FAILURE);
    }
    obj = dbus_g_proxy_new_for_name(bus, "org.bluez", "/org/bluez/hci0", "org.bluez.Adapter");

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
        exit(EXIT_FAILURE);
    }
    // /  dbus_g_connection_flush (bus);
    //    dbus_g_connection_unref(bus);
}    
