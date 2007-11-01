/*  gtk.c - functions to build and display GTK windows
 *  Copyright (C) 2000-2007  Jason Jordan <shnutils@freeshell.org>
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
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * $Id: gtk.c,v 1.27 2007/03/29 00:40:40 jason Exp $
 */

#include <stdlib.h>
#include <glib.h>
#include <audacious/util.h>
#include <audacious/configdb.h>
#include "shorten.h"

#undef XMMS_SHN_LOAD_TEXTFILES
#ifdef HAVE_DIRENT_H
#  include <sys/types.h>
#  include <dirent.h>
#  ifdef HAVE_OPENDIR
#    ifdef HAVE_READDIR
#      ifdef HAVE_CLOSEDIR
#        define XMMS_SHN_LOAD_TEXTFILES 1
#      endif
#    endif
#  endif
#endif

static GtkWidget *shn_configurewin = NULL,
		 *about_box,
		 *vbox,
		 *options_vbox,
		 *miscellaneous_vbox,
		 *seektables_vbox,
		 *bbox,
		 *notebook,
		 *output_frame,
		 *output_vbox,
		 *output_label,
		 *output_error_stderr,
		 *output_error_window,
		 *output_error_devnull,
		 *misc_frame,
		 *misc_vbox,
		 *swap_bytes_toggle,
		 *verbose_toggle,
		 *textfile_toggle,
		 *textfile_extensions_entry,
		 *textfile_extensions_label,
		 *textfile_extensions_hbox,
		 *path_frame,
		 *path_vbox,
		 *path_label,
		 *path_entry,
		 *path_entry_hbox,
		 *path_browse,
		 *relative_path_label,
		 *relative_path_entry,
		 *relative_path_entry_hbox,
		 *ok,
		 *cancel,
		 *apply;

void shn_display_about(void)
{
	if (about_box != NULL)
	{
		gdk_window_raise(about_box->window);
		return;
	}

	about_box = audacious_info_dialog(
		(gchar *) "About " PACKAGE,
		(gchar *) PACKAGE " version " VERSION "\n"
			  "Copyright (C) 2000-2007 Jason Jordan <shnutils@freeshell.org>\n"
			  "Portions Copyright (C) 1992-1995 Tony Robinson\n"
			  "\n"
			  "shorten utilities pages:\n"
			  "\n"
			  "http://www.etree.org/shnutils/\n"
			  "http://shnutils.freeshell.org/",
		(gchar *) "Cool",
		FALSE, NULL, NULL);
	g_signal_connect_swapped(GTK_OBJECT(about_box), "destroy",
		gtk_widget_destroyed, &about_box);
}

void shn_configurewin_save(void)
{
	ConfigDb *cfg;
	gchar *filename;

	shn_cfg.error_output_method = ERROR_OUTPUT_DEVNULL;
	if (GTK_TOGGLE_BUTTON(output_error_stderr)->active)
		shn_cfg.error_output_method = ERROR_OUTPUT_STDERR;
	else if (GTK_TOGGLE_BUTTON(output_error_window)->active)
		shn_cfg.error_output_method = ERROR_OUTPUT_WINDOW;

	g_free(shn_cfg.seek_tables_path);
	shn_cfg.seek_tables_path = g_strdup(gtk_entry_get_text(GTK_ENTRY(path_entry)));

	g_free(shn_cfg.relative_seek_tables_path);
	shn_cfg.relative_seek_tables_path = g_strdup(gtk_entry_get_text(GTK_ENTRY(relative_path_entry)));

	shn_cfg.verbose = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(verbose_toggle));

	shn_cfg.swap_bytes = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(swap_bytes_toggle));

	shn_cfg.load_textfiles = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(textfile_toggle));

	g_free(shn_cfg.textfile_extensions);
	shn_cfg.textfile_extensions = g_strdup(gtk_entry_get_text(GTK_ENTRY(textfile_extensions_entry)));

	filename = g_strconcat(g_get_home_dir(), "/.xmms/config", NULL);
	cfg = aud_cfg_db_open();
	aud_cfg_db_set_int(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.error_output_method_config_name, shn_cfg.error_output_method);
	aud_cfg_db_set_bool(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.verbose_config_name, shn_cfg.verbose);
	aud_cfg_db_set_string(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.seek_tables_path_config_name, shn_cfg.seek_tables_path);
	aud_cfg_db_set_string(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.relative_seek_tables_path_config_name, shn_cfg.relative_seek_tables_path);
	aud_cfg_db_set_bool(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.swap_bytes_config_name, shn_cfg.swap_bytes);
	aud_cfg_db_set_bool(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.load_textfiles_config_name, shn_cfg.load_textfiles);
	aud_cfg_db_set_string(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.textfile_extensions_config_name, shn_cfg.textfile_extensions);

	aud_cfg_db_close(cfg);
	g_free(filename);
}

