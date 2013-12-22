/*
 * some functions for MP4 files
*/

#include <stdlib.h>
#include <glib.h>

#include "mp4ff.h"
#include "neaacdec.h"

const char *mp4AudioNames[] = {
    "MPEG-1 Audio Layers 1,2 or 3",
    "MPEG-2 low biterate (MPEG-1 extension) - MP3",
    "MPEG-2 AAC Main Profile",
    "MPEG-2 AAC Low Complexity profile",
    "MPEG-2 AAC SSR profile",
    "MPEG-4 audio (MPEG-4 AAC)",
    0
};

/* MPEG-4 Audio types from 14496-3 Table 1.5.1 (from mp4.h)*/
const char *mpeg4AudioNames[] = {
    "!!!!MPEG-4 Audio track Invalid !!!!!!!",
    "MPEG-4 AAC Main profile",
    "MPEG-4 AAC Low Complexity profile",
    "MPEG-4 AAC SSR profile",
    "MPEG-4 AAC Long Term Prediction profile",
    "MPEG-4 AAC Scalable",
    "MPEG-4 CELP",
    "MPEG-4 HVXC",
    "MPEG-4 Text To Speech",
    "MPEG-4 Main Synthetic profile",
    "MPEG-4 Wavetable Synthesis profile",
    "MPEG-4 MIDI Profile",
    "MPEG-4 Algorithmic Synthesis and Audio FX profile"
};


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
