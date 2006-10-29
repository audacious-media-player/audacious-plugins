/*
 * SAP xmms plug-in. 
 * Copyright 2002/2003 by Michal 'Mikey' Szwaczko <mikey@scene.pl>
 *
 * SAP Library ver. 1.56 by Adam Bienias
 *
 * This is free software. You can modify it and distribute it under the terms
 * of the GNU General Public License. The verbatim text of the license can 
 * be found in file named COPYING in the source directory.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR 
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <audacious/util.h>
#include <string.h>
#include <pthread.h>

#include "version.h"
#include "fileinfo.h"
#include "saplib/sapLib.h"

static void fail (gchar *error) {

	gchar *errorstring;

	    errorstring = g_strdup_printf("An error occured:\n%s", error);
	    xmms_show_message("Error!",errorstring,	"Ok", FALSE, NULL, NULL);
	    g_free(errorstring);
	    return;
}


static void write_tag (GtkWidget *w , gpointer data) { 


	gchar *author, *title, *date, *tmpfn, *original_filename;
        FILE *outf; 
	
	int ofd;
 
	/* tags taken from info_box (edited or not) */
	author = gtk_entry_get_text(GTK_ENTRY(performer_entry));
	title = gtk_entry_get_text(GTK_ENTRY(title_entry));
	date = gtk_entry_get_text(GTK_ENTRY(date_entry));
	original_filename = gtk_entry_get_text(GTK_ENTRY(filename_entry));
 
	// prepare tmp file
	tmpfn = g_strdup_printf("/tmp/sapplugXXXXXX");

	if ((ofd = mkstemp( tmpfn )) < 0 ) {
	
		fail("Can't make tempfile");
		g_free(tmpfn);
		return;
	
	}

	if ((outf = fdopen(ofd,"wb")) == NULL ) {

		g_free(tmpfn);
		fail("Cant write to file");
		close(ofd);
		return;
	
	}

	/* start writing header */ 
	
	fprintf(outf,"SAP\r\n");

	if (strlen(author) != 0) 
	
	    fprintf(outf, "AUTHOR \"%s\"\r\n",author);
	
	if (strlen(title) != 0) 
	
	    fprintf(outf,"NAME \"%s\"\r\n",title);
	
	if (strlen(date) != 0) 
	
	    fprintf(outf,"DATE \"%s\"\r\n",date);
 
	/* save original songtype */
	    
	fprintf(outf,"TYPE %c\r\n",type_h);
 
	/* save original TAGS */
	
	if (is_stereo == 1) 
		
	    fprintf(outf,"STEREO\r\n");

	if (fastplay != -1) 
	
	    fprintf(outf,"FASTPLAY %d\r\n",fastplay);    

	if (songs != -1) 
	    
	    fprintf(outf,"SONGS %d\r\n",songs);

	if (defsong != -1)  
	
	    fprintf(outf,"DEFSONG %d\r\n",defsong);

	if (ini_address != -1) 
	
	    fprintf(outf,"INIT %.4X\r\n",ini_address);

	if (msx_address != -1) 
	
	    fprintf(outf,"MUSIC %.4X\r\n",msx_address);

	if (plr_address != -1) 
	
	    fprintf(outf,"PLAYER %.4X\r\n",plr_address);
	
	/* header written - now append original SAP data */

	fwrite(&buffer+headersize, 1, filesize-headersize,outf);
	fclose(outf);
	
	/* and rename it */
	if (rename(tmpfn,original_filename) < 0) {
   
		remove(tmpfn);
		g_free(tmpfn);
		fail("Failed to write!");
		return;
	}
  
	remove(tmpfn);
	g_free(tmpfn);
	gtk_widget_destroy(window);

}

static void remove_tag (void) {

    gtk_entry_set_text(GTK_ENTRY(performer_entry),"");	
    gtk_entry_set_text(GTK_ENTRY(title_entry),"");
    gtk_entry_set_text(GTK_ENTRY(date_entry),"");

}

