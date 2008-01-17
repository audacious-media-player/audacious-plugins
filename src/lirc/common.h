extern GtkWidget *lirc_cfg;

extern gint b_enable_reconnect;
extern gint reconnect_timeout;
extern gchar *aosd_font;

void load_cfg(void);
void save_cfg(void);

void configure(void);

GtkWidget* create_lirc_cfg (void);

void
on_reconnectcheck_toggled              (GtkToggleButton *togglebutton,
                                        GtkWidget       *reconnectspin);
void
on_cancelbutton1_clicked               (GtkButton       *button,
                                        gpointer         user_data);
void
on_okbutton1_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

