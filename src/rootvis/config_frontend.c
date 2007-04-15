#include <stdarg.h>
#include <string.h>

#include <rootvis.h>

#include <config_frontend.h>

extern GtkWidget *frontend_create_channel(int channel);
extern GtkWidget *frontend_create_main(void);
extern void frontend_create_colorpicker(struct config_value*);

void signal_revert(GtkWidget *togglebutton, gpointer data)
{
	config_revert(GPOINTER_TO_INT(data));
}

void signal_save(GtkWidget *togglebutton, gpointer data)
{
	config_save(GPOINTER_TO_INT(data));
}

void signal_show(GtkWidget *togglebutton, gpointer data)
{
	config_show(GPOINTER_TO_INT(data));
}

void signal_hide(GtkWidget *togglebutton, gpointer data)
{
	config_hide(GPOINTER_TO_INT(data));
}

void signal_toggle_colorselector(GtkWidget *w, struct config_value *cvar)
{
	struct rootvis_colorsel* colorsel = cvar->valc.frontend;
	if (colorsel->window == NULL)
		frontend_create_colorpicker(cvar);
	gtk_widget_show(colorsel->window);
}

void signal_colorselector_ok(GtkWidget *w, struct config_value *cvar)
{
	struct rootvis_colorsel* colorsel = cvar->valc.frontend;
	gtk_color_selection_get_color(GTK_COLOR_SELECTION(colorsel->color_picker), colorsel->color);
	memcpy(colorsel->saved_color, colorsel->color, COLORSIZE*sizeof(gdouble));
	frontend_update_color(cvar, 1);
	gtk_widget_hide(colorsel->window);
}

void signal_colorselector_cancel(GtkWidget *w, struct config_value *cvar)
{
	struct rootvis_colorsel* colorsel = cvar->valc.frontend;
	memcpy(colorsel->color, colorsel->saved_color, COLORSIZE*sizeof(gdouble));
	frontend_update_color(cvar, 1);
	gtk_widget_destroy(colorsel->window);
	colorsel->window = NULL;
}

void signal_colorselector_update(GtkWidget *w, struct config_value *cvar)
{
	struct rootvis_colorsel* colorsel = cvar->valc.frontend;
	gtk_color_selection_get_color(GTK_COLOR_SELECTION(colorsel->color_picker), colorsel->color);
	frontend_update_color(cvar, 1);
}

void color_char2double(unsigned char source[4], gdouble dest[4])
{
	int i;
	for (i = 0; i < COLORSIZE; ++i)
	{
		dest[i] = (double)source[i] / 255.0;
	}
}

void color_double2char(double source[4], unsigned char dest[4])
{
	int i;
	for (i = 0; i < COLORSIZE; ++i)
	{
		dest[i] = (int)(source[i] * 255.0);
	}
}

// This was ripped from xmms-iris's config.c.
void frontend_update_color(struct config_value *cvar, int system)
{
	struct rootvis_colorsel* colorsel = cvar->valc.frontend;
	if (system > 0)
	{
		threads_lock();
		color_double2char(colorsel->color, cvar->valc.var);
		threads_unlock(2);
	}

	// following is among the dumbest shit I've ever seen. GtkPreview seems to suck a lot!
	unsigned int i;
	guchar color_buf[3*30];
	char red, green, blue;
	red = colorsel->color[RED]*0xff;
	green = colorsel->color[GREEN]*0xff;
	blue = colorsel->color[BLUE]*0xff;
	for (i = 0; i < 30*3; i += 3) {
		color_buf[i+RED] = (char) red;
		color_buf[i+GREEN] = (char) green;
		color_buf[i+BLUE] = (char) blue;
	}
	for (i = 0; i < 30; i++)
		gtk_preview_draw_row(GTK_PREVIEW(colorsel->preview), color_buf, 0, i, 30);
	gtk_widget_draw(colorsel->preview, NULL);
}

void frontend_set_color(struct config_value *cvar)
{
	struct rootvis_colorsel* colorsel = cvar->valc.frontend;
	color_char2double(cvar->valc.var, colorsel->color);
	memcpy(colorsel->saved_color, colorsel->color, COLORSIZE*sizeof(gdouble));
}

/* following functions catch signals from the gui widgets */

int signal_window_close(GtkWidget *window, gpointer data)
{
	int number = 2;
	if (window == widgets.window_channel[0]) number = 0;
	if (window == widgets.window_channel[1]) number = 1;
	config_hide(number);
	return TRUE;
}

void signal_check_toggled(GtkWidget *togglebutton, gpointer data)
{
	printf("%s \n", (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton))) ? "TRUE" : "FALSE");
}

void signal_stereo_toggled(GtkWidget *togglebutton, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(togglebutton))) {
		gtk_label_set_text(GTK_LABEL(widgets.stereo_status[0]), "renders left channel");
		gtk_label_set_text(GTK_LABEL(widgets.stereo_status[1]), "renders right channel");
	} else {
		gtk_label_set_text(GTK_LABEL(widgets.stereo_status[0]), "renders both channels");
		gtk_label_set_text(GTK_LABEL(widgets.stereo_status[1]), "unused / inactive");
	}
}

void signal_textentry_changed(GtkWidget *entry, gpointer data)
{
}

void config_show_channel(int channel) {
	if (widgets.window_channel[channel] == NULL) {
		widgets.window_channel[channel] = frontend_create_channel(channel);
	} else {
		print_status("raising channel window");
		gtk_widget_show(widgets.window_channel[channel]);
	}
}

void config_show(int channel) {
	if (channel == 2)
	{
		if (widgets.window_main == NULL) {
			widgets.window_main = frontend_create_main();
		} else {
			print_status("raising windows");
			gtk_widget_show(widgets.window_main);
			if (widgets.window_channel[0] != NULL) {
				gtk_widget_show(widgets.window_channel[0]);
			}
			if (widgets.window_channel[1] != NULL) {
				gtk_widget_show(widgets.window_channel[1]);
			}
		}
	} else config_show_channel(channel);
}

void config_hide(int number) {
	/* hide or destroy? if destroy, pointers must be set to NULL */
	if (number < 2) {
		if (widgets.window_channel[number] != NULL)
			gtk_widget_hide(widgets.window_channel[number]);
		/* widgets.window_channel[number] = NULL; */
	} else {
		if (widgets.window_main != NULL)
			gtk_widget_hide(widgets.window_main);
		if (widgets.window_channel[0] != NULL)
			gtk_widget_hide(widgets.window_channel[0]);
		if (widgets.window_channel[1] != NULL)
			gtk_widget_hide(widgets.window_channel[1]);
		widgets.window_main = NULL;
		widgets.window_channel[0] = NULL;
		widgets.window_channel[1] = NULL;
		// THIS SUCKS BEYOND REPAIR!
	}
}

void config_set_widgets(int number)
{
	if (number < 2) {
		// set per channel stuff
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.stereo_check), ((conf.stereo>0) ? TRUE : FALSE));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.debug_check), ((conf.debug>0) ? TRUE : FALSE));
	}
}

void config_frontend_init(void) {
	widgets.window_main = NULL;
	widgets.window_channel[0] = NULL;
	widgets.window_channel[1] = NULL;
}
