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

#include "gui.h"
#include "bluetooth.h"

static GtkWidget *window = NULL;
static GtkTreeModel *model;
static GtkWidget *mainbox;
static GtkWidget *hbox_top;
static GtkWidget *hbox_bottom;
static GtkWidget *box_about;
static GtkWidget *box_about_left;
static GtkWidget *box_about_right;
static GtkWidget *headset_frame;
static GtkWidget *about_frame;
static GtkWidget *refresh;
static GtkWidget *connect_button;
static GtkWidget *close_button;   
static GtkWidget *treeview;
static GtkWidget *label_p;
static GtkWidget *label_c;
static GtkWidget *label_a;
static GtkWidget *label_prod;
static GtkWidget *label_class;
static GtkWidget *label_address;
static GList * dev = NULL;
gchar *status = NULL;
enum{
    COLUMN_PRODUCER,
    NUM_COLUMNS
};

static GtkTreeModel * create_model(void)
{
    GtkListStore *store;
    GtkTreeIter iter;
    /* create list store */
    store = gtk_list_store_new(NUM_COLUMNS,
            G_TYPE_STRING);
    dev = audio_devices;
    if(dev == NULL) {
        /*if we are scanning for devices now then print the Scanning message,
         * else we print the "no devices found message */
        if(discover_finish == 1) 
            /*we are scanning*/
            status = g_strdup_printf("Scanning"); 
        else 
            status = g_strdup_printf("No devices found!");
        /* add the status to the list */
        gtk_list_store_append(store,&iter);
        gtk_list_store_set(store,&iter, COLUMN_PRODUCER,status,-1);
        return GTK_TREE_MODEL(store);    
    } 
    while(dev != NULL)
    {
        gtk_list_store_append(store,&iter);
        gtk_list_store_set(store,&iter, COLUMN_PRODUCER, 
                ((DeviceData*)(dev->data))-> name,-1);
        dev = g_list_next(dev);
    }

    return GTK_TREE_MODEL(store);                       
}
static GtkTreeModel * rebuild_model(void)
{

    GtkListStore *store;
    GtkTreeIter iter;
    gint dev_no=0;
    GList *dev;
      if(!window) 
        return NULL;
    /* create list store */
    store = gtk_list_store_new(NUM_COLUMNS,
            G_TYPE_STRING);

    /*add inf to test_data from audio_devices */
    dev_no = g_list_length(audio_devices);
    dev = audio_devices;
    if(dev == NULL || discover_finish == 0) {
        /*if we are scanning for devices now then print the Scanning message,
         * else we print the "no devices found message */
        printf("discover: %d\n",discover_finish);
        if(discover_finish == 1) {
            /*we are scanning*/
            status = g_strdup_printf("Scanning"); 
        } else 
            status = g_strdup_printf("No devices found!");
        /* add the status to the list */
        gtk_list_store_append(store,&iter);
        gtk_list_store_set(store,&iter, COLUMN_PRODUCER,status,-1);
        gtk_label_set_text(GTK_LABEL(label_prod),status);
        return GTK_TREE_MODEL(store);    
    } 

    /* add data to the list store */
    while(dev != NULL)
    {
        gtk_list_store_append(store,&iter);
        gtk_list_store_set(store,&iter, COLUMN_PRODUCER, 
                ((DeviceData*)(dev->data))-> name,-1);
        dev = g_list_next(dev);
    }
    /*set the labels */
    /*   temp = g_strdup_printf("0x%x",((DeviceData*)(dev->data))->class); */
    gtk_label_set_text(GTK_LABEL(label_prod),((DeviceData*)(dev->data))->name);
    /*    gtk_label_set_text(GTK_LABEL(label_class),temp); */
    gtk_label_set_text(GTK_LABEL(label_address),((DeviceData*)(dev->data))->address);
   /* g_free(temp); */
    return GTK_TREE_MODEL(store);          

}


void refresh_tree()
{
    if(!window) 
        return;
    model = rebuild_model();
    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview),GTK_TREE_MODEL(model));
}


static void add_columns(GtkTreeView *treeview)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    // GtkTreeModel *model = gtk_tree_view_get_model (treeview);

    /* column for producer */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes (_("Producer"),
            renderer,
            "text",
            COLUMN_PRODUCER,
            NULL);
    gtk_tree_view_column_set_sort_column_id (column,COLUMN_PRODUCER);
    gtk_tree_view_append_column (treeview, column);

}

void close_call(void){
    printf("close callback \n");
    gtk_widget_destroy (window);
    window = NULL;
}
void select_row(GtkWidget *treeview){

    GtkTreeIter iter;
    gint sel;
    gchar *temp;
    gint i;
    printf("select\n");
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(treeview)); 
    if(gtk_tree_selection_get_selected (selection, NULL,&iter)){
        GtkTreePath *path;
        path = gtk_tree_model_get_path (model, &iter);
        sel = gtk_tree_path_get_indices (path)[0];
        printf("i=%d\n",sel);
        selected_dev = audio_devices;
        for(i=0;i<sel;i++) 
           selected_dev = g_list_next(dev);
        if(dev != NULL) {
            temp = g_strdup_printf("0x%x",((DeviceData*)(selected_dev->data))->class);
            gtk_label_set_text(GTK_LABEL(label_prod),((DeviceData*)(selected_dev->data))->name);
            gtk_label_set_text(GTK_LABEL(label_class),temp);
            gtk_label_set_text(GTK_LABEL(label_address),((DeviceData*)(selected_dev->data))->address);
            gtk_tree_path_free (path);
            g_free(temp);
        }else 
            gtk_label_set_text(GTK_LABEL(label_prod),status);
        g_free(status);
        gtk_widget_set_sensitive(connect_button,TRUE);
    }
}

