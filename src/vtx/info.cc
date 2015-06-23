#include "ayemu.h"
#include "vtx.h"

#ifdef USE_GTK

#include <libaudcore/audstrings.h>
#include <libaudcore/i18n.h>
#include <libaudcore/runtime.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

void vtx_file_info(const char *filename, VFSFile &file)
{
  static GtkWidget *box;
  ayemu_vtx_t vtx;

  if (!vtx.read_header(file))
    {
      AUDERR("Can't open file %s\n", filename);
      return;
    }
  else
    {
      StringBuf head = str_printf(_("Details about %s"), filename);
      StringBuf body = vtx.sprintname(_(
        "Title: %t\n"
        "Author: %a\n"
        "From: %f\n"
        "Tracker: %T\n"
        "Comment: %C\n"
        "Chip type: %c\n"
        "Stereo: %s\n"
        "Loop: %l\n"
        "Chip freq: %F\n"
        "Player Freq: %P\n"
        "Year: %y"));

      audgui_simple_message (& box, GTK_MESSAGE_INFO, head, body);
    }
}

#endif // USE_GTK
