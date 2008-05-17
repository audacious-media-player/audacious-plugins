#include "bluetooth.h"
#define DEBUG 1
static gboolean plugin_active = FALSE,exiting=FALSE;

void bluetooth_init ( void );
void bluetooth_cleanup ( void );
void bt_cfg(void);
void bt_about(void);
GeneralPlugin bluetooth_gp =
{
    .description = "Bluetooth audio suport",
    .init = bluetooth_init,
    .about = bt_about,
    .configure = bt_cfg,
    .cleanup = bluetooth_cleanup
};
GeneralPlugin *bluetooth_gplist[] = { &bluetooth_gp, NULL };
DECLARE_PLUGIN(bluetooth_gp, NULL, NULL, NULL, NULL, NULL, bluetooth_gplist, NULL, NULL)

void bluetooth_init ( void )
{

}

void bluetooth_cleanup ( void )
{

}
/*void bt_cfg( void )
{

}
*/
void bt_about( void )
{

}


void refresh_call(void){
    printf("refresh function called\n");
}

void connect_call(void){
    printf("connect function \n");
}

