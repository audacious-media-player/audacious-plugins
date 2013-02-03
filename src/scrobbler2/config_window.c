
//external includes
#include <gdk/gdk.h>

//plugin includes
#include "scrobbler.h"


//shared variables
bool_t          permission_check_requested   = FALSE;
bool_t          invalidate_session_requested = FALSE;
enum permission perm_result                  = PERMISSION_UNKNOWN;
gchar          *username                     = "";


//static (private) variables
static GtkWidget *button;
static GtkWidget *revoke_button;
static GtkWidget *permission_status_icon;
static GtkWidget *permission_status_label;

static GtkWidget *details_label;

static GtkWidget *additional_details_icon;
static GtkWidget *additional_details_label;


static gboolean permission_checker_thread (gpointer data) {
    if (permission_check_requested == TRUE) {
        //the answer hasn't arrived yet
        return TRUE;

    } else {
        //the answer has arrived
        g_assert(perm_result != PERMISSION_UNKNOWN);

        if (perm_result == PERMISSION_ALLOWED) {
            gtk_image_set_from_stock(GTK_IMAGE(permission_status_icon), GTK_STOCK_YES, GTK_ICON_SIZE_SMALL_TOOLBAR);

            gchar *markup = g_markup_printf_escaped(N_("OK. Scrobbling for user: %s"), username);

            gtk_label_set_markup(GTK_LABEL(permission_status_label), markup);
            gtk_widget_set_sensitive(revoke_button, TRUE);

        } else if (perm_result == PERMISSION_DENIED) {

            gtk_image_set_from_stock(GTK_IMAGE(permission_status_icon),  GTK_STOCK_NO,   GTK_ICON_SIZE_SMALL_TOOLBAR);
            gtk_image_set_from_stock(GTK_IMAGE(additional_details_icon), GTK_STOCK_INFO, GTK_ICON_SIZE_SMALL_TOOLBAR);


            gtk_label_set_label(GTK_LABEL(permission_status_label), "Permission Denied");

            gchar *markup = g_markup_printf_escaped(
                 N_("Access the following link to allow Audacious to scrobble your plays:\n"
                    "<a href=\"http://www.last.fm/api/auth/?api_key=%s&amp;token=%s\">http://www.last.fm/api/auth/?api_key=%s&amp;token=%s</a>\n\n"
                    "Keep this window open and click 'Check Permissions' again.\n"), SCROBBLER_API_KEY, request_token, SCROBBLER_API_KEY, request_token);
            gtk_label_set_markup(GTK_LABEL(details_label), markup);
            g_free(markup);

            gtk_label_set_label(GTK_LABEL(additional_details_label),
                                N_("Don't worry. Your scrobbles are saved on your computer.\n"
                                   "They will be submitted as soon as Audacious is allowed to do so."));

        } else if (perm_result == PERMISSION_NONET) {
            gtk_image_set_from_stock(GTK_IMAGE(permission_status_icon),  GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_SMALL_TOOLBAR);
            gtk_image_set_from_stock(GTK_IMAGE(additional_details_icon), GTK_STOCK_INFO, GTK_ICON_SIZE_SMALL_TOOLBAR);


            gtk_label_set_label(GTK_LABEL(permission_status_label), N_("Network Problem."));
            gtk_label_set_label(GTK_LABEL(details_label),           N_("There was a problem contacting Last.fm. Please try again later."));
            gtk_label_set_label(GTK_LABEL(additional_details_label),
                  N_("Don't worry. Your scrobbles are saved on your computer.\n"
                    "They will be submitted as soon as Audacious is allowed to do so."));
        }

        perm_result = PERMISSION_UNKNOWN;
        gtk_widget_set_sensitive(button, TRUE);

        return FALSE;
    }
}


