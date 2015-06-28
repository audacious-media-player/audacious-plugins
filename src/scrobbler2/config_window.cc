
//external includes
#include <gdk/gdk.h>
//plugin includes
#include "scrobbler.h"


//shared variables
gboolean          permission_check_requested   = false;
gboolean          invalidate_session_requested = false;
enum permission perm_result                  = PERMISSION_UNKNOWN;
String          username;


//static (private) variables
static GtkWidget *button;
static GtkWidget *revoke_button;
static GtkWidget *permission_status_icon;
static GtkWidget *permission_status_label;

static GtkWidget *details_label_first;
static GtkWidget *url_button;
static GtkWidget *details_label_second;

static GtkWidget *additional_details_icon;
static GtkWidget *additional_details_label;


static gboolean permission_checker_thread (void * data) {
    if (permission_check_requested == true) {
        //the answer hasn't arrived yet
        return true;

    } else {
        //the answer has arrived
        g_assert(perm_result != PERMISSION_UNKNOWN);

        if (perm_result == PERMISSION_ALLOWED) {
            gtk_image_set_from_icon_name(GTK_IMAGE(permission_status_icon), "face-smile", GTK_ICON_SIZE_SMALL_TOOLBAR);

            char *markup = g_markup_printf_escaped(_("OK. Scrobbling for user: %s"),
             (const char *)username);

            gtk_label_set_markup(GTK_LABEL(permission_status_label), markup);
            gtk_widget_set_sensitive(revoke_button, true);
            g_free(markup);

        } else if (perm_result == PERMISSION_DENIED) {

            gtk_image_set_from_icon_name(GTK_IMAGE(permission_status_icon), "dialog-error", GTK_ICON_SIZE_SMALL_TOOLBAR);
            gtk_image_set_from_icon_name(GTK_IMAGE(additional_details_icon), "dialog-information", GTK_ICON_SIZE_SMALL_TOOLBAR);


            gtk_label_set_label(GTK_LABEL(permission_status_label), _("Permission Denied"));

            gtk_label_set_markup(GTK_LABEL(details_label_first), _("Access the following link to allow Audacious to scrobble your plays:"));

            char *url = g_markup_printf_escaped("http://www.last.fm/api/auth/?api_key=%s&token=%s",
             SCROBBLER_API_KEY, (const char *)request_token);

            gtk_link_button_set_uri(GTK_LINK_BUTTON(url_button), url);
            gtk_button_set_label(GTK_BUTTON(url_button), url);
            gtk_widget_show(url_button);
            g_free(url);

            gtk_label_set_markup(GTK_LABEL(details_label_second), _("Keep this window open and click 'Check Permission' again.\n"));

            gtk_label_set_label(GTK_LABEL(additional_details_label),
                                _("Don't worry. Your scrobbles are saved on your computer.\n"
                                  "They will be submitted as soon as Audacious is allowed to do so."));

        } else if (perm_result == PERMISSION_NONET) {
            gtk_image_set_from_icon_name(GTK_IMAGE(permission_status_icon), "dialog-warning", GTK_ICON_SIZE_SMALL_TOOLBAR);
            gtk_image_set_from_icon_name(GTK_IMAGE(additional_details_icon), "dialog-information", GTK_ICON_SIZE_SMALL_TOOLBAR);


            gtk_label_set_label(GTK_LABEL(permission_status_label), _("Network Problem."));
            gtk_label_set_label(GTK_LABEL(details_label_first),     _("There was a problem contacting Last.fm. Please try again later."));
            gtk_label_set_label(GTK_LABEL(additional_details_label),
                    _("Don't worry. Your scrobbles are saved on your computer.\n"
                      "They will be submitted as soon as Audacious is allowed to do so."));
        }

        perm_result = PERMISSION_UNKNOWN;
        gtk_widget_set_sensitive(button, true);

        return false;
    }
}


static void cleanup_window() {
    gtk_widget_set_sensitive(button, false);
    gtk_widget_set_sensitive(revoke_button, false);

    gtk_image_clear(GTK_IMAGE(permission_status_icon));
    gtk_image_clear(GTK_IMAGE(additional_details_icon));

    gtk_label_set_label(GTK_LABEL(permission_status_label), (""));
    gtk_label_set_label(GTK_LABEL(details_label_first), "");
    gtk_widget_hide(url_button);
    gtk_label_set_label(GTK_LABEL(details_label_second), "");
    gtk_label_set_label(GTK_LABEL(additional_details_label), "");
}

