#include "gui.h"
#include "bluetooth.h"
#include <gtk/gtk.h>
#include <glib.h>

static GtkWidget *window = NULL;
static GtkTreeModel *model;
GtkWidget *mainbox;
GtkWidget *hbox_top;
GtkWidget *hbox_bottom;
GtkWidget *box_about;
GtkWidget *box_about_left;
GtkWidget *box_about_right;
GtkWidget *headset_frame;
GtkWidget *about_frame;
GtkWidget *refresh;
GtkWidget *connect_button;
GtkWidget *close_button;   
GtkWidget *treeview;
GtkWidget *label_p;
GtkWidget *label_m;
GtkWidget *label_a;
GtkWidget *label_prod;
GtkWidget *label_model;
GtkWidget *label_address;


typedef struct 
{
    gchar *producer;
    gchar *model;
}Headset;

enum{
    COLUMN_PRODUCER,
    COLUMN_MODEL,
    NUM_COLUMNS
};

static Headset test_data[]=
{
    {"Motorola", "S9"},
    {"Nokia", "BH-503"},
    {"Blueant","Stereo X5"}
};

static GtkTreeModel * create_model(void)
{
    gint i = 0;
    GtkListStore *store;
    GtkTreeIter iter;
    /* create list store */
    store = gtk_list_store_new(NUM_COLUMNS,
            G_TYPE_STRING,
            G_TYPE_STRING);
    /* add data to the list store */
    for(i = 0;i<G_N_ELEMENTS(test_data);i++)
    {
        gtk_list_store_append(store,&iter);
        gtk_list_store_set(store,&iter,
                COLUMN_PRODUCER, test_data[i].producer,
                COLUMN_MODEL, test_data[i].model,
                -1);
    }
    return GTK_TREE_MODEL(store);                       
}

static void add_columns(GtkTreeView *treeview)
{
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    // GtkTreeModel *model = gtk_tree_view_get_model (treeview);

    /* column for producer */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Producer",
            renderer,
            "text",
            COLUMN_PRODUCER,
            NULL);
    gtk_tree_view_column_set_sort_column_id (column,COLUMN_PRODUCER);
    gtk_tree_view_append_column (treeview, column);

    /* column for model */
    renderer = gtk_cell_renderer_text_new ();
    column = gtk_tree_view_column_new_with_attributes ("Model",
            renderer,
            "text",
            COLUMN_MODEL,
            NULL);
    gtk_tree_view_column_set_sort_column_id (column,COLUMN_MODEL);
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
    printf("select\n");
    GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(treeview)); 
    if(gtk_tree_selection_get_selected (selection, NULL,&iter)){
      GtkTreePath *path;
      path = gtk_tree_model_get_path (model, &iter);
      sel = gtk_tree_path_get_indices (path)[0];
      printf("i=%d\n",sel);
      gtk_label_set_text(GTK_LABEL(label_prod),test_data[sel].producer);
      gtk_label_set_text(GTK_LABEL(label_model),test_data[sel].model);
      gtk_label_set_text(GTK_LABEL(label_address),"00:01:02:03:04:05");

      gtk_tree_path_free (path);

    }



}
void bt_cfg()
{
    
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

        headset_frame = gtk_frame_new("Available Headsets");
        gtk_container_add (GTK_CONTAINER (hbox_top), headset_frame);

        about_frame = gtk_frame_new("Current Headset");
        gtk_container_add(GTK_CONTAINER(hbox_top),about_frame);

        refresh = gtk_button_new_with_mnemonic ("_Refresh");
        g_signal_connect (refresh, "clicked",G_CALLBACK (refresh_call), NULL);
        gtk_container_add(GTK_CONTAINER(hbox_bottom),refresh);

        connect_button = gtk_button_new_with_mnemonic("_Connect");
        g_signal_connect(connect_button,"clicked",G_CALLBACK (connect_call), NULL);
        gtk_container_add(GTK_CONTAINER(hbox_bottom),connect_button);

        close_button = gtk_button_new_with_mnemonic("_Close");
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
        label_p = gtk_label_new("Producer:");
        gtk_container_add(GTK_CONTAINER(box_about_left),label_p);

        label_m = gtk_label_new("Model:");
        gtk_container_add(GTK_CONTAINER(box_about_left),label_m);


        label_a = gtk_label_new("Address:");
        gtk_container_add(GTK_CONTAINER(box_about_left),label_a);


        /*right labels */
        label_prod = gtk_label_new("Producer:");
        gtk_container_add(GTK_CONTAINER(box_about_right),label_prod);

        label_model = gtk_label_new("Model:");
        gtk_container_add(GTK_CONTAINER(box_about_right),label_model);


        label_address = gtk_label_new("Address:");
        gtk_container_add(GTK_CONTAINER(box_about_right),label_address);
        
        gtk_window_set_default_size (GTK_WINDOW (window), 480, 180);
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

