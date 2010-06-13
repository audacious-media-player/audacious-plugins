
#include <glib.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "config.h"

void about_show(void)
{
	static GtkWidget *aboutbox = NULL;
	gchar *tmp;

	tmp = g_strdup_printf(_("Audacious AudioScrobbler Plugin\n\n"
				"Originally created by Audun Hove <audun@nlc.no> and Pipian <pipian@pipian.com>\n"));
	audgui_simple_message(&aboutbox, GTK_MESSAGE_INFO, _("About Scrobbler Plugin"),
			tmp);

	g_free(tmp);
}

void errorbox_show(char *errortxt)
{
	gchar *tmp;

#if 0
	tmp = g_strdup_printf(_("There has been an error"
			" that may require your attention.\n\n"
			"Contents of server error:\n\n"
			"%s\n"),
			errortxt);

	GDK_THREADS_ENTER();
	audacious_info_dialog(_("Scrobbler Error"),
			tmp,
			_("OK"), FALSE, NULL, NULL);
	GDK_THREADS_LEAVE();

	g_free(tmp);
#endif
}