void shn_configurewin_apply()
{
	shn_configurewin_save();
}

void shn_configurewin_ok(void)
{
	shn_configurewin_save();
	gtk_widget_destroy(shn_configurewin);
}

void shn_display_configure(void)
{
	if (shn_configurewin != NULL)
	{
		gdk_window_raise(shn_configurewin->window);
		return;
	}

	shn_configurewin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(shn_configurewin), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &shn_configurewin);
	gtk_window_set_title(GTK_WINDOW(shn_configurewin), (gchar *)"SHN Player Configuration");
	gtk_window_set_policy(GTK_WINDOW(shn_configurewin), FALSE, FALSE, FALSE);
	gtk_container_border_width(GTK_CONTAINER(shn_configurewin), 10);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(shn_configurewin), vbox);

	notebook = gtk_notebook_new();

	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	options_vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(options_vbox), 5);

	seektables_vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(seektables_vbox), 5);

	miscellaneous_vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(miscellaneous_vbox), 5);

/* begin error output box */

	output_frame = gtk_frame_new((gchar *)" Error display options ");
	gtk_container_set_border_width(GTK_CONTAINER(output_frame), 5);

	output_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(output_vbox), 5);
	gtk_container_add(GTK_CONTAINER(output_frame), output_vbox);

	output_label = gtk_label_new((gchar *)"When an error occurs, display it to:");
	gtk_misc_set_alignment(GTK_MISC(output_label), 0, 0);
	gtk_label_set_justify(GTK_LABEL(output_label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(output_vbox), output_label, FALSE, FALSE, 0);
	gtk_widget_show(output_label);

	output_error_window = gtk_radio_button_new_with_label(NULL, (gchar *)"a window");
	if (shn_cfg.error_output_method == ERROR_OUTPUT_WINDOW)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(output_error_window), TRUE);
	gtk_box_pack_start(GTK_BOX(output_vbox), output_error_window, FALSE, FALSE, 0);
	gtk_widget_show(output_error_window);

	output_error_stderr = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(output_error_window)), (gchar *)"stderr");
	if (shn_cfg.error_output_method == ERROR_OUTPUT_STDERR)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(output_error_stderr), TRUE);
	gtk_box_pack_start(GTK_BOX(output_vbox), output_error_stderr, FALSE, FALSE, 0);
	gtk_widget_show(output_error_stderr);

	output_error_devnull = gtk_radio_button_new_with_label(gtk_radio_button_group(GTK_RADIO_BUTTON(output_error_window)), (gchar *)"/dev/null");
	if (shn_cfg.error_output_method == ERROR_OUTPUT_DEVNULL)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(output_error_devnull), TRUE);
	gtk_box_pack_start(GTK_BOX(output_vbox), output_error_devnull, FALSE, FALSE, 0);
	gtk_widget_show(output_error_devnull);

	gtk_widget_show(output_vbox);
	gtk_widget_show(output_frame);

/* end error output box */

