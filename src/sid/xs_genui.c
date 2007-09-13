#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "xs_genui.h"
#include "xs_interface.h"
#include "xs_glade.h"



gboolean
xs_confirmwin_delete                   (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{

  return FALSE;
}

