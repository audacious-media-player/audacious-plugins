/*
 *  Audacious Bluetooth headset suport plugin
 *
 *  Copyright (c) 2008 Paula Stanciu <paula.stanciu@gmail.com>
 *  Copyright (C) 2005-2008  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2006-2007  Bastien Nocera <hadess@hadess.net>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <glib/gprintf.h>
#include "bluetooth.h"
#include "gui.h"

#define PASSKEY_AGENT_PATH	"/org/bluez/audacious_passkey"
#define AUTH_AGENT_PATH		"/org/bluez/audacious_auth"

static GtkWidget *agent_window = NULL;
static GtkWidget *window_box = NULL;
static GtkWidget *top_box = NULL;
static GtkWidget *middle_box = NULL;
static GtkWidget *bottom_box = NULL;
static GtkWidget *pair_label = NULL;
static GtkWidget *device_label = NULL;
static GtkWidget *passkey_entry = NULL;
static GtkWidget *ok_button = NULL;
static GtkWidget *cancel_button = NULL;
static char* passkey;
DBusGProxy *pair_obj = NULL;
static DBusGConnection *connection;
void ok_button_call()
{
    passkey = gtk_entry_get_text(GTK_ENTRY(passkey_entry));
    printf("Key entered : %s\n",passkey);
}

void cancel_button_call()
{
    gtk_widget_destroy (agent_window);
    agent_window = NULL;
}

void gui_agent()
{
    if (!agent_window)
    {
        agent_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        g_signal_connect (agent_window, "destroy",G_CALLBACK (gtk_widget_destroyed), &agent_window);
        window_box = gtk_vbox_new(FALSE,2);
        top_box = gtk_vbox_new(FALSE,2);
        middle_box = gtk_hbox_new(FALSE,2);
        bottom_box = gtk_hbox_new(FALSE,2);

        gtk_container_add(GTK_CONTAINER(agent_window),window_box);
        gtk_container_add(GTK_CONTAINER(window_box),top_box);
        gtk_container_set_border_width (GTK_CONTAINER(middle_box), 20);
        gtk_container_add(GTK_CONTAINER(window_box),middle_box);
        gtk_container_add(GTK_CONTAINER(window_box),bottom_box);

        pair_label = gtk_label_new_with_mnemonic("Enter passkey for pairing");
        device_label = gtk_label_new_with_mnemonic(((DeviceData*)(selected_dev->data))->name);

        gtk_container_add(GTK_CONTAINER(top_box),pair_label);
        gtk_container_add(GTK_CONTAINER(top_box),device_label);

        passkey_entry = gtk_entry_new();
        gtk_container_add(GTK_CONTAINER(middle_box),passkey_entry);

        ok_button = gtk_button_new_with_mnemonic("OK");
        cancel_button = gtk_button_new_with_mnemonic("Cancel");
        gtk_container_add(GTK_CONTAINER(bottom_box),ok_button);
        g_signal_connect(ok_button,"clicked",G_CALLBACK(ok_button_call),NULL);

        gtk_container_add(GTK_CONTAINER(bottom_box),cancel_button);
        g_signal_connect(cancel_button,"clicked",G_CALLBACK (cancel_button_call),NULL);


        gtk_window_set_title(GTK_WINDOW(agent_window),"Pairing passkey request");
        if (!GTK_WIDGET_VISIBLE (agent_window))
            gtk_widget_show_all (agent_window);
        else
        {   
            gtk_widget_destroy (agent_window);
            agent_window = NULL;
        }

    }


}
static DBusGConnection *connection;

static int volatile registered_passkey	= 0;
static int volatile registered_auth	= 0;

static gboolean auto_authorize = FALSE;

typedef enum {
    AGENT_ERROR_REJECT
} AgentError;

#define AGENT_ERROR (agent_error_quark())

#define AGENT_ERROR_TYPE (agent_error_get_type()) 

static GQuark agent_error_quark(void)
{
    static GQuark quark = 0;
    if (!quark)
        quark = g_quark_from_static_string("agent");

    return quark;
}

#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }

static GType agent_error_get_type(void)
{
    static GType etype = 0;
    if (etype == 0) {
        static const GEnumValue values[] = {
            ENUM_ENTRY(AGENT_ERROR_REJECT, "Rejected"),
            { 0, 0, 0 }
        };

        etype = g_enum_register_static("agent", values);
    }

    return etype;
}

static GList *input_list = NULL;

struct input_data {
    char *path;
    char *address;
    char *service;
    char *uuid;
    DBusGMethodInvocation *context;
};

static gint input_compare(gconstpointer a, gconstpointer b)
{
    struct input_data *a_data = (struct input_data *) a;
    struct input_data *b_data = (struct input_data *) b;

    return g_ascii_strcasecmp(a_data->address, b_data->address);
}

static void input_free(struct input_data *input)
{

    input_list = g_list_remove(input_list, input);

    g_free(input->uuid);
    g_free(input->service);
    g_free(input->address);
    g_free(input->path);
    g_free(input);

    //	if (g_list_length(input_list) == 0)
    //	disable_blinking();
}

static void passkey_callback(gint response, gpointer user_data)
{
    struct input_data *input = user_data;

    if (response == GTK_RESPONSE_ACCEPT) {
        const char *passkey;
        /*!!!!!!!!! hardcoded passkey !!!!!!!!!!!!! to modify*/
        passkey ="0000";
        dbus_g_method_return(input->context, passkey);
    } else {
        GError *error;
        error = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
                "Pairing request rejected");
        dbus_g_method_return_error(input->context, error);
    }

    input_free(input);
}

