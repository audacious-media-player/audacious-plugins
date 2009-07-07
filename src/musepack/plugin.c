#include <audacious/plugin.h>

void mpcOpenPlugin();
void mpcAboutBox();
void mpcConfigBox();
gint mpcIsOurFile(gchar* p_Filename);
gint mpcIsOurFD(gchar* p_Filename, VFSFile* file);
void mpcPlay(InputPlayback *data);
void mpcStop(InputPlayback *data);
void mpcPause(InputPlayback *data, short p_Pause);
void mpcSeek(InputPlayback *data, int p_Offset);
gint mpcGetTime(InputPlayback *data);
Tuple *mpcGetSongTuple(gchar* p_Filename);
void mpcGetSongInfo(char* p_Filename, char** p_Title, int* p_Length);
void mpcFileInfoBox(char* p_Filename);

static const gchar *mpc_fmts[] = { "mpc", NULL };

static InputPlugin MpcPlugin = {
    .description = "Musepack Audio Plugin",
    .init = mpcOpenPlugin,              //Open Plugin               [CALLBACK]
    .about = mpcAboutBox,               //Show About box            [CALLBACK]
    .configure = mpcConfigBox,          //Show Configure box        [CALLBACK]
    .enabled = FALSE,                   //Enabled/Disabled          [BOOLEAN]
    .is_our_file = mpcIsOurFile,        //Check if it's our file    [CALLBACK]
    .play_file = mpcPlay,               //Play                      [CALLBACK]
    .stop = mpcStop,                    //Stop                      [CALLBACK]
    .pause = mpcPause,                  //Pause                     [CALLBACK]
    .seek = mpcSeek,                    //Seek                      [CALLBACK]
    .get_time = mpcGetTime,             //Get Time                  [CALLBACK]
    .get_song_info = mpcGetSongInfo,    //Get Title String callback [CALLBACK]
    .file_info_box = mpcFileInfoBox,    //Show File Info Box        [CALLBACK]
    .get_song_tuple = mpcGetSongTuple,  //Acquire tuple for song    [CALLBACK]
    .is_our_file_from_vfs = mpcIsOurFD,
    .vfs_extensions = (gchar **)mpc_fmts
};

InputPlugin *mpc_iplist[] = { &MpcPlugin, NULL };

DECLARE_PLUGIN(musepack, NULL, NULL, mpc_iplist, NULL, NULL, NULL, NULL,NULL);