static void cleanup_window() {
    gtk_widget_set_sensitive(button, FALSE);
    gtk_widget_set_sensitive(revoke_button, FALSE);

    gtk_image_clear(GTK_IMAGE(permission_status_icon));
    gtk_image_clear(GTK_IMAGE(additional_details_icon));

    gtk_label_set_label(GTK_LABEL(permission_status_label), N_(""));
    gtk_label_set_label(GTK_LABEL(details_label), "");
    gtk_label_set_label(GTK_LABEL(additional_details_label), "");
}

static void permission_checker (GtkButton *button12, gpointer data) {
    cleanup_window();

    gtk_image_set_from_stock(GTK_IMAGE(permission_status_icon), GTK_STOCK_EXECUTE, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_label_set_label(GTK_LABEL(permission_status_label), N_("Checking..."));

    //This will make the communication thread check the permission
    //and set the current status on the perm_result enum
    g_mutex_lock(&communication_mutex);
    permission_check_requested = TRUE;

    //This is only to accelerate the check.
    //If scrobbles are being made, they are stopped for the request to be done sooner.
    scrobbling_enabled = FALSE;


    //Wake the communication thread up in case it's waiting for track plays
    g_cond_signal(&communication_signal);
    g_mutex_unlock(&communication_mutex);

    //The button is clicked. Wait for the permission check to be done.
    gdk_threads_add_timeout_seconds(1, permission_checker_thread, data);
}

static void revoke_permissions (GtkButton *revoke_button2, gpointer data) {
    cleanup_window();

    g_mutex_lock(&communication_mutex);
    invalidate_session_requested = TRUE;

    scrobbling_enabled = FALSE;
    g_cond_signal(&communication_signal);
    g_mutex_unlock(&communication_mutex);

    gtk_widget_set_sensitive(button, TRUE);
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
  | details_label                    |
  |                                  |
  |,-additional_details_box--------. |
  ||_______________________________| |
  |__________________________________|

 */
static void *config_status_checker () {
    GtkWidget *config_box;
    GtkWidget *permission_box;
    GtkWidget *buttons_box;
    GtkWidget *additional_details_box;

    config_box              = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    permission_box          = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    buttons_box             = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
    additional_details_box  = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);

    button                  = gtk_button_new_with_mnemonic(N_("C_heck Permission"));
    revoke_button            = gtk_button_new_with_mnemonic(N_("_Revoke Permission"));
    gtk_widget_set_sensitive(revoke_button, FALSE);

    permission_status_icon  = gtk_image_new();
    permission_status_label = gtk_label_new("");

    details_label = gtk_label_new ("");
    gtk_label_set_use_markup(GTK_LABEL(details_label), TRUE);

    additional_details_icon  = gtk_image_new();
    additional_details_label = gtk_label_new("");


    g_signal_connect (button,        "clicked", G_CALLBACK (permission_checker), NULL);
    g_signal_connect (revoke_button, "clicked", G_CALLBACK (revoke_permissions), NULL);

    gtk_box_pack_start(GTK_BOX(permission_box), buttons_box,             FALSE, FALSE, 20);
    gtk_box_pack_start(GTK_BOX(permission_box), permission_status_icon,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(permission_box), permission_status_label, FALSE, FALSE, 5);

    gtk_box_pack_start(GTK_BOX(buttons_box), button,        FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(buttons_box), revoke_button, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(additional_details_box), additional_details_icon,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(additional_details_box), additional_details_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(config_box), permission_box,         FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(config_box), details_label,          FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(config_box), additional_details_box, FALSE, FALSE, 0);

    return config_box;
}


static const PreferencesWidget config_contents[] = {
    {
        .type  = WIDGET_LABEL,
        .label = N_("You need to allow Audacious to scrobble tracks to your Last.fm account.\n"),
        .data  = { .label = {.single_line = TRUE} }
    },
    {
        .type = WIDGET_CUSTOM,
        .data = { .populate = config_status_checker }
    }
};

const PluginPreferences configuration = {
    .widgets = config_contents,
    .n_widgets = G_N_ELEMENTS(config_contents)
};