static void confirm_callback(gint response, gpointer user_data)
{
    struct input_data *input = user_data;

    if (response != GTK_RESPONSE_YES) {
        GError *error;
        error = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
                "Confirmation request rejected");
        dbus_g_method_return_error(input->context, error);
    } else
        dbus_g_method_return(input->context);

    input_free(input);
}

static void set_trusted(struct input_data *input)
{
    DBusGProxy *object;
    gboolean active;

    //aways set trusted -- we dont have yet an active button -
    //maybe I'll add one	
    object = dbus_g_proxy_new_for_name(connection, "org.bluez",
            input->path, "org.bluez.Adapter");

    dbus_g_proxy_call(object, "SetTrusted", NULL,
            G_TYPE_STRING, input->address, G_TYPE_INVALID,
            G_TYPE_INVALID);
}

static void auth_callback(gint response, gpointer user_data)
{

    /* I must call this function with the alway allow option and 
     * don't boder to aks the user - to many questions 
     */
    struct input_data *input = user_data;

    if (response == GTK_RESPONSE_YES) {
        set_trusted(input);
        dbus_g_method_return(input->context);
    } else {
        GError *error;
        error = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
                "Authorization request rejected");
        dbus_g_method_return_error(input->context, error);
    }

    input_free(input);
}


static void passkey_dialog(const char *path, const char *address,
        const gchar *device, DBusGMethodInvocation *context)
{
    struct input_data *input;

    input = g_try_malloc0(sizeof(*input));
    if (!input)
        return;

    input->path = g_strdup(path);
    input->address = g_strdup(address);

    input->context = context;



    /*
       g_signal_connect(G_OBJECT(entry), "changed",
       G_CALLBACK(changed_callback), input);


       g_signal_connect(G_OBJECT(button), "toggled",
       G_CALLBACK(toggled_callback), input);

       gtk_container_add(GTK_CONTAINER(vbox), button);

       input_list = g_list_append(input_list, input);

       g_signal_connect(G_OBJECT(dialog), "response",
       G_CALLBACK(passkey_callback), input);

       enable_blinking();
       */
    printf("passkey callback\n");
    passkey_callback(GTK_RESPONSE_ACCEPT,input);
}

static void confirm_dialog(const char *path, const char *address,
        const char *value, const gchar *device,
        DBusGMethodInvocation *context)
{
    struct input_data *input;

    input = g_try_malloc0(sizeof(*input));
    if (!input)
        return;

    input->path = g_strdup(path);
    input->address = g_strdup(address);

    input->context = context;


    //	g_signal_connect(G_OBJECT(dialog), "response",
    //				G_CALLBACK(confirm_callback), input);

    //enable_blinking();
}

