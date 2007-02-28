#include <audacious/plugin.h>
#include <audacious/util.h>
#include <audacious/output.h>
#include <audacious/titlestring.h>
#include <audacious/vfs.h>
#include <audacious/strings.h>
#include <audacious/i18n.h>

#include "ayemu.h"
#include "vtx.h"

void vtx_file_info(gchar *filename)
{
  static GtkWidget *box;
  ayemu_vtx_t vtx;

  if (!ayemu_vtx_open(&vtx, filename))
    {
      fprintf(stderr, "Can't open file %s\n", filename);
      return;
    }
  else
    {
      gchar head[1024];
      gchar body[8192];

      sprintf(head, "Details about %s", filename);

	  ayemu_vtx_sprintname(&vtx, body, sizeof(body),
	      "Title: %t\n"
	      "Author: %a\n"
	      "From : %f\n"
	      "Tracker : %T\n"
	      "Comment : %C\n"
	      "Chip type: %c\n"
	      "Stereo: %s\n"
	      "Loop: %l\n"
	      "Chip freq: %F\n"
	      "Player Freq:%P\n"
	      "Year: %y");
      
      box = xmms_show_message (head,
			       body,
			       _("Ok"), FALSE, NULL, NULL);
      
      
    }
  g_signal_connect (G_OBJECT (box), "destroy", G_CALLBACK(gtk_widget_destroyed),  &box);
}
