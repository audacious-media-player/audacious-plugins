#include <audacious/i18n.h>
#include <libaudgui/libaudgui.h>
#include <libaudgui/libaudgui-gtk.h>

#include <gtk/gtk.h>

#include "vtx.h"
#include "ayemu.h"

void vtx_about (void)
{
    static GtkWidget * box = NULL;

    audgui_simple_message (& box, GTK_MESSAGE_INFO, _("About Vortex Player"),
     _("Vortex file format player by Sashnov Alexander <sashnov@ngs.ru>\n"
     "Founded on original source in_vtx.dll by Roman Sherbakov <v_soft@microfor.ru>\n"
     "\n"
     "Audacious implementation by Pavel Vymetalek <pvymetalek@seznam.cz>"));
}