static void auth_dialog(const char *path, const char *address,
        const char *service, const char *uuid, const gchar *device,
        const gchar *profile, DBusGMethodInvocation *context)
{
    GtkWidget *dialog;
    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *vbox;
    gchar *markup, *text;
    struct input_data *input;

    input = g_try_malloc0(sizeof(*input));
    if (!input)
        return;

    input->path = g_strdup(path);
    input->address = g_strdup(address);
    input->service = g_strdup(service);
    input->uuid = g_strdup(uuid);

    input->context = context;


    /* translators: Whether to grant access to a particular service
     * to the device mentioned */

    //	g_signal_connect(G_OBJECT(dialog), "response",
    //				G_CALLBACK(auth_callback), input);

    //enable_blinking();
}

typedef struct {
    GObject parent;
} PasskeyAgent;

typedef struct {
    GObjectClass parent;
} PasskeyAgentClass;

static GObjectClass *passkey_agent_parent;

G_DEFINE_TYPE(PasskeyAgent, passkey_agent, G_TYPE_OBJECT)

#define PASSKEY_AGENT_OBJECT_TYPE (passkey_agent_get_type())

#define PASSKEY_AGENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
            PASSKEY_AGENT_OBJECT_TYPE, PasskeyAgent))

static void passkey_agent_finalize(GObject *obj)
{
    passkey_agent_parent->finalize(obj);
}

static void passkey_agent_init(PasskeyAgent *obj)
{
    g_printf("passkeyagent init\n");
}

static void passkey_agent_class_init(PasskeyAgentClass *klass)
{
    GObjectClass *gobject_class;

    passkey_agent_parent = g_type_class_peek_parent(klass);

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = passkey_agent_finalize;
}

static PasskeyAgent *passkey_agent_new(const char *path)
{
    PasskeyAgent *agent;

    agent = g_object_new(PASSKEY_AGENT_OBJECT_TYPE, NULL);

    dbus_g_connection_register_g_object(connection, path, G_OBJECT(agent));
    g_printf("new passkey agent \n");
    return agent;
}



static gboolean passkey_agent_request(PasskeyAgent *agent,
        const char *path, const char *address,
        DBusGMethodInvocation *context)
{
    printf("passkey_agent request\n");
    DBusGProxy *object;
    const char *adapter = NULL, *name = NULL;
    gchar *device, *line;

    object = dbus_g_proxy_new_for_name(connection, "org.bluez",
            path, "org.bluez.Adapter");

    dbus_g_proxy_call(object, "GetName", NULL, G_TYPE_INVALID,
            G_TYPE_STRING, &adapter, G_TYPE_INVALID);

    dbus_g_proxy_call(object, "GetRemoteName", NULL,
            G_TYPE_STRING, address, G_TYPE_INVALID,
            G_TYPE_STRING, &name, G_TYPE_INVALID);

    if (name) {
        if (g_strrstr(name, address))
            device = g_strdup(name);
        else
            device = g_strdup_printf("%s (%s)", name, address);
    } else
        device = g_strdup(address);

    passkey_dialog(path, address, device, context);
    printf ("pairing request for device :%s",address);
    /* translators: this is a popup telling you a particular device
     * has asked for pairing */
    line = g_strdup_printf(_("Pairing request for '%s'"), device);
    g_free(device);

    /*show_notification(adapter ? adapter : _("Bluetooth device"),
      line, _("Enter passkey"), 0,
      G_CALLBACK(notification_closed));
      */
    g_free(line);

    return TRUE;
}

