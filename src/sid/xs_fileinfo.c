/*
   XMMS-SID - SIDPlay input plugin for X MultiMedia System (XMMS)

   File information window

   Programmed and designed by Matti 'ccr' Hamalainen <ccr@tnsp.org>
   (C) Copyright 1999-2007 Tecnic Software productions (TNSP)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "xs_fileinfo.h"
#include "xs_player.h"
#include "xs_support.h"
#include "xs_stil.h"
#include "xs_config.h"
#include "xs_interface.h"
#include "xs_glade.h"


static t_xs_stildb *xs_stildb_db = NULL;
XS_MUTEX(xs_stildb_db);

static GtkWidget *xs_fileinfowin = NULL;
static t_xs_stil_node *xs_fileinfostil = NULL;
XS_MUTEX(xs_fileinfowin);

#define LUW(x)	lookup_widget(xs_fileinfowin, x)


/* STIL-database handling
 */
gint xs_stil_init(void)
{
	XS_MUTEX_LOCK(xs_cfg);

	if (!xs_cfg.stilDBPath) {
		XS_MUTEX_UNLOCK(xs_cfg);
		return -1;
	}

	XS_MUTEX_LOCK(xs_stildb_db);

	/* Check if already initialized */
	if (xs_stildb_db)
		xs_stildb_free(xs_stildb_db);

	/* Allocate database */
	xs_stildb_db = (t_xs_stildb *) g_malloc0(sizeof(t_xs_stildb));
	if (!xs_stildb_db) {
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_stildb_db);
		return -2;
	}

	/* Read the database */
	if (xs_stildb_read(xs_stildb_db, xs_cfg.stilDBPath) != 0) {
		xs_stildb_free(xs_stildb_db);
		xs_stildb_db = NULL;
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_stildb_db);
		return -3;
	}

	/* Create index */
	if (xs_stildb_index(xs_stildb_db) != 0) {
		xs_stildb_free(xs_stildb_db);
		xs_stildb_db = NULL;
		XS_MUTEX_UNLOCK(xs_cfg);
		XS_MUTEX_UNLOCK(xs_stildb_db);
		return -4;
	}

	XS_MUTEX_UNLOCK(xs_cfg);
	XS_MUTEX_UNLOCK(xs_stildb_db);
	return 0;
}


void xs_stil_close(void)
{
	XS_MUTEX_LOCK(xs_stildb_db);
	xs_stildb_free(xs_stildb_db);
	xs_stildb_db = NULL;
	XS_MUTEX_UNLOCK(xs_stildb_db);
}


t_xs_stil_node *xs_stil_get(gchar *pcFilename)
{
	t_xs_stil_node *pResult;
	gchar *tmpFilename;

	XS_MUTEX_LOCK(xs_stildb_db);
	XS_MUTEX_LOCK(xs_cfg);

	if (xs_cfg.stilDBEnable && xs_stildb_db) {
		if (xs_cfg.hvscPath) {
			/* Remove postfixed directory separator from HVSC-path */
			tmpFilename = xs_strrchr(xs_cfg.hvscPath, '/');
			if (tmpFilename && (tmpFilename[1] == 0))
				tmpFilename[0] = 0;

			/* Remove HVSC location-prefix from filename */
			tmpFilename = strstr(pcFilename, xs_cfg.hvscPath);
			if (tmpFilename)
				tmpFilename += strlen(xs_cfg.hvscPath);
			else
				tmpFilename = pcFilename;
		} else
			tmpFilename = pcFilename;

XSDEBUG("xs_stil_get('%s') = '%s'\n", pcFilename, tmpFilename);
		
		pResult = xs_stildb_get_node(xs_stildb_db, tmpFilename);
	} else
		pResult = NULL;

	XS_MUTEX_UNLOCK(xs_stildb_db);
	XS_MUTEX_UNLOCK(xs_cfg);

	return pResult;
}


