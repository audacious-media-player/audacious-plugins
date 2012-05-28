#include <audacious/i18n.h>
#include <libaudgui/libaudgui-gtk.h>

#include "config.h"
#include "echo.h"

static const char *echo_about_text =
N_("Echo Plugin\n"
   "By Johan Levin 1999.\n\n"
   "Surround echo by Carl van Schaik 1999");

void echo_about (void)
{
	static GtkWidget * echo_about_dialog = NULL;

	audgui_simple_message (& echo_about_dialog, GTK_MESSAGE_INFO,
	 _("About Echo Plugin"), _(echo_about_text));
}