/* begin misc box */

	misc_frame = gtk_frame_new((gchar *)" Miscellaneous options ");
	gtk_container_set_border_width(GTK_CONTAINER(misc_frame), 5);

	misc_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(misc_vbox), 5);
	gtk_container_add(GTK_CONTAINER(misc_frame), misc_vbox);

	swap_bytes_toggle = gtk_check_button_new_with_label((gchar *)"Swap audio bytes (toggle if all you hear is static)");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(swap_bytes_toggle), shn_cfg.swap_bytes);
	gtk_box_pack_start(GTK_BOX(misc_vbox), swap_bytes_toggle, FALSE, FALSE, 0);
	gtk_widget_show(swap_bytes_toggle);

	verbose_toggle = gtk_check_button_new_with_label((gchar *)"Display debug info to stderr");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(verbose_toggle), shn_cfg.verbose);
	gtk_box_pack_start(GTK_BOX(misc_vbox), verbose_toggle, FALSE, FALSE, 0);
	gtk_widget_show(verbose_toggle);

	textfile_toggle = gtk_check_button_new_with_label((gchar *)"Load text files in file information box");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(textfile_toggle), shn_cfg.load_textfiles);
	gtk_box_pack_start(GTK_BOX(misc_vbox), textfile_toggle, FALSE, FALSE, 0);
	gtk_widget_show(textfile_toggle);

	textfile_extensions_hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(textfile_extensions_hbox), 5);
	gtk_box_pack_start(GTK_BOX(misc_vbox), textfile_extensions_hbox, FALSE, FALSE, 0);

	textfile_extensions_label = gtk_label_new((gchar *)"     Text file extensions:");
	gtk_misc_set_alignment(GTK_MISC(textfile_extensions_label), 0, 0);
	gtk_label_set_justify(GTK_LABEL(textfile_extensions_label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(textfile_extensions_hbox), textfile_extensions_label, FALSE, FALSE, 0);
	gtk_widget_show(textfile_extensions_label);

	textfile_extensions_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(textfile_extensions_entry), shn_cfg.textfile_extensions);
	gtk_box_pack_start(GTK_BOX(textfile_extensions_hbox), textfile_extensions_entry, TRUE, TRUE, 0);
	gtk_widget_show(textfile_extensions_entry);

	gtk_widget_show(textfile_extensions_hbox);
	gtk_widget_show(misc_vbox);
	gtk_widget_show(misc_frame);

/* end misc box */

/* begin seek table path box */

	path_frame = gtk_frame_new((gchar *)" Alternate seek table file locations ");
	gtk_container_set_border_width(GTK_CONTAINER(path_frame), 5);

	path_vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(path_vbox), 5);
	gtk_container_add(GTK_CONTAINER(path_frame), path_vbox);

	relative_path_label = gtk_label_new((gchar *)"Relative seek table path:");
	gtk_misc_set_alignment(GTK_MISC(relative_path_label), 0, 0);
	gtk_label_set_justify(GTK_LABEL(relative_path_label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(path_vbox), relative_path_label, FALSE, FALSE, 0);
	gtk_widget_show(relative_path_label);

	relative_path_entry_hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(relative_path_entry_hbox), 5);
	gtk_box_pack_start(GTK_BOX(path_vbox), relative_path_entry_hbox, TRUE, TRUE, 0);

	relative_path_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(relative_path_entry), shn_cfg.relative_seek_tables_path);
	gtk_box_pack_start(GTK_BOX(relative_path_entry_hbox), relative_path_entry, TRUE, TRUE, 0);
	gtk_widget_show(relative_path_entry);

	path_label = gtk_label_new((gchar *)"\nAbsolute seek table path:");
	gtk_misc_set_alignment(GTK_MISC(path_label), 0, 0);
	gtk_label_set_justify(GTK_LABEL(path_label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(path_vbox), path_label, FALSE, FALSE, 0);
	gtk_widget_show(path_label);

	path_entry_hbox = gtk_hbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(path_entry_hbox), 5);
	gtk_box_pack_start(GTK_BOX(path_vbox), path_entry_hbox, TRUE, TRUE, 0);

	path_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(path_entry), shn_cfg.seek_tables_path);
	gtk_box_pack_start(GTK_BOX(path_entry_hbox), path_entry, TRUE, TRUE, 0);
	gtk_widget_show(path_entry);

#if 0
	path_browse = gtk_button_new_with_label("Browse");
	gtk_signal_connect(GTK_OBJECT(path_browse), "clicked", GTK_SIGNAL_FUNC(path_browse_cb), NULL);
	gtk_box_pack_start(GTK_BOX(path_entry_hbox), path_browse, FALSE, FALSE, 0);
	gtk_widget_show(path_browse);
#endif

	gtk_widget_show(relative_path_entry_hbox);
	gtk_widget_show(path_entry_hbox);
	gtk_widget_show(path_vbox);
	gtk_widget_show(path_frame);

