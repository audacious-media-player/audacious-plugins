#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>

#include <gtk/gtk.h>

#include "vtx.h"
#include "ayemu.h"

void
vtx_about (void)
{
  static GtkWidget *box = NULL;
  if (box) {
      gtk_window_present(GTK_WINDOW(box));
  } else {
	box = audacious_info_dialog (_("About Vortex Player"),
				_
				("Vortex file format player by Sashnov Alexander <sashnov@ngs.ru>\n"
				"Founded on original source in_vtx.dll by Roman Sherbakov <v_soft@microfor.ru>\n"
				"\n"
				"Music in vtx format can be found at http://vtx.microfor.ru/music.htm\n"
				"and other AY/YM music sites.\n"
				"\n"
				"Audacious implementation by Pavel Vymetalek <pvymetalek@seznam.cz>"),
				_("Ok"), FALSE, NULL, NULL);
	g_signal_connect (G_OBJECT (box), "destroy", G_CALLBACK(gtk_widget_destroyed), &box);
  }
}
