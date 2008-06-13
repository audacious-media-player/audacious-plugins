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

static void about(void)
{
	static GtkWidget *dialog;

	if (dialog)
		return;

	dialog = audacious_info_dialog(_("About aRts Output"),
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
	.description = "aRts Output Plugin",
	.init = artsxmms_init,
	.cleanup = NULL,
	.about = about,
	.configure = artsxmms_configure,
	.get_volume = artsxmms_get_volume,
	.set_volume = artsxmms_set_volume,
	.open_audio = artsxmms_open,
	.write_audio = artsxmms_write,
	.close_audio = artsxmms_close,
	.flush = artsxmms_flush,
	.pause = artsxmms_pause,
	.buffer_free = artsxmms_free,
	.buffer_playing = artsxmms_playing,
	.output_time = artsxmms_get_output_time,
	.written_time = artsxmms_get_written_time,
	.tell_audio = artsxmms_tell_audio
};

OutputPlugin *arts_oplist[] = { &arts_op, NULL };

DECLARE_PLUGIN(arts, NULL, NULL, NULL, arts_oplist, NULL, NULL, NULL, NULL);