static gboolean passkey_agent_confirm(PasskeyAgent *agent,
        const char *path, const char *address,
        const char *value, DBusGMethodInvocation *context)
{
    DBusGProxy *object;
    const char *adapter = NULL, *name = NULL;
    gchar *device, *line;

    object = dbus_g_proxy_new_for_name(connection, "org.bluez",
            path, "org.bluez.Adapter");

    dbus_g_proxy_call(object, "GetName", NULL, G_TYPE_INVALID,
            G_TYPE_STRING, &adapter, G_TYPE_INVALID);

    dbus_g_proxy_call(object, "GetRemoteName", NULL,
            G_TYPE_STRING, address, G_TYPE_INVALID,
            G_TYPE_STRING, &name, G_TYPE_INVALID);

    if (name) {
        if (g_strrstr(name, address))
            device = g_strdup(name);
        else
            device = g_strdup_printf("%s (%s)", name, address);
    } else
        device = g_strdup(address);

    confirm_dialog(path, address, value, device, context);

    line = g_strdup_printf(_("Pairing request for '%s'"), device);
    g_free(device);

    /*show_notification(adapter ? adapter : _("Bluetooth device"),
      line, _("Confirm pairing"), 0,
      G_CALLBACK(notification_closed));
      */
    g_free (line);

    return TRUE;
}

static gboolean passkey_agent_cancel(PasskeyAgent *agent,
        const char *path, const char *address, GError **error)
{
    GList *list;
    GError *result;
    struct input_data *input;

    input = g_try_malloc0(sizeof(*input));
    if (!input)
        return FALSE;

    input->path = g_strdup(path);
    input->address = g_strdup(address);

    list = g_list_find_custom(input_list, input, input_compare);

    g_free(input->address);
    g_free(input->path);
    g_free(input);

    if (!list || !list->data)
        return FALSE;

    input = list->data;

    //close_notification();

    result = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
            "Agent callback canceled");

    dbus_g_method_return_error(input->context, result);

    input_free(input);

    return TRUE;
}

static gboolean passkey_agent_release(PasskeyAgent *agent, GError **error)
{
    registered_passkey = 0;

    return TRUE;
}

#include "passkey-agent-glue.h"

typedef struct {
    GObject parent;
} AuthAgent;

typedef struct {
    GObjectClass parent;
} AuthAgentClass;

static GObjectClass *auth_agent_parent;

G_DEFINE_TYPE(AuthAgent, auth_agent, G_TYPE_OBJECT)

#define AUTH_AGENT_OBJECT_TYPE (auth_agent_get_type())

#define AUTH_AGENT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), \
            AUTH_AGENT_OBJECT_TYPE, AuthAgent))

static void auth_agent_finalize(GObject *obj)
{
    auth_agent_parent->finalize(obj);
}

static void auth_agent_init(AuthAgent *obj)
{
}

static void auth_agent_class_init(AuthAgentClass *klass)
{
    GObjectClass *gobject_class;

    auth_agent_parent = g_type_class_peek_parent(klass);

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = auth_agent_finalize;
}

static AuthAgent *auth_agent_new(const char *path)
{
    AuthAgent *agent;

    agent = g_object_new(AUTH_AGENT_OBJECT_TYPE, NULL);

    dbus_g_connection_register_g_object(connection, path, G_OBJECT(agent));

    return agent;
}

static gboolean auth_agent_authorize(PasskeyAgent *agent,
        const char *path, const char *address, const char *service,
        const char *uuid, DBusGMethodInvocation *context)
{
    DBusGProxy *object;
    const char *adapter = NULL, *name = NULL;
    gchar *device, *profile, *line;

    if (auto_authorize == TRUE) {
        dbus_g_method_return(context);
        return TRUE;
    }

    object = dbus_g_proxy_new_for_name(connection, "org.bluez",
            path, "org.bluez.Adapter");

    dbus_g_proxy_call(object, "GetName", NULL, G_TYPE_INVALID,
            G_TYPE_STRING, &adapter, G_TYPE_INVALID);

    dbus_g_proxy_call(object, "GetRemoteName", NULL,
            G_TYPE_STRING, address, G_TYPE_INVALID,
            G_TYPE_STRING, &name, G_TYPE_INVALID);

    object = dbus_g_proxy_new_for_name(connection, "org.bluez",
            service, "org.bluez.Service");

    dbus_g_proxy_call(object, "GetName", NULL, G_TYPE_INVALID,
            G_TYPE_STRING, &profile, G_TYPE_INVALID);

    if (name) {
        if (g_strrstr(name, address))
            device = g_strdup(name);
        else
            device = g_strdup_printf("%s (%s)", name, address);
    } else
        device = g_strdup(address);

    auth_dialog(path, address, service, uuid, device, profile, context);

    line = g_strdup_printf(_("Authorization request for %s"), device);
    g_free(device);

    /*show_notification(adapter ? adapter : _("Bluetooth device"),
      line, _("Check authorization"), 0,
      G_CALLBACK(notification_closed));
      */
    g_free(line);

    return TRUE;
}

