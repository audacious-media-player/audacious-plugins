#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <audacious/i18n.h>
#include "config.h"

#include "configure.h"

extern cdng_cfg_t cdng_cfg;

static GtkWidget *configwindow = NULL,
                 *okbutton = NULL,
                 *cancelbutton = NULL,
                 *maintable = NULL,
                 *daeframe = NULL,
                 *titleinfoframe = NULL,
                 *miscframe = NULL,
                 *daetable = NULL,
                 *titleinfotable = NULL,
                 *misctable = NULL,
                 *limitcheckbutton = NULL,
                 *usecdtextcheckbutton = NULL,
                 *usecddbcheckbutton = NULL,
                 *cddbserverlabel = NULL,
                 *cddbpathlabel = NULL,
                 *cddbportlabel = NULL,
                 *cddbserverentry = NULL,
                 *cddbpathentry = NULL,
                 *cddbportentry = NULL,
                 *cddbhttpcheckbutton = NULL,
                 *usedevicecheckbutton = NULL,
                 *buttonbox = NULL,
                 *limitspinbutton = NULL,
                 *deviceentry = NULL,
                 *debugcheckbutton = NULL;


static void configure_values_to_gui(void)
{
	gchar portstr[16];

	/*gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usedaecheckbutton), cdng_cfg.use_dae);*/

	/*gtk_widget_set_sensitive(limitcheckbutton, cdng_cfg.use_dae);*/
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(limitcheckbutton), cdng_cfg.limitspeed > 0);

	/*gtk_widget_set_sensitive(limitspinbutton, cdng_cfg.use_dae && cdng_cfg.limitspeed > 0);*/
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(limitspinbutton), cdng_cfg.limitspeed);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usecdtextcheckbutton), cdng_cfg.use_cdtext);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton), cdng_cfg.use_cddb);

	gtk_entry_set_text(GTK_ENTRY(cddbserverentry), cdng_cfg.cddb_server);
	gtk_entry_set_text(GTK_ENTRY(cddbpathentry), cdng_cfg.cddb_path);
	g_snprintf(portstr, sizeof(portstr), "%d", cdng_cfg.cddb_port);
	gtk_entry_set_text(GTK_ENTRY(cddbportentry), portstr);
	gtk_widget_set_sensitive(cddbserverentry, cdng_cfg.use_cddb);
	gtk_widget_set_sensitive(cddbpathentry, cdng_cfg.use_cddb);
	gtk_widget_set_sensitive(cddbportentry, cdng_cfg.use_cddb);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cddbhttpcheckbutton), cdng_cfg.cddb_http);
	gtk_widget_set_sensitive(cddbhttpcheckbutton, cdng_cfg.use_cddb);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usedevicecheckbutton), strlen(cdng_cfg.device) > 0);

	gtk_widget_set_sensitive(deviceentry, strlen(cdng_cfg.device) > 0);
	gtk_entry_set_text(GTK_ENTRY(deviceentry), cdng_cfg.device);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(debugcheckbutton), cdng_cfg.debug);
}


static void configure_gui_to_values(void)
{
	/*usedae = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedaecheckbutton));*/
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(limitcheckbutton)))
		cdng_cfg.limitspeed = gtk_spin_button_get_value(GTK_SPIN_BUTTON(limitspinbutton));
	else
		cdng_cfg.limitspeed = 0;
	
	cdng_cfg.use_cdtext = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecdtextcheckbutton));
	cdng_cfg.use_cddb = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton));
	pstrcpy(&cdng_cfg.cddb_server, gtk_entry_get_text(GTK_ENTRY(cddbserverentry)));
	pstrcpy(&cdng_cfg.cddb_path, gtk_entry_get_text(GTK_ENTRY(cddbpathentry)));
	cdng_cfg.cddb_http = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddbhttpcheckbutton));
	cdng_cfg.cddb_port = strtol(gtk_entry_get_text(GTK_ENTRY(cddbportentry)), NULL, 10);
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedevicecheckbutton)))
		pstrcpy(&cdng_cfg.device, gtk_entry_get_text(GTK_ENTRY(deviceentry)));
	else
		pstrcpy(&cdng_cfg.device, "");
	
	cdng_cfg.debug = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(debugcheckbutton));
}


static gboolean delete_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	(void) widget; (void) event; (void) data;
	gtk_widget_hide(configwindow);
	return TRUE;
}


static void button_clicked(GtkWidget *widget, gpointer data)
{
	(void) data;
	gtk_widget_hide(configwindow);
	if (widget == okbutton)
		configure_gui_to_values();
}