void xs_fileinfo_update(void)
{
	XS_MUTEX_LOCK(xs_status);
	XS_MUTEX_LOCK(xs_fileinfowin);

	/* Check if control window exists, we are currently playing and have a tune */
	if (xs_fileinfowin) {
		gboolean isEnabled;
		GtkAdjustment *tmpAdj;

		if (xs_status.tuneInfo && xs_status.isPlaying && (xs_status.tuneInfo->nsubTunes > 1)) {
			tmpAdj = gtk_range_get_adjustment(GTK_RANGE(LUW("fileinfo_subctrl_adj")));

			tmpAdj->value = xs_status.currSong;
			tmpAdj->lower = 1;
			tmpAdj->upper = xs_status.tuneInfo->nsubTunes;
			XS_MUTEX_UNLOCK(xs_status);
			XS_MUTEX_UNLOCK(xs_fileinfowin);
			gtk_adjustment_value_changed(tmpAdj);
			XS_MUTEX_LOCK(xs_status);
			XS_MUTEX_LOCK(xs_fileinfowin);
			isEnabled = TRUE;
		} else
			isEnabled = FALSE;

		/* Enable or disable subtune-control in fileinfo window */
		gtk_widget_set_sensitive(LUW("fileinfo_subctrl_prev"), isEnabled);
		gtk_widget_set_sensitive(LUW("fileinfo_subctrl_adj"), isEnabled);
		gtk_widget_set_sensitive(LUW("fileinfo_subctrl_next"), isEnabled);
	}

	XS_MUTEX_UNLOCK(xs_status);
	XS_MUTEX_UNLOCK(xs_fileinfowin);
}


static void xs_fileinfo_setsong(void)
{
	gint n;

	XS_MUTEX_LOCK(xs_status);
	XS_MUTEX_LOCK(xs_fileinfowin);

	if (xs_status.tuneInfo && xs_status.isPlaying) {
		n = (gint) gtk_range_get_adjustment(GTK_RANGE(LUW("fileinfo_subctrl_adj")))->value;
		if ((n >= 1) && (n <= xs_status.tuneInfo->nsubTunes))
			xs_status.currSong = n;
	}

	XS_MUTEX_UNLOCK(xs_fileinfowin);
	XS_MUTEX_UNLOCK(xs_status);
}


void xs_fileinfo_ok(void)
{
	XS_MUTEX_LOCK(xs_fileinfowin);
	if (xs_fileinfowin) {
		gtk_widget_destroy(xs_fileinfowin);
		xs_fileinfowin = NULL;
	}
	XS_MUTEX_UNLOCK(xs_fileinfowin);
}


gboolean xs_fileinfo_delete(GtkWidget * widget, GdkEvent * event, gpointer user_data)
{
	(void) widget;
	(void) event;
	(void) user_data;

	XSDEBUG("delete_event\n");
	xs_fileinfo_ok();
	return FALSE;
}


static void xs_fileinfo_subtune(GtkWidget * widget, void *data)
{
	t_xs_stil_subnode *tmpNode;
	GtkWidget *tmpText;
	gint tmpIndex;
	gchar *subName, *subAuthor, *subInfo;

	(void) widget;
	(void) data;

	/* Freeze text-widget and delete the old text */
	tmpText = LUW("fileinfo_sub_info");

	/* Get subtune information */
	if (widget)
		tmpIndex = gtk_option_menu_get_history(GTK_OPTION_MENU(widget));
	else
		tmpIndex = 0;
	
	if (xs_fileinfostil && tmpIndex <= xs_fileinfostil->nsubTunes)
		tmpNode = xs_fileinfostil->subTunes[tmpIndex];
	else
		tmpNode = NULL;
	
	if (tmpNode) {
		subName = tmpNode->pName;
		subAuthor = tmpNode->pAuthor;
		subInfo = tmpNode->pInfo;
	} else {
		subName = NULL;
		subAuthor = NULL;
		subInfo = NULL;
	}

	/* Get and set subtune information */
	gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_sub_name")), subName ? subName : "");
	gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_sub_author")), subAuthor ? subAuthor : "");
	gtk_text_buffer_set_text(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tmpText))),
		subInfo ? subInfo : "", -1);
}