void refresh_resultsui(){
    gtk_widget_destroy (window);
    window = NULL;
selected_dev = NULL;
    refresh_call();
}


void results_ui()
{
    gchar *temp;
    if (!window)
    {
        window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
        g_signal_connect (window, "destroy",G_CALLBACK (gtk_widget_destroyed), &window);

        mainbox = gtk_vbox_new(FALSE,4);
        gtk_container_set_border_width (GTK_CONTAINER (mainbox), 4); 
        gtk_container_add (GTK_CONTAINER (window), mainbox);

        hbox_top = gtk_hbox_new(FALSE,4);
        gtk_container_set_border_width (GTK_CONTAINER(hbox_top), 4); 
        gtk_container_add (GTK_CONTAINER (mainbox), hbox_top);

        hbox_bottom = gtk_hbox_new(FALSE,4);
        gtk_container_set_border_width (GTK_CONTAINER (hbox_bottom), 4); 
        gtk_container_add (GTK_CONTAINER (mainbox), hbox_bottom);

        headset_frame = gtk_frame_new(_("Available Headsets"));
        gtk_container_add (GTK_CONTAINER (hbox_top), headset_frame);

        about_frame = gtk_frame_new(_("Current Headset"));
        gtk_container_add(GTK_CONTAINER(hbox_top),about_frame);

        refresh = gtk_button_new_with_mnemonic (_("_Refresh"));
        g_signal_connect (refresh, "clicked",G_CALLBACK (refresh_resultsui), NULL);
        gtk_container_add(GTK_CONTAINER(hbox_bottom),refresh);

        connect_button = gtk_button_new_with_mnemonic(_("_Connect"));
        g_signal_connect(connect_button,"clicked",G_CALLBACK (connect_call), NULL);
        gtk_container_add(GTK_CONTAINER(hbox_bottom),connect_button);
        gtk_widget_set_sensitive(connect_button,FALSE);


        close_button = gtk_button_new_with_mnemonic(_("_Close"));
        g_signal_connect(close_button,"clicked",G_CALLBACK (close_call),NULL);
        gtk_container_add(GTK_CONTAINER(hbox_bottom),close_button);
        /* create tree model */
        model = create_model ();

        /* create tree view */
        treeview = gtk_tree_view_new_with_model (model);
        gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (treeview), TRUE);
        gtk_tree_selection_set_mode (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview)),GTK_SELECTION_SINGLE);
        g_object_unref (model);
        gtk_container_add (GTK_CONTAINER (headset_frame), treeview);
        /* add columns to the tree view */
        add_columns (GTK_TREE_VIEW (treeview));

        g_signal_connect(treeview,"cursor-changed",G_CALLBACK(select_row),treeview);


        box_about = gtk_hbox_new(FALSE,4);
        gtk_container_set_border_width (GTK_CONTAINER (box_about), 4); 
        gtk_container_add (GTK_CONTAINER (about_frame), box_about);

        /*about box left - vbox */

        box_about_left = gtk_vbox_new(FALSE,4);
        gtk_container_set_border_width (GTK_CONTAINER (box_about_left), 4); 
        gtk_container_add (GTK_CONTAINER (box_about), box_about_left);

        /*about box right - vbox */
        box_about_right = gtk_vbox_new(TRUE,4);
        gtk_container_set_border_width (GTK_CONTAINER (box_about_right), 4); 
        gtk_container_add (GTK_CONTAINER (box_about), box_about_right);

        /* Left labels  */
        label_p = gtk_label_new(_("Name:"));
        gtk_container_add(GTK_CONTAINER(box_about_left),label_p);

        label_c = gtk_label_new(_("Class"));
        gtk_container_add(GTK_CONTAINER(box_about_left),label_c);


        label_a = gtk_label_new(_("Address:"));
        gtk_container_add(GTK_CONTAINER(box_about_left),label_a);


        /*right labels */
        label_prod = gtk_label_new("  ");
        gtk_container_add(GTK_CONTAINER(box_about_right),label_prod);

        label_class = gtk_label_new("  ");
        gtk_container_add(GTK_CONTAINER(box_about_right),label_class);


        label_address = gtk_label_new("   ");
        gtk_container_add(GTK_CONTAINER(box_about_right),label_address);

        dev = audio_devices;
        if(dev != NULL) {
            temp = g_strdup_printf("0x%x",((DeviceData*)(dev->data))->class);
            gtk_label_set_text(GTK_LABEL(label_prod),((DeviceData*)(dev->data))->name);
            gtk_label_set_text(GTK_LABEL(label_class),temp);
            gtk_label_set_text(GTK_LABEL(label_address),((DeviceData*)(dev->data))->address);
            g_free(temp);
        }

        gtk_window_set_default_size (GTK_WINDOW (window), 460, 150);
        if (!GTK_WIDGET_VISIBLE (window))
            gtk_widget_show_all (window);
        else
        {
            gtk_widget_destroy (window);
            window = NULL;
        }
        //   return window;
    }
    //  return window;
}