/* end seek table path box */

	gtk_box_pack_start(GTK_BOX(options_vbox), output_frame, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(seektables_vbox), path_frame, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(miscellaneous_vbox), misc_frame, TRUE, TRUE, 0);

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), options_vbox, gtk_label_new((gchar *)"Error Display"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), seektables_vbox, gtk_label_new((gchar *)"Seek Tables"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), miscellaneous_vbox, gtk_label_new((gchar *)"Miscellaneous"));

	bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
	gtk_box_pack_start(GTK_BOX(vbox), bbox, TRUE, TRUE, 0);

	ok = gtk_button_new_with_label((gchar *)"OK");
	gtk_signal_connect(GTK_OBJECT(ok), "clicked", GTK_SIGNAL_FUNC(shn_configurewin_ok), NULL);
	GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), ok, TRUE, TRUE, 0);
	gtk_widget_show(ok);
	gtk_widget_grab_default(ok);

	cancel = gtk_button_new_with_label((gchar *)"Cancel");
	gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(shn_configurewin));
	GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), cancel, TRUE, TRUE, 0);
	gtk_widget_show(cancel);

	apply = gtk_button_new_with_label((gchar *)"Apply");
	gtk_signal_connect_object(GTK_OBJECT(apply), "clicked", GTK_SIGNAL_FUNC(shn_configurewin_apply), NULL);
	GTK_WIDGET_SET_FLAGS(apply, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(bbox), apply, TRUE, TRUE, 0);
	gtk_widget_show(apply);

	gtk_widget_show(bbox);
	gtk_widget_show(options_vbox);
	gtk_widget_show(seektables_vbox);
	gtk_widget_show(miscellaneous_vbox);
	gtk_widget_show(notebook);
	gtk_widget_show(vbox);
	gtk_widget_show(shn_configurewin);
}

void shn_message_box(char *message)
{
	GtkWidget *mbox_win,
		  *mbox_vbox1,
		  *mbox_vbox2,
		  *mbox_frame,
		  *mbox_label,
		  *mbox_bbox,
		  *mbox_ok;

	mbox_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(mbox_win), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &mbox_win);
	gtk_window_set_title(GTK_WINDOW(mbox_win), (gchar *)"Shorten file information");
	gtk_window_set_policy(GTK_WINDOW(mbox_win), FALSE, FALSE, FALSE);
	gtk_container_border_width(GTK_CONTAINER(mbox_win), 10);

	mbox_vbox1 = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(mbox_win), mbox_vbox1);

	mbox_frame = gtk_frame_new((gchar *)" " PACKAGE " error ");
	gtk_container_set_border_width(GTK_CONTAINER(mbox_frame), 5);
	gtk_box_pack_start(GTK_BOX(mbox_vbox1), mbox_frame, FALSE, FALSE, 0);

	mbox_vbox2 = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(mbox_vbox2), 5);
	gtk_container_add(GTK_CONTAINER(mbox_frame), mbox_vbox2);

	mbox_label = gtk_label_new((gchar *)message);
	gtk_misc_set_alignment(GTK_MISC(mbox_label), 0, 0);
	gtk_label_set_justify(GTK_LABEL(mbox_label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(mbox_vbox2), mbox_label, TRUE, TRUE, 0);
	gtk_widget_show(mbox_label);

	mbox_bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(mbox_bbox), GTK_BUTTONBOX_SPREAD);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(mbox_bbox), 5);
	gtk_box_pack_start(GTK_BOX(mbox_vbox2), mbox_bbox, FALSE, FALSE, 0);

	mbox_ok = gtk_button_new_with_label((gchar *)"OK");
	gtk_signal_connect_object(GTK_OBJECT(mbox_ok), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(mbox_win));
	GTK_WIDGET_SET_FLAGS(mbox_ok, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(mbox_bbox), mbox_ok, TRUE, TRUE, 0);
	gtk_widget_show(mbox_ok);
	gtk_widget_grab_default(mbox_ok);

	gtk_widget_show(mbox_bbox);
	gtk_widget_show(mbox_vbox2);
	gtk_widget_show(mbox_frame);
	gtk_widget_show(mbox_vbox1);
	gtk_widget_show(mbox_win);
}

