/*
 * Audacious Bluetooth headset suport plugin
 *
 * Copyright (c) 2008 Paula Stanciu paula.stanciu@gmail.com
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 */

#include "scan_gui.h"
#include "bluetooth.h"
#include "agent.h"
static GtkWidget *window = NULL;
static GtkWidget *winbox;
static GtkWidget *scanbox;
static GtkWidget *buttonsbox;
static GtkWidget *progressbox;
static GtkWidget *bluetooth_img;
static GtkWidget *scan_label;
static GtkWidget *progress_bar;
static GtkWidget *rescan_buttton;
static GtkWidget *close_button;
static gint usage=0;
void show_pairing_ok(void);
gpointer progress()
{

    for(;;){
        if(window){
            gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar));
        }
        sleep(1);
        if(usage == 0){
            if(discover_finish == 2 ) {            
                if(window){
                    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar),1);
                    gtk_widget_set_sensitive(rescan_buttton,TRUE);
                }
                return 0;
            }
        }else 
        {
            if(bonding_finish == 1 ) {            
                if(window){
                    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar),1);
                    show_pairing_ok();
                    gtk_widget_set_sensitive(rescan_buttton,TRUE);

                }
                return 0;
            }
        }
    }
    return 0;
}

void show_pairing_ok()
{
    if(window ){
        gtk_label_set_text(GTK_LABEL(scan_label),_("Bonding finish!"));
    }
}

void show_no_devices()
{
    if(window ){
        gtk_label_set_text(GTK_LABEL(scan_label),_("No devices found!"));
    }
}
void destroy_scan_window()
{
    gtk_widget_hide(window);
}
void close_window(void)
{
    printf("scan_gui close callback \n");
    gtk_widget_destroy (window);
    window = NULL;
}
void show_scan(gint use)
{
    GThread *th1;
    gchar *filename;
    usage = use;
    /*
     * 0 - discovery 
     * 1 - pairing 
     */
    if (!window)
    {
        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        g_signal_connect (window, "destroy",G_CALLBACK (gtk_widget_destroyed), &window);

        winbox = gtk_vbox_new(FALSE,2);
        gtk_container_set_border_width (GTK_CONTAINER (winbox), 2); 
        gtk_container_add (GTK_CONTAINER (window), winbox);

        scanbox = gtk_hbox_new(FALSE,2);
        gtk_container_set_border_width(GTK_CONTAINER(scanbox),2);
        gtk_container_add(GTK_CONTAINER(winbox),scanbox);

        progressbox = gtk_vbox_new(FALSE,2);
        gtk_container_set_border_width(GTK_CONTAINER(progressbox),4);
        gtk_container_add(GTK_CONTAINER(winbox),progressbox);
        filename =  DATA_DIR G_DIR_SEPARATOR_S "images" G_DIR_SEPARATOR_S "blue.png";
        bluetooth_img = gtk_image_new_from_file(filename);
        gtk_image_set_pixel_size(GTK_IMAGE(bluetooth_img),-1);
        gtk_container_add(GTK_CONTAINER(scanbox),bluetooth_img);
        if(usage == 0){
            scan_label = gtk_label_new_with_mnemonic(_("Scanning..."));
        }else
        {
            scan_label = gtk_label_new_with_mnemonic(_("Pairing..."));
        }

        gtk_container_add(GTK_CONTAINER(scanbox),scan_label);

        progress_bar = gtk_progress_bar_new();
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar));
        gtk_container_add(GTK_CONTAINER(progressbox),progress_bar);
        th1 =  g_thread_create((GThreadFunc)progress, NULL,TRUE,NULL);
        buttonsbox = gtk_hbox_new(FALSE,2);
        gtk_container_set_border_width(GTK_CONTAINER(buttonsbox),2);
        gtk_container_add(GTK_CONTAINER(progressbox),buttonsbox);
        /* I have to modify the rescan button with a play one 
         * and treat the case when the bounding is not ok
         */
        if(usage == 0){
            rescan_buttton = gtk_button_new_with_mnemonic(_("Rescan"));
            g_signal_connect(rescan_buttton,"clicked",G_CALLBACK (refresh_call),NULL);
            gtk_widget_set_sensitive(rescan_buttton,FALSE);
        }else{
            rescan_buttton = gtk_button_new_with_mnemonic(_("Play"));
            g_signal_connect(rescan_buttton,"clicked",G_CALLBACK (play_call),NULL);
            gtk_widget_set_sensitive(rescan_buttton,FALSE);
        }



        close_button = gtk_button_new_with_mnemonic(_("Close"));
        gtk_container_add(GTK_CONTAINER(buttonsbox),rescan_buttton);
        gtk_container_add(GTK_CONTAINER(buttonsbox),close_button);
        g_signal_connect(close_button,"clicked",G_CALLBACK (close_window),NULL);

        gtk_window_set_default_size (GTK_WINDOW (window), 60, 40);
        gtk_window_set_resizable(GTK_WINDOW(window),FALSE);
        if (!GTK_WIDGET_VISIBLE (window))
            gtk_widget_show_all (window);
        else
        {   
            gtk_widget_destroy (window);
            g_free(filename);
            window = NULL;
        }

    }

}
