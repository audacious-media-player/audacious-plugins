/*
 *  aRts ouput plugin for xmms
 *
 *  Copyright (C) 2000,2003  Haavard Kvaalen <havardk@xmms.org>
 *
 *  Licenced under GNU GPL version 2.
 *
 *  Audacious port by Giacomo Lozito from develia.org
 *
 */

#include "arts.h"
#include "audacious/util.h"

static void about(void)
{
	static GtkWidget *dialog;

	if (dialog)
		return;

	dialog = xmms_show_message(_("About aRts Output"),
				   _("aRts output plugin by "
				   "H\303\245vard Kv\303\245len <havardk@xmms.org>\n"
				   "Audacious port by Giacomo Lozito from develia.org"),
				   _("Ok"), FALSE, NULL, NULL);
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed),
			   &dialog);
}


OutputPlugin arts_op =
{
	NULL,
	NULL,
	"aRts Output Plugin",
	artsxmms_init,
	NULL,
	about,
	artsxmms_configure,
	artsxmms_get_volume,
	artsxmms_set_volume,
	artsxmms_open,
	artsxmms_write,
	artsxmms_close,
	artsxmms_flush,
	artsxmms_pause,
	artsxmms_free,
	artsxmms_playing,
	artsxmms_get_output_time,
	artsxmms_get_written_time,
	artsxmms_tell_audio
};

OutputPlugin *arts_oplist[] = { &arts_op, NULL };

DECLARE_PLUGIN(arts, NULL, NULL, NULL, arts_oplist, NULL, NULL, NULL, NULL);