void load_shntextfile(char *filename,int num,GtkWidget *shntxt_notebook)
{
	FILE *f;
	char buffer[1024],*shortfilename;
	int nchars;
	GtkWidget *shntxt_frame,
		  *shntxt_vbox,
		  *shntxt_vbox_inner,
//		  *shntxt_text,
//		  *shntxt_table,
//		  *shntxt_vscrollbar,
		  *shntxt_filename_hbox,
		  *shntxt_filename_entry,
		  *shntxt_filename_label;

	shn_debug("Loading text file '%s'",filename);

	if ((shortfilename = strrchr(filename,'/')))
		shortfilename++;
	else
		shortfilename = filename;

	shntxt_vbox = gtk_vbox_new(FALSE, 10);

	shn_snprintf(buffer,1024," %s ",shortfilename);

	shntxt_frame = gtk_frame_new((gchar *)buffer);
	gtk_container_set_border_width(GTK_CONTAINER(shntxt_frame), 10);
	gtk_box_pack_start(GTK_BOX(shntxt_vbox), shntxt_frame, TRUE, TRUE, 0);

	shntxt_vbox_inner = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(shntxt_vbox_inner), 5);
	gtk_container_add(GTK_CONTAINER(shntxt_frame), shntxt_vbox_inner);

/* begin filename */
	shntxt_filename_hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(shntxt_vbox_inner), shntxt_filename_hbox, FALSE, TRUE, 0);

	shntxt_filename_label = gtk_label_new((gchar *)"Filename:");
	gtk_box_pack_start(GTK_BOX(shntxt_filename_hbox), shntxt_filename_label, FALSE, TRUE, 0);
	shntxt_filename_entry = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(shntxt_filename_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(shntxt_filename_hbox), shntxt_filename_entry, TRUE, TRUE, 0);

	gtk_entry_set_text(GTK_ENTRY(shntxt_filename_entry), filename);
	gtk_editable_set_position(GTK_EDITABLE(shntxt_filename_entry), -1);
/* end filename */

#if 0
	shntxt_text = gtk_text_new(NULL,NULL);

	shntxt_table = gtk_table_new(2,2,FALSE);
	gtk_container_add(GTK_CONTAINER(shntxt_vbox_inner), shntxt_table);

	shntxt_vscrollbar = gtk_vscrollbar_new(GTK_TEXT(shntxt_text)->vadj);

	gtk_text_set_editable(GTK_TEXT(shntxt_text),FALSE);
	gtk_text_set_word_wrap(GTK_TEXT(shntxt_text),TRUE);

	gtk_table_attach(GTK_TABLE(shntxt_table),shntxt_text, 0, 1, 0, 1, GTK_EXPAND | GTK_SHRINK | GTK_FILL, GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(shntxt_table),shntxt_vscrollbar, 1, 2, 0, 1, GTK_FILL, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0);

	gtk_widget_show(shntxt_vscrollbar);
	gtk_widget_show(shntxt_text);
	gtk_widget_show(shntxt_table);
#endif
	gtk_widget_show(shntxt_frame);
	gtk_widget_show(shntxt_vbox);
	gtk_widget_show(shntxt_vbox_inner);
	gtk_widget_show(shntxt_filename_hbox);
	gtk_widget_show(shntxt_filename_entry);
	gtk_widget_show(shntxt_filename_label);

	if ((f = fopen(filename,"rb")))
	{
		nchars = fread(buffer,1,1024,f);
		while (nchars > 0)
		{
//			gtk_text_insert(GTK_TEXT(shntxt_text),NULL,NULL,NULL,buffer,nchars);
			nchars = fread(buffer,1,1024,f);
		}
		fclose(f);
	}
	else
	{
		shn_snprintf(buffer,1024,"error loading file: '%s'",filename);
//		gtk_text_insert(GTK_TEXT(shntxt_text),NULL,NULL,NULL,buffer,strlen(buffer));
	}

	shn_snprintf(buffer,1024,"Text file %d",num);

	gtk_notebook_append_page(GTK_NOTEBOOK(shntxt_notebook), shntxt_vbox, gtk_label_new((gchar *)buffer));
}

void scan_for_textfiles(GtkWidget *this_notebook,char *dirname,int *filenum)
{
	char *ext,*textfile_exts,buffer[2048];
	gchar *exts;
	DIR *dp;
	struct dirent *entry;

	shn_debug("Searching for text files in directory '%s'",dirname);

	if (NULL == (dp = opendir(dirname)))
	{
		shn_debug("Could not open directory '%s'",dirname);
		return;
	}

	while ((entry = readdir(dp)))
	{
		exts = g_strdup(shn_cfg.textfile_extensions);

		if ((ext = strrchr(entry->d_name,'.')))
			ext++;
		else
			ext = "";

		textfile_exts = strtok(exts,",");
		while (textfile_exts)
		{
			if ((0 == strcmp(textfile_exts,ext)) || (0 == strcmp(textfile_exts,"")))
			{
				shn_snprintf(buffer,2048,"%s/%s",dirname,entry->d_name);

				load_shntextfile(buffer,*filenum,this_notebook);
				*filenum = *filenum + 1;
				break;
			}

			textfile_exts = strtok(NULL,",");
		}
		
		g_free(exts);
	}

	closedir(dp);
}