static void permission_checker (GtkButton *button12, void * data) {
    cleanup_window();

    gtk_image_set_from_icon_name(GTK_IMAGE(permission_status_icon), "system-run", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_label_set_label(GTK_LABEL(permission_status_label), _("Checking..."));

    //This will make the communication thread check the permission
    //and set the current status on the perm_result enum
    permission_check_requested = true;

    //This is only to accelerate the check.
    //If scrobbles are being made, they are stopped for the request to be done sooner.
    scrobbling_enabled = false;

    //Wake up the communication thread in case it's waiting for track plays
    pthread_mutex_lock(&communication_mutex);
    pthread_cond_signal(&communication_signal);
    pthread_mutex_unlock(&communication_mutex);

    //The button is clicked. Wait for the permission check to be done.
    gdk_threads_add_timeout_seconds(1, permission_checker_thread, data);
}

static void revoke_permissions (GtkButton *revoke_button2, void * data) {
    cleanup_window();

    pthread_mutex_lock(&communication_mutex);
    invalidate_session_requested = true;

    scrobbling_enabled = false;
    pthread_cond_signal(&communication_signal);
    pthread_mutex_unlock(&communication_mutex);

    gtk_widget_set_sensitive(button, true);
}

/*
  ,---config_box---------------------.
  |                                  |
  |,-permission_box----------------. |
  || ,-buttons_box--.              | |
  || |button        | perm_status  | |
  || |revoke_button |              | |
  || `--------------'              | |
  |`-------------------------------' |
  |                                  |
  |,-details_box------------------.  |
  ||details_label_first           |  |
  ||url_button                    |  |
  ||details_label_second          |  |
  |`------------------------------'  |
  |                                  |
  |,-additional_details_box--------. |
  ||_______________________________| |
  |__________________________________|

 */
static void *config_status_checker () {
    GtkWidget *config_box;
    GtkWidget *permission_box;
    GtkWidget *buttons_box;
    GtkWidget *details_box;
    GtkWidget *additional_details_box;

    config_box              = gtk_vbox_new (false, 15);
    permission_box          = gtk_hbox_new (false, 0);
    buttons_box             = gtk_vbutton_box_new ();
    details_box             = gtk_vbox_new (false, 0);
    additional_details_box  = gtk_hbox_new (false, 7);

    button                  = gtk_button_new_with_mnemonic(_("C_heck Permission"));
    revoke_button           = gtk_button_new_with_mnemonic(_("_Revoke Permission"));
    gtk_widget_set_sensitive(revoke_button, false);

    permission_status_icon  = gtk_image_new();
    permission_status_label = gtk_label_new("");

    details_label_first  = gtk_label_new ("");
    url_button           = gtk_link_button_new("");
    details_label_second = gtk_label_new("");

    gtk_widget_hide(url_button);
    gtk_widget_set_no_show_all(url_button, true);

    additional_details_icon  = gtk_image_new();
    additional_details_label = gtk_label_new("");


    g_signal_connect (button,        "clicked", G_CALLBACK (permission_checker), nullptr);
    g_signal_connect (revoke_button, "clicked", G_CALLBACK (revoke_permissions), nullptr);

    gtk_box_pack_start(GTK_BOX(permission_box), buttons_box,             false, false, 20);
    gtk_box_pack_start(GTK_BOX(permission_box), permission_status_icon,  false, false, 0);
    gtk_box_pack_start(GTK_BOX(permission_box), permission_status_label, false, false, 5);

    gtk_box_pack_start(GTK_BOX(buttons_box), button,        false, false, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), revoke_button, false, false, 0);

    gtk_box_pack_start(GTK_BOX(details_box), details_label_first,  false, false, 0);
    gtk_box_pack_start(GTK_BOX(details_box), url_button,           false, false, 0);
    gtk_box_pack_start(GTK_BOX(details_box), details_label_second, false, false, 0);

    gtk_box_pack_start(GTK_BOX(additional_details_box), additional_details_icon,  false, false, 0);
    gtk_box_pack_start(GTK_BOX(additional_details_box), additional_details_label, false, false, 0);

    gtk_box_pack_start(GTK_BOX(config_box), permission_box,         false, false, 0);
    gtk_box_pack_start(GTK_BOX(config_box), details_box,            false, false, 0);
    gtk_box_pack_start(GTK_BOX(config_box), additional_details_box, false, false, 0);

    return config_box;
}


static const PreferencesWidget config_contents[] = {
    WidgetLabel (N_("You need to allow Audacious to scrobble tracks to your Last.fm account.\n")),
    WidgetCustomGTK (config_status_checker)
};

const PluginPreferences configuration = {{config_contents}};
