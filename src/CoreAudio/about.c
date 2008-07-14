/*  XMMS - Cross-platform multimedia player
 *  Copyright (C) 1998-2000  Peter Alm, Mikael Alm, Olle Hallnas, Thomas Nilsson and 4Front Technologies
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include "coreaudio.h"

void osx_about(void)
{
	static GtkWidget *dialog;

	if (dialog != NULL)
		return;
	
	dialog = audacious_info_dialog(
							   "About CoreAudio Plugin",
							   "Audacious CoreAudio Plugin\n\n "
							   "This program is free software; you can redistribute it and/or modify\n"
							   "it under the terms of the GNU General Public License as published by\n"
							   "the Free Software Foundation; either version 2 of the License, or\n"
							   "(at your option) any later version.\n"
							   "\n"
							   "This program is distributed in the hope that it will be useful,\n"
							   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
							   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
							   "GNU General Public License for more details.\n"
							   "\n"
							   "You should have received a copy of the GNU General Public License\n"
							   "along with this program; if not, write to the Free Software\n"
							   "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,\n"
							   "USA.","Ok", FALSE, NULL, NULL);
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
					   GTK_SIGNAL_FUNC(gtk_widget_destroyed),
					   &dialog);
}
