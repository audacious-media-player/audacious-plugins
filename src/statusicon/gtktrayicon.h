/* gtktrayicon.h
 * Copyright (C) 2002 Anders Carlsson <andersca@gnu.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __AUD_GTK_TRAY_ICON_H__
#define __AUD_GTK_TRAY_ICON_H__

#include <gtk/gtkplug.h>

G_BEGIN_DECLS

#define AUD_GTK_TYPE_TRAY_ICON		(aud_gtk_tray_icon_get_type ())
#define AUD_GTK_TRAY_ICON(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), AUD_GTK_TYPE_TRAY_ICON, AudGtkTrayIcon))
#define AUD_GTK_TRAY_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), AUD_GTK_TYPE_TRAY_ICON, AudGtkTrayIconClass))
#define AUD_GTK_IS_TRAY_ICON(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), AUD_GTK_TYPE_TRAY_ICON))
#define AUD_GTK_IS_TRAY_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), AUD_GTK_TYPE_TRAY_ICON))
#define AUD_GTK_TRAY_ICON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), AUD_GTK_TYPE_TRAY_ICON, AudGtkTrayIconClass))
	
typedef struct _AudGtkTrayIcon	   AudGtkTrayIcon;
typedef struct _AudGtkTrayIconPrivate  AudGtkTrayIconPrivate;
typedef struct _AudGtkTrayIconClass    AudGtkTrayIconClass;

struct _AudGtkTrayIcon
{
  GtkPlug parent_instance;

  AudGtkTrayIconPrivate *priv;
};

struct _AudGtkTrayIconClass
{
  GtkPlugClass parent_class;

  void (*__gtk_reserved1);
  void (*__gtk_reserved2);
  void (*__gtk_reserved3);
  void (*__gtk_reserved4);
  void (*__gtk_reserved5);
  void (*__gtk_reserved6);
};

GType            aud_gtk_tray_icon_get_type         (void) G_GNUC_CONST;

AudGtkTrayIcon   *_aud_gtk_tray_icon_new_for_screen  (GdkScreen   *screen,
					       const gchar *name);

AudGtkTrayIcon   *_aud_gtk_tray_icon_new             (const gchar *name);

guint          _aud_gtk_tray_icon_send_message    (AudGtkTrayIcon *icon,
					       gint         timeout,
					       const gchar *message,
					       gint         len);
void           _aud_gtk_tray_icon_cancel_message  (AudGtkTrayIcon *icon,
					       guint        id);

GtkOrientation _aud_gtk_tray_icon_get_orientation (AudGtkTrayIcon *icon);
					    
G_END_DECLS

#endif /* __AUD_GTK_TRAY_ICON_H__ */
