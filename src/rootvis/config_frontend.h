#include <config.h>

#define VBOX  	1
#define HBOX  	2
#define HBBOX 	3
#define HBBOX2	4
#define VBBOX 	5
#define FRAME 	6

#define ATTACH_TO_NOTEBOOK  1
#define ATTACH_TO_CONTAINER 2
#define ATTACH_TO_BOX       3

#define ENTRY 1
#define COMBO 2

// this struct contains all we need to change a color:
struct rootvis_colorsel
{
	GtkWidget *preview;           // the preview we need to update.
	GtkWidget *button;            // the button that made it all
			// ( to keep pressed ). FIXME
	GtkWidget *label;             // the name left of the button.
	GtkWidget *color_picker;      // the colorpicker that was launched.
	GtkWidget *window;            // the window we launched.
	gdouble   color[4];           // the color.
	gdouble   saved_color[4];
	char      *name;              // the name.
				// (we'll use this to tune the colorpicker).
	char      *complete_name;     // this is for the window's title.
};

// this is to contain pointers to various widgets...
struct rootvis_frontend
{
	GtkWidget *window_main;
	GtkWidget *window_channel[2];
	GtkWidget *stereo_status[2];
	GtkWidget *stereo_check;
	GtkWidget *debug_check;
} widgets;

void config_hide(int);
void config_set_widgets(int);

int signal_window_close(GtkWidget *window, gpointer data);
void signal_check_toggled(GtkWidget *togglebutton, gpointer data);
void signal_stereo_toggled(GtkWidget *togglebutton, gpointer data);
void signal_textentry_changed(GtkWidget *entry, gpointer data);
void signal_toggle_colorselector(GtkWidget *button, struct config_value* cvar);
void signal_colorselector_ok(GtkWidget *button, struct config_value* cvar);
void signal_colorselector_cancel(GtkWidget *button, struct config_value* cvar);
void signal_colorselector_update(GtkWidget *w, struct config_value* cvar);

void signal_revert(GtkWidget *togglebutton, gpointer data);
void signal_save(GtkWidget *togglebutton, gpointer data);
void signal_show(GtkWidget *togglebutton, gpointer data);
void signal_hide(GtkWidget *togglebutton, gpointer data);


void frontend_update_color(struct config_value* cvar, int system);
void frontend_set_color(struct config_value* cvar);