void load_shntextfiles(GtkWidget *this_notebook,char *filename)
{
#ifdef XMMS_SHN_LOAD_TEXTFILES
	char *basedir,*uponedir;
	int filenum = 1;

	basedir = shn_get_base_directory(filename);

	if (NULL == (uponedir = malloc((strlen(basedir) + 5) * sizeof(char))))
	{
		shn_debug("Could not allocate memory for search directories");
		return;
	}

	shn_snprintf(uponedir,strlen(basedir) + 4,"%s/..",basedir);

	scan_for_textfiles(this_notebook,basedir,&filenum);
	scan_for_textfiles(this_notebook,uponedir,&filenum);

	free(basedir);
	free(uponedir);
#else
	shn_debug("Text file support is disabled on your platform because the\n"
	      "appropriate functions were not found.  Please email me with\n"
	      "the specifics of your platform, and I will try to support it.");
#endif
}


void shn_display_info(shn_file *tmp_file)
{
	char props[BUF_SIZE];
	char details[BUF_SIZE];
	char props_label[BUF_SIZE];
	char details_label[BUF_SIZE];
	char misalignment[8];
	char seektable_revision[8];
	char id3v2_info[32];
	GtkWidget *info_win,
		  *info_notebook,
		  *props_hbox,
		  *props_vbox,
		  *props_vbox_inner,
		  *props_frame,
		  *props_label_left,
		  *props_label_right,
		  *props_filename_hbox,
		  *props_filename_label,
		  *props_filename_entry,
		  *details_hbox,
		  *details_vbox,
		  *details_vbox_inner,
		  *details_frame,
		  *details_label_left,
		  *details_label_right,
		  *details_filename_hbox,
		  *details_filename_label,
		  *details_filename_entry,
		  *main_vbox,
		  *info_bbox,
		  *info_ok;

	shn_snprintf(props_label,BUF_SIZE," Properties for %s ",
		strrchr(tmp_file->wave_header.filename,'/') ?
		strrchr(tmp_file->wave_header.filename,'/') + 1 :
		tmp_file->wave_header.filename);

	shn_snprintf(details_label,BUF_SIZE," Details for %s ",
		strrchr(tmp_file->wave_header.filename,'/') ?
		strrchr(tmp_file->wave_header.filename,'/') + 1 :
		tmp_file->wave_header.filename);

	shn_snprintf(misalignment,8,"%d",tmp_file->wave_header.data_size % CD_BLOCK_SIZE);

	if (NO_SEEK_TABLE != tmp_file->seek_header.version)
		shn_snprintf(seektable_revision,8,"%d",tmp_file->seek_header.version);

	shn_snprintf(id3v2_info,32,"yes (%ld bytes)",tmp_file->wave_header.id3v2_tag_size);

	shn_snprintf(props,BUF_SIZE,
		"%s\n"
		"%s\n"
		"%s\n"
		"%0.4f\n"
		"\n"
		"%s\n"
		"%s\n"
		"%s bytes\n"
		"%s\n"
		"\n"
		"%s\n"
		"%s\n"
		"\n"
		"%s",
		tmp_file->wave_header.m_ss,
		(tmp_file->vars.seek_table_entries == NO_SEEK_TABLE)?"no":"yes",
		(tmp_file->seek_header.version == NO_SEEK_TABLE)?"n/a":seektable_revision,
		(double)tmp_file->wave_header.actual_size/(double)tmp_file->wave_header.total_size,
		(PROB_NOT_CD(tmp_file->wave_header))?"no":"yes",
		(PROB_NOT_CD(tmp_file->wave_header))?"n/a":((PROB_BAD_BOUND(tmp_file->wave_header))?"no":"yes"),
		(PROB_NOT_CD(tmp_file->wave_header))?"n/a":misalignment,
		(PROB_NOT_CD(tmp_file->wave_header))?"n/a":((PROB_TOO_SHORT(tmp_file->wave_header))?"no":"yes"),
		(PROB_HDR_NOT_CANONICAL(tmp_file->wave_header))?"yes":"no",
		(PROB_EXTRA_CHUNKS(tmp_file->wave_header))?"yes":"no",
		(tmp_file->wave_header.file_has_id3v2_tag)?id3v2_info:"no"
		);

	shn_snprintf(details,BUF_SIZE,
		"\n"
		"0x%04x (%s)\n"
		"%hu\n"
		"%hu\n"
		"%lu\n"
		"%lu\n"
		"%lu bytes/sec\n"
		"%hu\n"
		"%d bytes\n"
		"%lu bytes\n"
		"%lu bytes\n"
		"%lu bytes\n"
		"%lu bytes",
		tmp_file->wave_header.wave_format,
		shn_format_to_str(tmp_file->wave_header.wave_format),
		tmp_file->wave_header.channels,
		tmp_file->wave_header.bits_per_sample,
		tmp_file->wave_header.samples_per_sec,
		tmp_file->wave_header.avg_bytes_per_sec,
		tmp_file->wave_header.rate,
		tmp_file->wave_header.block_align,
		tmp_file->wave_header.header_size,
		tmp_file->wave_header.data_size,
		tmp_file->wave_header.chunk_size,
		tmp_file->wave_header.total_size,
		tmp_file->wave_header.actual_size
		);

	info_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_signal_connect(GTK_OBJECT(info_win), "destroy", GTK_SIGNAL_FUNC(gtk_widget_destroyed), &info_win);
	gtk_window_set_title(GTK_WINDOW(info_win), (gchar *)"Shorten file information");
	gtk_window_set_policy(GTK_WINDOW(info_win), FALSE, FALSE, FALSE);
	gtk_container_border_width(GTK_CONTAINER(info_win), 10);

	main_vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(info_win), main_vbox);

	info_notebook = gtk_notebook_new();
	gtk_container_add(GTK_CONTAINER(main_vbox), info_notebook);

