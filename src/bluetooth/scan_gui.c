#include "scan_gui.h"
#include "bluetooth.h"
static GtkWidget *window = NULL;
static GtkWidget *winbox;
static GtkWidget *scanbox;
static GtkWidget *progressbox;
static GtkWidget *bluetooth_img;
static GtkWidget *scan_label;
static GtkWidget *progress_bar;

gpointer progress() {

for(;;){
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar));
    printf("threadfunction\n");
    sleep(1);
    if(discover_finish == 2 ) {            
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar),1);
            return 0;
        }
    }
return 0;
}

void show_no_devices(){
    gtk_label_set_text(GTK_LABEL(scan_label),"No devices found!");
}
void destroy_scan_window(){
    gtk_widget_hide(window);
}

void show_scan()
{
    GThread *th1;
    gchar *filename;
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

        scan_label = gtk_label_new_with_mnemonic("Scanning...");
        gtk_container_add(GTK_CONTAINER(scanbox),scan_label);

        progress_bar = gtk_progress_bar_new();
        gtk_progress_bar_pulse(GTK_PROGRESS_BAR(progress_bar));
        gtk_container_add(GTK_CONTAINER(progressbox),progress_bar);
        th1 =  g_thread_create((GThreadFunc)progress, NULL,TRUE,NULL);
        /* I have to add a button for Rescan when there are
         * no devices found and not currently scanning
         */
        gtk_window_set_default_size (GTK_WINDOW (window), 50, 40);
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