void sap_file_info_box(char *filename) {

	gchar *tmp;
	gchar *info_text;
	
	static GtkWidget *info_frame, *info_box, *sap_info_label;
	static GtkWidget *tag_frame;

	GtkWidget *hbox, *label, *filename_hbox, *vbox, *left_vbox;
	GtkWidget *table, *bbox, *cancel_button;
	GtkWidget *save_button, *remove_button;

	
	if (filename != NULL) {
	
	    /* dig info from the sapfile */
	    if ((load_sap(filename)) < 0) return;		

		if (!window) {

		    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		    gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
		    gtk_signal_connect(GTK_OBJECT(window), "destroy", 
				       GTK_SIGNAL_FUNC(gtk_widget_destroyed), &window);
		    gtk_container_set_border_width(GTK_CONTAINER(window), 10);

		    vbox = gtk_vbox_new(FALSE, 10);
		    gtk_container_add(GTK_CONTAINER(window), vbox);

		    filename_hbox = gtk_hbox_new(FALSE, 5);
		    gtk_box_pack_start(GTK_BOX(vbox), filename_hbox, FALSE, TRUE, 0);
		
		    label = gtk_label_new("Filename:");
		    gtk_box_pack_start(GTK_BOX(filename_hbox), label, FALSE, TRUE, 0);
		    filename_entry = gtk_entry_new();
		    gtk_editable_set_editable(GTK_EDITABLE(filename_entry), FALSE);
		    gtk_box_pack_start(GTK_BOX(filename_hbox), filename_entry,TRUE, TRUE, 0);

		    hbox = gtk_hbox_new(FALSE, 10);
		    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
		
		    left_vbox = gtk_vbox_new(FALSE, 10);
		    gtk_box_pack_start(GTK_BOX(hbox), left_vbox, FALSE, FALSE, 0);

		    tag_frame = gtk_frame_new("SAP Tag:");
		    gtk_box_pack_start(GTK_BOX(left_vbox), tag_frame, FALSE, FALSE, 0);

		    table = gtk_table_new(5, 5, FALSE);
		    gtk_container_set_border_width(GTK_CONTAINER(table), 5);
		    gtk_container_add(GTK_CONTAINER(tag_frame), table);
		
		    label = gtk_label_new("Title:");
		    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
		    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
				 GTK_FILL, GTK_FILL, 5, 5);
		
		    title_entry = gtk_entry_new();
		    gtk_table_attach(GTK_TABLE(table), title_entry, 1, 4, 0, 1,
				 GTK_FILL | GTK_EXPAND | GTK_SHRINK,
				 GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

		    label = gtk_label_new("Artist:");
		    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
		    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
				 GTK_FILL, GTK_FILL, 5, 5);
		
		    performer_entry = gtk_entry_new();
		    gtk_table_attach(GTK_TABLE(table), performer_entry, 1, 4, 1, 2,
				 GTK_FILL | GTK_EXPAND | GTK_SHRINK,
				 GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);

		    label = gtk_label_new("Date:");
		    gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
		    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
				 GTK_FILL, GTK_FILL, 5, 5);

		    date_entry = gtk_entry_new();
		    gtk_table_attach(GTK_TABLE(table), date_entry, 1, 2, 4, 5,
				 GTK_FILL | GTK_EXPAND | GTK_SHRINK,
				 GTK_FILL | GTK_EXPAND | GTK_SHRINK, 0, 5);
		

		    bbox = gtk_hbutton_box_new(); 
		    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox),
					  GTK_BUTTONBOX_END);
		    gtk_button_box_set_spacing(GTK_BUTTON_BOX(bbox), 5);
		    gtk_box_pack_start(GTK_BOX(left_vbox), bbox, FALSE, FALSE, 0);
		
		    save_button = gtk_button_new_with_label("Save");
		    gtk_signal_connect(GTK_OBJECT(save_button), "clicked", 
				   GTK_SIGNAL_FUNC(write_tag), NULL);
		    
		    GTK_WIDGET_SET_FLAGS(save_button, GTK_CAN_DEFAULT);
		    
		    gtk_box_pack_start(GTK_BOX(bbox), save_button, TRUE, TRUE, 0);
		    gtk_widget_grab_default(save_button);

		    remove_button = gtk_button_new_with_label("Remove Tag");
		    gtk_signal_connect_object(GTK_OBJECT(remove_button),
					  "clicked", 
					  GTK_SIGNAL_FUNC(remove_tag), NULL);
		
		    GTK_WIDGET_SET_FLAGS(remove_button, GTK_CAN_DEFAULT);
		    gtk_box_pack_start(GTK_BOX(bbox),remove_button, TRUE, TRUE, 0);

		    cancel_button = gtk_button_new_with_label("Cancel");
		    gtk_signal_connect_object(GTK_OBJECT(cancel_button),
					  "clicked", 
					  GTK_SIGNAL_FUNC(gtk_widget_destroy),
					  GTK_OBJECT(window));
		    GTK_WIDGET_SET_FLAGS(cancel_button, GTK_CAN_DEFAULT);
		    gtk_box_pack_start(GTK_BOX(bbox),cancel_button, TRUE, TRUE, 0);

		    /* info frame */
		    info_frame = gtk_frame_new("SAP Info:");
		    gtk_box_pack_start(GTK_BOX(hbox), info_frame, FALSE, FALSE, 0);

		    info_box = gtk_vbox_new(FALSE, 5);
		    gtk_container_add(GTK_CONTAINER(info_frame), info_box);
		    gtk_container_set_border_width(GTK_CONTAINER(info_box), 5);
		    gtk_box_set_spacing(GTK_BOX(info_box), 0);

		    sap_info_label = gtk_label_new("");
		    gtk_misc_set_alignment(GTK_MISC(sap_info_label), 0, 0);
		    gtk_label_set_justify(GTK_LABEL(sap_info_label),
				      GTK_JUSTIFY_LEFT);
		    gtk_box_pack_start(GTK_BOX(info_box), sap_info_label, FALSE,
				   FALSE, 0);
		    /* end info frame */
	
		    gtk_widget_show_all(window);
		    
		} else
		    
		    gdk_window_raise(window->window);
		    gtk_widget_set_sensitive(tag_frame, TRUE);	
	
		    /* window drawn and displayed ... now - the code 

    		    clear up entries */
		    remove_tag();

		    /* show filename in the entrybox */
		    gtk_entry_set_text(GTK_ENTRY(filename_entry),filename);
		    /* and in the window title ... */
		    tmp = g_strdup_printf("File Info - [%s]", g_basename(filename));
		    gtk_window_set_title(GTK_WINDOW(window), tmp);

		    /* fill up entries with what we dug out from the sapfile */
		    gtk_entry_set_text(GTK_ENTRY(performer_entry),author);
		    gtk_entry_set_position(GTK_ENTRY(performer_entry),0);	
		    gtk_entry_set_text(GTK_ENTRY(title_entry),name);
		    gtk_entry_set_position(GTK_ENTRY(title_entry),0);	
		    gtk_entry_set_text(GTK_ENTRY(date_entry),date);
		    gtk_entry_set_position(GTK_ENTRY(date_entry),0);
		    /* fill in sap_info_frame; */
		    info_text = g_strdup_printf(
				"Type: %s\nStereo: %s\nSpeed: %d / Frame\nSongs: %d\n\nSize: %ld\n",
				
				    type,
				    (is_stereo > 0) ? "Yes" : "No",
				    times_per_frame,
				    (songs > 0) ? songs : 1,
				    filesize );

		    gtk_label_set_text(GTK_LABEL(sap_info_label),info_text);

		    /* cleanup */
		    g_free(info_text);
    		    g_free(tmp);

        } 

}