/* begin properties page */

	props_vbox = gtk_vbox_new(FALSE, 10);

	props_frame = gtk_frame_new((gchar *)props_label);
	gtk_container_set_border_width(GTK_CONTAINER(props_frame), 10);
	gtk_box_pack_start(GTK_BOX(props_vbox), props_frame, TRUE, TRUE, 0);

	props_vbox_inner = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(props_vbox_inner), 5);
	gtk_container_add(GTK_CONTAINER(props_frame), props_vbox_inner);

/* begin filename */
	props_filename_hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(props_vbox_inner), props_filename_hbox, FALSE, TRUE, 0);

	props_filename_label = gtk_label_new((gchar *)"Filename:");
	gtk_box_pack_start(GTK_BOX(props_filename_hbox), props_filename_label, FALSE, TRUE, 0);
	props_filename_entry = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(props_filename_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(props_filename_hbox), props_filename_entry, TRUE, TRUE, 0);

	gtk_entry_set_text(GTK_ENTRY(props_filename_entry), tmp_file->wave_header.filename);
	gtk_editable_set_position(GTK_EDITABLE(props_filename_entry), -1);
/* end filename */

	props_hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(props_vbox_inner), props_hbox, TRUE, TRUE, 0);

	props_label_left = gtk_label_new((gchar *)
		"Length:\n"
		"Seekable:\n"
		"Seek table revision:\n"
		"Compression ratio:\n"
		"CD-quality properties:\n"
		"  CD-quality:\n"
		"  Cut on sector boundary:\n"
		"  Sector misalignment:\n"
		"  Long enough to be burned:\n"
		"WAVE properties:\n"
		"  Non-canonical header:\n"
		"  Extra RIFF chunks:\n"
		"Possible problems:\n"
		"  File contains ID3v2 tag:"
		);

	props_label_right = gtk_label_new((gchar *)props);

	gtk_misc_set_alignment(GTK_MISC(props_label_left), 0, 0);
	gtk_label_set_justify(GTK_LABEL(props_label_left), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(props_hbox), props_label_left, TRUE, TRUE, 0);
	gtk_widget_show(props_label_left);

	gtk_misc_set_alignment(GTK_MISC(props_label_right), 0, 0);
	gtk_label_set_justify(GTK_LABEL(props_label_right), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(props_hbox), props_label_right, TRUE, TRUE, 0);
	gtk_widget_show(props_label_right);

/* end properties page */