void xs_fileinfo(gchar * pcFilename)
{
	GtkWidget *tmpMenuItem, *tmpMenu, *tmpOptionMenu;
	t_xs_tuneinfo *tmpInfo;
	gchar tmpStr[256], *tmpStr2;
	gint n;

	/* Current implementation leaves old fileinfo window untouched if
	 * no information can be found for the new file. Hmm...
	 */

	/* Get new tune information */
	XS_MUTEX_LOCK(xs_fileinfowin);
	XS_MUTEX_LOCK(xs_status);
	if ((tmpInfo = xs_status.sidPlayer->plrGetSIDInfo(pcFilename)) == NULL) {
		XS_MUTEX_UNLOCK(xs_fileinfowin);
		XS_MUTEX_UNLOCK(xs_status);
		return;
	}
	XS_MUTEX_UNLOCK(xs_status);

	xs_fileinfostil = xs_stil_get(pcFilename);

	/* Check if there already is an open fileinfo window */
	if (xs_fileinfowin)
		gdk_window_raise(xs_fileinfowin->window);
	else {
		xs_fileinfowin = create_xs_fileinfowin();
		g_signal_connect(G_OBJECT(
			gtk_range_get_adjustment(GTK_RANGE(LUW("fileinfo_subctrl_adj")))), "value_changed",
			G_CALLBACK(xs_fileinfo_setsong), NULL);
	}

	/* Delete current items */
	tmpOptionMenu = LUW("fileinfo_sub_tune");
	tmpMenu = gtk_option_menu_get_menu(GTK_OPTION_MENU(tmpOptionMenu));
	gtk_widget_destroy(tmpMenu);
	gtk_option_menu_remove_menu(GTK_OPTION_MENU(tmpOptionMenu));
	tmpMenu = gtk_menu_new();


	/* Set the generic song information */
	tmpStr2 = g_filename_to_utf8(pcFilename, -1, NULL, NULL, NULL);
	gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_filename")), tmpStr2);
	g_free(tmpStr2);
	
	gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_songname")), tmpInfo->sidName);
	gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_composer")), tmpInfo->sidComposer);
	gtk_entry_set_text(GTK_ENTRY(LUW("fileinfo_copyright")), tmpInfo->sidCopyright);


	/* Main tune - the pseudo tune */
	tmpMenuItem = gtk_menu_item_new_with_label(_("General info"));
	gtk_widget_show(tmpMenuItem);
	gtk_menu_append(GTK_MENU(tmpMenu), tmpMenuItem);

	/* Other menu items */
	for (n = 1; n <= tmpInfo->nsubTunes; n++) {
		if (xs_fileinfostil && n <= xs_fileinfostil->nsubTunes && xs_fileinfostil->subTunes[n]) {
			t_xs_stil_subnode *tmpNode = xs_fileinfostil->subTunes[n];
			
			g_snprintf(tmpStr, sizeof(tmpStr), _("Tune #%i: "), n);

			if (tmpNode->pName)
				xs_pnstrcat(tmpStr, sizeof(tmpStr), tmpNode->pName);
			else if (tmpNode->pTitle)
				xs_pnstrcat(tmpStr, sizeof(tmpStr), tmpNode->pTitle);
			else if (tmpNode->pInfo)
				xs_pnstrcat(tmpStr, sizeof(tmpStr), tmpNode->pInfo);
			else
				xs_pnstrcat(tmpStr, sizeof(tmpStr), "---");
		} else {
			g_snprintf(tmpStr, sizeof(tmpStr), _("Tune #%i"), n);
		}

		tmpMenuItem = gtk_menu_item_new_with_label(tmpStr);
		gtk_widget_show(tmpMenuItem);
		gtk_menu_append(GTK_MENU(tmpMenu), tmpMenuItem);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(tmpOptionMenu), tmpMenu);
	g_signal_connect(G_OBJECT(tmpOptionMenu), "changed", G_CALLBACK(xs_fileinfo_subtune), tmpMenu);
	gtk_widget_show(tmpOptionMenu);

	/* Set the subtune information */
	xs_fileinfo_subtune(NULL, tmpMenu);

	/* Free temporary tuneinfo */
	xs_tuneinfo_free(tmpInfo);

	/* Show the window */
	gtk_widget_show(xs_fileinfowin);

	XS_MUTEX_UNLOCK(xs_fileinfowin);

	xs_fileinfo_update();
}
