
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <audacious/i18n.h>
#include "config.h"

#include "configure.h"


static GtkWidget		*configwindow;
static GtkWidget		*okbutton;
static GtkWidget		*cancelbutton;
static GtkWidget		*maintable;
static GtkWidget		*daeframe;
static GtkWidget		*titleinfoframe;
static GtkWidget		*miscframe;
static GtkWidget		*daetable;
static GtkWidget		*titleinfotable;
static GtkWidget		*misctable;
/*static GtkWidget		*usedaecheckbutton;*/
static GtkWidget		*limitcheckbutton;
static GtkWidget		*usecdtextcheckbutton;
static GtkWidget		*usecddbcheckbutton;
static GtkWidget		*cddbserverlabel;
static GtkWidget		*cddbportlabel;
static GtkWidget		*cddbserverentry;
static GtkWidget		*cddbportentry;
static GtkWidget		*usedevicecheckbutton;
static GtkWidget		*buttonbox;
static GtkWidget		*limitspinbutton;
static GtkWidget		*deviceentry;
static GtkWidget		*debugcheckbutton;

/*static gboolean			*usedae;*/
static int				*limitspeed;
static gboolean			*usecdtext;
static gboolean			*usecddb;
static char				*device;
static gboolean			*debug;
static char				*cddbserver;
static int				*cddbport;

static gboolean			delete_window(GtkWidget *widget, GdkEvent *event, gpointer data);
static void				button_clicked(GtkWidget *widget, gpointer data);
static void				checkbutton_toggled(GtkWidget *widget, gpointer data);
static void				values_to_gui();
static void				gui_to_values();


void configure_set_variables(/*gboolean *_usedae, */gint *_limitspeed, gboolean *_usecdtext, gboolean *_usecddb, gchar *_device, gboolean *_debug, gchar *_cddbserver, gint *_cddbport)
{
	/*usedae = _usedae;*/
	limitspeed = _limitspeed;
	usecdtext = _usecdtext;
	usecddb = _usecddb;
	device = _device;
	debug = _debug;
	cddbserver = _cddbserver;
	cddbport = _cddbport;
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
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), usecdtextcheckbutton, 0, 2, 0, 1);

	usecddbcheckbutton = gtk_check_button_new_with_label(_("Use CDDB if available"));
	g_signal_connect(G_OBJECT(usecddbcheckbutton), "toggled", G_CALLBACK(checkbutton_toggled), NULL);
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), usecddbcheckbutton, 0, 2, 1, 2);

	cddbserverlabel = gtk_label_new(_("Server: "));
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbserverlabel, 0, 1, 2, 3);

	cddbportlabel = gtk_label_new(_("Port: "));
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbportlabel, 0, 1, 3, 4);

	cddbserverentry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbserverentry, 1, 2, 2, 3);

	cddbportentry = gtk_entry_new();
	gtk_table_attach_defaults(GTK_TABLE(titleinfotable), cddbportentry, 1, 2, 3, 4);


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
	gtk_widget_show(cddbportentry);
	gtk_widget_show(cddbserverlabel);
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

void configure_show_gui()
{
	values_to_gui();
	gtk_widget_show(configwindow);
}

gboolean delete_window(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gtk_widget_hide(configwindow);
	return TRUE;
}

void button_clicked(GtkWidget *widget, gpointer data)
{
	gtk_widget_hide(configwindow);
	if (widget == okbutton)
		gui_to_values();
}


void checkbutton_toggled(GtkWidget *widget, gpointer data)
{
	/*gtk_widget_set_sensitive(limitcheckbutton, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedaecheckbutton)));*/

	gtk_widget_set_sensitive(
		limitspinbutton,
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(limitcheckbutton)) &&
		GTK_WIDGET_IS_SENSITIVE(limitcheckbutton));

	gtk_widget_set_sensitive(cddbserverentry, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton)));
	gtk_widget_set_sensitive(cddbportentry, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton)));

	gtk_widget_set_sensitive(deviceentry, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedevicecheckbutton)));
}

void values_to_gui()
{
	gchar portstr[16];

	/*gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usedaecheckbutton), *usedae);*/

	/*gtk_widget_set_sensitive(limitcheckbutton, *usedae);*/
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(limitcheckbutton), *limitspeed > 0);

	/*gtk_widget_set_sensitive(limitspinbutton, *usedae && *limitspeed > 0);*/
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(limitspinbutton), *limitspeed);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usecdtextcheckbutton), *usecdtext);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton), *usecddb);

	g_snprintf(portstr, sizeof(portstr), "%d", *cddbport);
	gtk_entry_set_text(GTK_ENTRY(cddbserverentry), cddbserver);
	gtk_entry_set_text(GTK_ENTRY(cddbportentry), portstr);
	gtk_widget_set_sensitive(cddbserverentry, *usecddb);
	gtk_widget_set_sensitive(cddbportentry, *usecddb);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(usedevicecheckbutton), strlen(device) > 0);

	gtk_widget_set_sensitive(deviceentry, strlen(device) > 0);
	gtk_entry_set_text(GTK_ENTRY(deviceentry), device);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(debugcheckbutton), *debug);
}

void gui_to_values()
{
	/*usedae = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedaecheckbutton));*/
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(limitcheckbutton)))
		*limitspeed = gtk_spin_button_get_value(GTK_SPIN_BUTTON(limitspinbutton));
	else
		*limitspeed = 0;
	*usecdtext = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecdtextcheckbutton));
	*usecddb = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usecddbcheckbutton));
	strncpy(cddbserver, gtk_entry_get_text(GTK_ENTRY(cddbserverentry)), strlen(gtk_entry_get_text(GTK_ENTRY(cddbserverentry))) + 1);
	*cddbport = strtol(gtk_entry_get_text(GTK_ENTRY(cddbportentry)), NULL, 10);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(usedevicecheckbutton)))
		strncpy(device, gtk_entry_get_text(GTK_ENTRY(deviceentry)), strlen(gtk_entry_get_text(GTK_ENTRY(deviceentry))) + 1);
	else
		strcpy(device, "");
	*debug = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(debugcheckbutton));
}
