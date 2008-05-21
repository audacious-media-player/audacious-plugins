
#include <stdlib.h>
#include <gtk/gtk.h>
#include <audacious/plugin.h>
#include <audacious/ui_plugin_menu.h>

#include "streambrowser.h"
#include "streamdir.h"
#include "shoutcast.h"


static void			sb_init();
static void 			sb_about();
static void 			sb_configure();
static void 			sb_cleanup();

static void 			menu_click();
static void 			add_plugin_services_menu_item();


static GeneralPlugin sb_plugin = 
{
	.description = "Stream Browser",
	.init = sb_init,
	.about = sb_about,
	.configure = sb_configure,
	.cleanup = sb_cleanup
};

GeneralPlugin *sb_gplist[] = 
{
	&sb_plugin,
	NULL
};

SIMPLE_GENERAL_PLUGIN(streambrowser, sb_gplist);


void debug(const char *fmt, ...)
{
	// todo: replace with config->debug
	if (TRUE) {
		va_list ap;
		fprintf(stderr, "* streambrowser: ");
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

void error(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "! streambrowser: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}


static void sb_init()
{
	debug("sb_init()\n");

	//shoutcast_streamdir_fetch();	
}

static void sb_about()
{
	debug("sb_about()\n");
}

static void sb_configure()
{
	debug("sb_configure()\n");
}

static void sb_cleanup()
{
	debug("sb_cleanup()\n");
}

static void menu_click()
{
	debug("menu_click()\n");
}

static void add_plugin_services_menu_item()
{
	/*
	GtkWidget *menu_item;

	menu_item = gtk_image_menu_item_new_with_label("SB Test");
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), gtk_image_new_from_stock(GTK_STOCK_CDROM, GTK_ICON_SIZE_MENU));
	gtk_widget_show(menu_item);
	audacious_menu_plugin_item_add(AUDACIOUS_MENU_PLAYLIST_RCLICK, menu_item);
	g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(menu_click), NULL);
	*/
}