static void checkbutton_toggled(GtkWidget *widget, gpointer data)
{
	(void) widget; (void) data;
	
	/*gtk_widget_set_sensitive(limitcheckbutton, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedaecheckbutton)));*/

	gtk_widget_set_sensitive(limitspinbutton,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(limitcheckbutton)) &&
		GTK_WIDGET_IS_SENSITIVE(limitcheckbutton));

	gtk_widget_set_sensitive(cddbserverentry,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton)));

	gtk_widget_set_sensitive(cddbpathentry,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton)));

	gtk_widget_set_sensitive(cddbhttpcheckbutton,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton)));
	
	gtk_widget_set_sensitive(cddbportentry,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton)));
	
	gtk_widget_set_sensitive(deviceentry,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedevicecheckbutton)));

	/*		
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cddbhttpcheckbutton)) && (widget CDDEBUG("use_dae = %d, limitspeed = %d, use_cdtext = %d, use_cddb = %d, cddbserver = \"%s\", cddbport = %d, cddbhttp = %d, device = \"%s\", debug = %d\n",
	cdng_cfg.use_dae, cdng_cfg.limitspeed, cdng_cfg.use_cdtext, cdng_cfg.use_cddb,
	cdng_cfg.cddb_server, cdng_cfg.cddb_port, cdng_cfg.cddb_http, cdng_cfg.device, cdng_cfg.debug);== cddbhttpcheckbutton))
		gtk_entry_set_text(GTK_ENTRY(cddbportentry), _("80"));
	else
		gtk_entry_set_text(GTK_ENTRY(cddbportentry), _("8880"));
	*/
}


