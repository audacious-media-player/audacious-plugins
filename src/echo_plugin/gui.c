#include "config.h"

#include <gtk/gtk.h>
#include <audacious/plugin.h>
#include <audacious/i18n.h>
#include "echo.h"

static const char *echo_about_text =
N_("Echo Plugin\n"
   "By Johan Levin 1999.\n\n"
   "Surround echo by Carl van Schaik 1999");

static GtkWidget *conf_dialog = NULL, *surround_btn;
static GtkObject *echo_delay_adj, *echo_feedback_adj, *echo_volume_adj;

void echo_about(void)
{
	static GtkWidget *echo_about_dialog = NULL;

	if (echo_about_dialog != NULL)
		return;

	echo_about_dialog = audacious_info_dialog(_("About Echo Plugin"),
					      _(echo_about_text), _("Ok"),
					      FALSE, NULL, NULL);
	gtk_signal_connect(GTK_OBJECT(echo_about_dialog), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			   &echo_about_dialog);
}

static void apply_changes(void)
{
	mcs_handle_t *cfg;
	echo_delay = GTK_ADJUSTMENT(echo_delay_adj)->value;
	echo_feedback = GTK_ADJUSTMENT(echo_feedback_adj)->value;
	echo_volume = GTK_ADJUSTMENT(echo_volume_adj)->value;
	echo_surround_enable =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(surround_btn));

	cfg = aud_cfg_db_open();
	aud_cfg_db_set_int(cfg, "echo_plugin", "delay", echo_delay);
	aud_cfg_db_set_int(cfg, "echo_plugin", "feedback", echo_feedback);
	aud_cfg_db_set_int(cfg, "echo_plugin", "volume", echo_volume);
	aud_cfg_db_set_bool(cfg, "echo_plugin", "enable_surround", echo_surround_enable);
	aud_cfg_db_close(cfg);
}

static void conf_ok_cb(GtkButton * button, gpointer data)
{
	apply_changes();
	gtk_widget_destroy(GTK_WIDGET(conf_dialog));
}

static void conf_cancel_cb(GtkButton * button, gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET(conf_dialog));
}

static void conf_apply_cb(GtkButton * button, gpointer data)
{
	apply_changes();
}

void echo_configure(void)
{
	GtkWidget *button, *table, *label, *hscale, *bbox;

	if (conf_dialog != NULL)
		return;

	conf_dialog = gtk_dialog_new();
	gtk_signal_connect(GTK_OBJECT(conf_dialog), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed), &conf_dialog);
	gtk_window_set_title(GTK_WINDOW(conf_dialog), _("Configure Echo"));

	echo_delay_adj = gtk_adjustment_new(echo_delay, 0, MAX_DELAY + 100, 10, 100, 100);
	echo_feedback_adj = gtk_adjustment_new(echo_feedback, 0, 100 + 10, 2, 10, 10);
	echo_volume_adj = gtk_adjustment_new(echo_volume, 0, 100 + 10, 2, 10, 10);

	table = gtk_table_new(2, 3, FALSE);
	gtk_table_set_col_spacings(GTK_TABLE(table), 5);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conf_dialog)->vbox), table,
			   TRUE, TRUE, 5);
	gtk_widget_show(table);

	label = gtk_label_new(_("Delay: (ms)"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);

	label = gtk_label_new(_("Feedback: (%)"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);

	label = gtk_label_new(_("Volume: (%)"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(label);

	hscale = gtk_hscale_new(GTK_ADJUSTMENT(echo_delay_adj));
	gtk_widget_set_usize(hscale, 400, 35);
	gtk_scale_set_digits(GTK_SCALE(hscale), 0);
	gtk_table_attach_defaults(GTK_TABLE(table), hscale, 1, 2, 0, 1);
	gtk_widget_show(hscale);

	hscale = gtk_hscale_new(GTK_ADJUSTMENT(echo_feedback_adj));
	gtk_widget_set_usize(hscale, 400, 35);
	gtk_scale_set_digits(GTK_SCALE(hscale), 0);
	gtk_table_attach_defaults(GTK_TABLE(table), hscale, 1, 2, 1, 2);
	gtk_widget_show(hscale);

	hscale = gtk_hscale_new(GTK_ADJUSTMENT(echo_volume_adj));
	gtk_widget_set_usize(hscale, 400, 35);
	gtk_scale_set_digits(GTK_SCALE(hscale), 0);
	gtk_table_attach_defaults(GTK_TABLE(table), hscale, 1, 2, 2, 3);
	gtk_widget_show(hscale);

	surround_btn = gtk_check_button_new_with_label(_("Surround echo"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(surround_btn),
				     echo_surround_enable);
	gtk_widget_show(surround_btn);
	
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(conf_dialog)->vbox), surround_btn,
			   TRUE, TRUE, 5);
	
	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
	gtk_box_pack_start(GTK_BOX((GTK_DIALOG(conf_dialog)->action_area)),
			   bbox, TRUE, TRUE, 0);


	button = gtk_button_new_with_label(_("Ok"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_ok_cb), NULL);
	gtk_widget_grab_default(button);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Cancel"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_cancel_cb), NULL);
	gtk_widget_show(button);

	button = gtk_button_new_with_label(_("Apply"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), button, TRUE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
			   GTK_SIGNAL_FUNC(conf_apply_cb), NULL);
	gtk_widget_show(button);
	gtk_widget_show(bbox);

	gtk_widget_show(conf_dialog);
}
