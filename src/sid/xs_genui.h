#include <gtk/gtk.h>


gboolean
xs_configwin_delete                    (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
xs_cfg_oversample_toggled              (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_emu_sidplay1_toggled            (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_emu_sidplay2_toggled            (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_emu_filters_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_sp1_filter_reset                (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_sp2_filter_export               (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_sp2_filter_load                 (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_sp2_filter_save                 (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_sp2_filter_import               (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_sp2_filter_delete               (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_mintime_enable_toggled          (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_mintime_changed                 (GtkEditable     *editable,
                                        gpointer         user_data);

void
xs_cfg_maxtime_enable_toggled          (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_maxtime_changed                 (GtkEditable     *editable,
                                        gpointer         user_data);

void
xs_cfg_sldb_enable_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_sldb_browse                     (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_stil_enable_toggled             (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_stil_browse                     (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_hvsc_browse                     (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_ftitle_override_toggled         (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_subauto_enable_toggled          (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_subauto_min_only_toggled        (GtkToggleButton *togglebutton,
                                        gpointer         user_data);

void
xs_cfg_ok                              (GtkButton       *button,
                                        gpointer         user_data);

void
xs_cfg_cancel                          (GtkButton       *button,
                                        gpointer         user_data);

gboolean
xs_fileinfo_delete                     (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
xs_subctrl_prevsong                    (GtkButton       *button,
                                        gpointer         user_data);

void
xs_subctrl_nextsong                    (GtkButton       *button,
                                        gpointer         user_data);

void
xs_fileinfo_ok                         (GtkButton       *button,
                                        gpointer         user_data);

gboolean
xs_sldb_fs_delete                      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
xs_sldb_fs_ok                          (GtkButton       *button,
                                        gpointer         user_data);

void
xs_sldb_fs_cancel                      (GtkButton       *button,
                                        gpointer         user_data);

gboolean
xs_stil_fs_delete                      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
xs_stil_fs_ok                          (GtkButton       *button,
                                        gpointer         user_data);

void
xs_stil_fs_cancel                      (GtkButton       *button,
                                        gpointer         user_data);

gboolean
xs_hvsc_fs_delete                      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
xs_hvsc_fs_ok                          (GtkButton       *button,
                                        gpointer         user_data);

void
xs_hvsc_fs_cancel                      (GtkButton       *button,
                                        gpointer         user_data);

gboolean
xs_filter_import_fs_delete             (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
xs_filter_import_fs_ok                 (GtkButton       *button,
                                        gpointer         user_data);

void
xs_filter_import_fs_cancel             (GtkButton       *button,
                                        gpointer         user_data);

gboolean
xs_filter_export_fs_delete             (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
xs_filter_export_fs_ok                 (GtkButton       *button,
                                        gpointer         user_data);

void
xs_filter_export_fs_cancel             (GtkButton       *button,
                                        gpointer         user_data);

gboolean
xs_confirmwin_delete                   (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
xs_cfg_ftitle_override_toggled         (GtkToggleButton *togglebutton,
                                        gpointer         user_data);