void configure_create_gui()
{
	configwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(configwindow), _("CD Audio Plugin Configuration"));
	gtk_window_set_resizable(GTK_WINDOW(configwindow), FALSE);
	gtk_window_set_position(GTK_WINDOW(configwindow), GTK_WIN_POS_CENTER_ALWAYS);
	gtk_container_set_border_width(GTK_CONTAINER(configwindow), 10);
	g_signal_connect(G_OBJECT(configwindow), "delete_event", G_CALLBACK(delete_window), NULL);

	maintable = gtk_table_new(4, 2, TRUE);
	gtk_table_set_homogeneous(GTK_TABLE(maintable), FALSE);
	gtk_container_add(GTK_CONTAINER(configwindow), maintable);

	daeframe = gtk_frame_new(_("Digital audio extraction"));
	gtk_table_attach_defaults(GTK_TABLE(maintable), daeframe, 0, 2, 0, 1);
	daetable = gtk_table_new(1, 2, TRUE);
	gtk_container_add(GTK_CONTAINER(daeframe), daetable);

	titleinfoframe = gtk_frame_new(_("Title information"));
	gtk_table_attach_defaults(GTK_TABLE(maintable), titleinfoframe, 0, 2, 1, 2);
	titleinfotable = gtk_table_new(2, 2, TRUE);
	gtk_container_add(GTK_CONTAINER(titleinfoframe), titleinfotable);

	miscframe = gtk_frame_new(_("Misc"));
	gtk_table_attach_defaults(GTK_TABLE(maintable), miscframe, 0, 2, 2, 3);
	misctable = gtk_table_new(2, 2, TRUE);
	gtk_container_add(GTK_CONTAINER(miscframe), misctable);


	/*
	usedaecheckbutton = gtk_check_button_new_with_label(_("Use digital audio extraction"));
	g_signal_connect(G_OBJECT(usedaecheckbutton), "toggled", G_CALLBACK(checkbutton_toggled), NULL);
	gtk_table_attach_defaults(GTK_TABLE(daetable), usedaecheckbutton, 0, 2, 0, 1);
	*/

	limitcheckbutton = gtk_check_button_new_with_label(_("Limit read speed to: "));
	g_signal_connect(G_OBJECT(limitcheckbutton), "toggled", G_CALLBACK(checkbutton_toggled), NULL);
	gtk_table_attach_defaults(GTK_TABLE(daetable), limitcheckbutton, 0, 1, 0, 1);

	limitspinbutton = gtk_spin_button_new_with_range(1.0, 24.0, 1.0);
	gtk_table_attach_defaults(GTK_TABLE(daetable), limitspinbutton, 1, 2, 0, 1);

	usecdtextcheckbutton = gtk_check_button_new_with_label(_("Use cd-text if available"));
	g_signal_connect(G_OBJECT(usecdtextcheckbutton), "toggled", G_CALLBACK(checkbutton_toggled), NULL);
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), usecdtextcheckbutton, 0, 1, 0, 1);

	usecddbcheckbutton = gtk_check_button_new_with_label(_("Use CDDB if available"));
	g_signal_connect(G_OBJECT(usecddbcheckbutton), "toggled", G_CALLBACK(checkbutton_toggled), NULL);
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), usecddbcheckbutton, 0, 1, 1, 2);

	cddbserverlabel = gtk_label_new(_("Server: "));
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbserverlabel, 0, 1, 2, 3);

	cddbpathlabel = gtk_label_new(_("Path: "));
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbpathlabel, 0, 1, 3, 4);

	cddbportlabel = gtk_label_new(_("Port: "));
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbportlabel, 0, 1, 4, 5);

	cddbserverentry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbserverentry, 1, 2, 2, 3);

	cddbpathentry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbpathentry, 1, 2, 3, 4);

	cddbhttpcheckbutton = gtk_check_button_new_with_label(_("Use HTTP instead of CDDBP"));
	g_signal_connect(G_OBJECT(cddbhttpcheckbutton), "toggled", G_CALLBACK(checkbutton_toggled), NULL);
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbhttpcheckbutton, 1, 2, 1, 2);

	cddbportentry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbportentry, 1, 2, 4, 5);


	usedevicecheckbutton = gtk_check_button_new_with_label(_("Override default device: "));
	g_signal_connect(G_OBJECT(usedevicecheckbutton), "toggled", G_CALLBACK(checkbutton_toggled), NULL);
	gtk_table_attach_defaults(GTK_TABLE(misctable), usedevicecheckbutton, 0, 1, 0, 1);

	deviceentry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(misctable), deviceentry, 1, 2, 0, 1);

	debugcheckbutton = gtk_check_button_new_with_label(_("Print debug information"));
	g_signal_connect(G_OBJECT(debugcheckbutton), "toggled", G_CALLBACK(checkbutton_toggled), NULL);
	gtk_table_attach_defaults(GTK_TABLE(misctable), debugcheckbutton, 0, 1, 1, 2);


	buttonbox = gtk_hbutton_box_new();
	gtk_hbutton_box_set_layout_default(GTK_BUTTONBOX_END);
	gtk_hbutton_box_set_spacing_default(10);
	gtk_table_attach_defaults(GTK_TABLE(maintable), buttonbox, 0, 2, 3, 4);

	okbutton = gtk_button_new_with_label(_("Ok"));
	g_signal_connect(G_OBJECT(okbutton), "clicked", G_CALLBACK(button_clicked), NULL);
	gtk_container_add(GTK_CONTAINER(buttonbox), okbutton);

	cancelbutton = gtk_button_new_with_label(_("Cancel"));
	g_signal_connect(G_OBJECT(cancelbutton), "clicked", G_CALLBACK(button_clicked), NULL);
	gtk_container_add(GTK_CONTAINER(buttonbox), cancelbutton);


	/*gtk_widget_show(usedaecheckbutton);*/
	gtk_widget_show(limitcheckbutton);
	gtk_widget_show(limitspinbutton);
	gtk_widget_show(usecdtextcheckbutton);
	gtk_widget_show(usecddbcheckbutton);
	gtk_widget_show(cddbserverentry);
	gtk_widget_show(cddbpathentry);
	gtk_widget_show(cddbhttpcheckbutton);
	gtk_widget_show(cddbportentry);
	gtk_widget_show(cddbserverlabel);
	gtk_widget_show(cddbpathlabel);
	gtk_widget_show(cddbportlabel);
	gtk_widget_show(usedevicecheckbutton);
	gtk_widget_show(deviceentry);
	gtk_widget_show(debugcheckbutton);

	gtk_widget_show(daetable);
	gtk_widget_show(daeframe);
	gtk_widget_show(titleinfotable);
	gtk_widget_show(titleinfoframe);
	gtk_widget_show(misctable);
	gtk_widget_show(miscframe);

	gtk_widget_show(buttonbox);
	gtk_widget_show(okbutton);
	gtk_widget_show(cancelbutton);

	gtk_widget_show(maintable);
}


void configure_show_gui(void)
{
    if (configwindow == NULL)
        configure_create_gui();

	configure_values_to_gui();
	gtk_widget_show(configwindow);
	gtk_window_present(GTK_WINDOW(configwindow));
}


gint pstrcpy(gchar **res, const gchar *str)
{
	if (!res || !str) return -1;

	g_free(*res);
	if ((*res = (gchar *) g_malloc(strlen(str) + 1)) == NULL)
		return -2;

	strcpy(*res, str);
	return 0;
}