/* begin details page */

	details_vbox = gtk_vbox_new(FALSE, 10);

	details_frame = gtk_frame_new((gchar *)details_label);
	gtk_container_set_border_width(GTK_CONTAINER(details_frame), 10);
	gtk_box_pack_start(GTK_BOX(details_vbox), details_frame, TRUE, TRUE, 0);

	details_vbox_inner = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(details_vbox_inner), 5);
	gtk_container_add(GTK_CONTAINER(details_frame), details_vbox_inner);

/* begin filename */
	details_filename_hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(details_vbox_inner), details_filename_hbox, FALSE, TRUE, 0);

	details_filename_label = gtk_label_new((gchar *)"Filename:");
	gtk_box_pack_start(GTK_BOX(details_filename_hbox), details_filename_label, FALSE, TRUE, 0);
	details_filename_entry = gtk_entry_new();
	gtk_editable_set_editable(GTK_EDITABLE(details_filename_entry), FALSE);
	gtk_box_pack_start(GTK_BOX(details_filename_hbox), details_filename_entry, TRUE, TRUE, 0);

	gtk_entry_set_text(GTK_ENTRY(details_filename_entry), tmp_file->wave_header.filename);
	gtk_editable_set_position(GTK_EDITABLE(details_filename_entry), -1);
/* end filename */

	details_hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(GTK_BOX(details_vbox_inner), details_hbox, TRUE, TRUE, 0);

	details_label_left = gtk_label_new((gchar *)
		"\n"
		"WAVE format:\n"
		"Channels:\n"
		"Bits per sample:\n"
		"Samples per second:\n"
		"Average bytes per second:\n"
		"Rate (calculated):\n"
		"Block align:\n"
		"Header size:\n"
		"WAVE data size:\n"
		"Chunk size:\n"
		"Total size (chunk size + 8):\n"
		"Actual file size:"
		);

	details_label_right = gtk_label_new((gchar *)details);

	gtk_misc_set_alignment(GTK_MISC(details_label_left), 0, 0);
	gtk_label_set_justify(GTK_LABEL(details_label_left), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(details_hbox), details_label_left, TRUE, TRUE, 0);
	gtk_widget_show(details_label_left);

	gtk_misc_set_alignment(GTK_MISC(details_label_right), 0, 0);
	gtk_label_set_justify(GTK_LABEL(details_label_right), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX(details_hbox), details_label_right, TRUE, TRUE, 0);
	gtk_widget_show(details_label_right);

/* end details page */

	info_bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(info_bbox), GTK_BUTTONBOX_SPREAD);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(info_bbox), 5);
	gtk_box_pack_start(GTK_BOX(main_vbox), info_bbox, FALSE, FALSE, 0);

	info_ok = gtk_button_new_with_label((gchar *)"OK");
	gtk_signal_connect_object(GTK_OBJECT(info_ok), "clicked", GTK_SIGNAL_FUNC(gtk_widget_destroy), GTK_OBJECT(info_win));
	GTK_WIDGET_SET_FLAGS(info_ok, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(info_bbox), info_ok, TRUE, TRUE, 0);
	gtk_widget_show(info_ok);
	gtk_widget_grab_default(info_ok);

	gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), props_vbox, gtk_label_new((gchar *)"Properties"));
	gtk_notebook_append_page(GTK_NOTEBOOK(info_notebook), details_vbox, gtk_label_new((gchar *)"Details"));

	gtk_widget_show(props_frame);
	gtk_widget_show(props_hbox);
	gtk_widget_show(props_vbox_inner);
	gtk_widget_show(props_vbox);
	gtk_widget_show(props_filename_hbox);
	gtk_widget_show(props_filename_entry);
	gtk_widget_show(props_filename_label);
	gtk_widget_show(details_frame);
	gtk_widget_show(details_hbox);
	gtk_widget_show(details_vbox_inner);
	gtk_widget_show(details_vbox);
	gtk_widget_show(details_filename_hbox);
	gtk_widget_show(details_filename_entry);
	gtk_widget_show(details_filename_label);

/* begin any text files pages */

	if (shn_cfg.load_textfiles)
		load_shntextfiles(info_notebook,tmp_file->wave_header.filename);

/* end any text files pages */

	gtk_notebook_set_page(GTK_NOTEBOOK(info_notebook), 0);

	gtk_widget_show(info_notebook);
	gtk_widget_show(main_vbox);
	gtk_widget_show(info_bbox);
	gtk_widget_show(info_win);
}
