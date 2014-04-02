/*
 * some functions for MP4 files
*/

#include <stdlib.h>
#include <glib.h>

#include "mp4ff.h"
#include "neaacdec.h"

int getAACTrack (mp4ff_t * infile)
{
    int i, rc, numTracks = mp4ff_total_tracks (infile);
    for (i = 0; i < numTracks; i++)
    {
        unsigned char *buff = NULL;
        unsigned buff_size = 0;
        mp4AudioSpecificConfig mp4ASC;

        mp4ff_get_decoder_config (infile, i, &buff, &buff_size);
        if (buff != NULL)
        {
            rc = AudioSpecificConfig (buff, buff_size, &mp4ASC);
            g_free (buff);
            if (rc < 0)
                continue;
            return i;
        }
    }
    return -1;
}