static gboolean auth_agent_cancel(PasskeyAgent *agent,
        const char *path, const char *address, const char *service,
        const char *uuid, DBusGMethodInvocation *context)
{
    GList *list;
    GError *result;
    struct input_data *input;

    input = g_try_malloc0(sizeof(*input));
    if (!input)
        return FALSE;

    input->path = g_strdup(path);
    input->address = g_strdup(address);
    input->service = g_strdup(service);
    input->uuid = g_strdup(uuid);

    list = g_list_find_custom(input_list, input, input_compare);

    g_free(input->uuid);
    g_free(input->service);
    g_free(input->address);
    g_free(input->path);
    g_free(input);

    if (!list || !list->data)
        return FALSE;

    input = list->data;

    //close_notification();

    result = g_error_new(AGENT_ERROR, AGENT_ERROR_REJECT,
            "Agent callback canceled");

    dbus_g_method_return_error(input->context, result);

    input_free(input);

    return TRUE;
}

static gboolean auth_agent_release(PasskeyAgent *agent, GError **error)
{
    registered_auth = 0;

    return TRUE;
}

#include "auth-agent-glue.h"

int register_agents(void)
{
    DBusGProxy *object;
    GError *error = NULL;

    if (registered_passkey && registered_auth)
        return 0;

    object = dbus_g_proxy_new_for_name(connection, "org.bluez",
            "/org/bluez", "org.bluez.Security");

    if (!registered_passkey) {

        dbus_g_proxy_call(object, "RegisterDefaultPasskeyAgent",
                &error, G_TYPE_STRING, PASSKEY_AGENT_PATH,
                G_TYPE_INVALID, G_TYPE_INVALID);

        if (error != NULL) {
            g_error_free(error);
            return -1;
        }

        registered_passkey = 1;
    }

    if (!registered_auth) {
        dbus_g_proxy_call(object, "RegisterDefaultAuthorizationAgent",
                &error,	G_TYPE_STRING, AUTH_AGENT_PATH,
                G_TYPE_INVALID, G_TYPE_INVALID);

        if (error != NULL) {
            g_error_free(error);
            return -1;
        }

        registered_auth = 1;
    }

    return 0;
}

void unregister_agents(void)
{
    registered_passkey = 0;
    registered_auth = 0;
}

int setup_agents(DBusGConnection *conn)
{
    void *agent;

    connection = dbus_g_connection_ref(conn);

    dbus_g_object_type_install_info(PASSKEY_AGENT_OBJECT_TYPE,
            &dbus_glib_passkey_agent_object_info);

    dbus_g_object_type_install_info(AUTH_AGENT_OBJECT_TYPE,
            &dbus_glib_auth_agent_object_info);

    dbus_g_error_domain_register(AGENT_ERROR, "org.bluez.Error",
            AGENT_ERROR_TYPE);

    agent = passkey_agent_new(PASSKEY_AGENT_PATH);

    agent = auth_agent_new(AUTH_AGENT_PATH);

    return 0;
}

void cleanup_agents(void)
{
    unregister_agents();

    dbus_g_connection_unref(connection);
}

void show_agents(void)
{
    printf("show_agents\n");
    //close_notification();

    //	g_list_foreach(input_list, show_dialog, NULL);

    //	disable_blinking();
}

void set_auto_authorize(gboolean value)
{
    auto_authorize = value;
}
void run_agents()
{
    setup_agents(bus);

    register_agents();

}

