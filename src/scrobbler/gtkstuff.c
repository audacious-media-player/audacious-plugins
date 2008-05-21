
#include <glib.h>
#include <audacious/i18n.h>
#include <audacious/plugin.h>

#include <stdio.h>
#include <string.h>
#include "settings.h"
#include "config.h"

void about_show(void)
{
	static GtkWidget *aboutbox;
	gchar *tmp;
	if (aboutbox)
		return;

	tmp = g_strdup_printf(_("Audacious AudioScrobbler Plugin\n\n"
				"Originally created by Audun Hove <audun@nlc.no> and Pipian <pipian@pipian.com>\n"));
	aboutbox = audacious_info_dialog(_("About Scrobbler Plugin"),
			tmp,
			_("Ok"), FALSE, NULL, NULL);

	g_free(tmp);
	gtk_signal_connect(GTK_OBJECT(aboutbox), "destroy",
			GTK_SIGNAL_FUNC(gtk_widget_destroyed), &aboutbox);
}

void errorbox_show(char *errortxt)
{
	gchar *tmp;

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
}
