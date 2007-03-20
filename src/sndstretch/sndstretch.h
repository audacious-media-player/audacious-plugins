// sndstretch.h
//
//    sndstretch - algorithm for adjusting pitch and speed of s16le data
//    Copyright (C) 2000  Florian Berger
//    Email: florian.berger@jk.uni-linz.ac.at
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License Version 2 as
//    published by the Free Software Foundation;
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//

#ifndef SNDSTRETCH_H
#define SNDSTRETCH_H


#ifdef __cplusplus
extern "C" {
#endif


typedef short int s16;


typedef struct{
    int     is_initialized;    // init me = 0
    int     snr_o_prod;
    int     snr_i_act;
    int     snr_o_act;
    int     pos_act;
    int     dsnr;
    double  snr_rest;        // init me = 0.0

    int     _RESMAX_div_max, _RESMAX_mod_max;
    int     fade_in_i, fade_out_i, fade_rest_i;
} StretchJob;

typedef struct{
    s16     last_samp[10];        /* 10 channels maximum ;) */
    int     pos_rest;
    
    int     snr;
    int     pos1, pos2;
    int     ch;
    int     ratio1_i;
    int     ds_li, ds_li_c, ds_rest;
    int     snr_proc_m_chnr;
} ScaleJob;

typedef struct{
    s16 *  ring_buff;        // init me = (s16*)0
    s16 *  ring_buff_old;    // init me = (s16*)0
    s16 *  buff_help;        // init me = (s16*)0
    int    ring_size;              // init me = 1
    int    ring_size_old;          // init me = 0
    int    ring_pos_w;             // init me = 0
    int    ring_pos_r;             // init me = 0
    int    snr_scale_i;
    int    snr_scale_o;
    int    snr_stretch_i;
    int    snr_stretch_o;
    int    snr_proc_scale;
    int    snr_proc_stretch;
    int    is_init;                // init me = 0
    int    dsnr;
    double speed_act;              // init me = 0
    double pitch_act;              // init me = 0
    int    fade_shift_act;
    StretchJob    stretch_job;
    ScaleJob      scale_job;
} PitchSpeedJob;



int sndstretch(
// stretches the sound (not changing pitch !!!)
// returns number of output samples produced
           s16 * buffer, int buff_size,   // ring buffer
           int pos_init,                  // only initial pos in ringbuffer
           int snr_i, int snr_o,          // enlarge - spec
           int chnr,                      /* # of channels                  */
           s16 * outbuff,                 // output
           int * out_prod,                // # of output-samples produced
           int snr_proc,                  // # of in-samples to process
           int initialize                 // bool
              );

    
int sndscale(
// rescales the sound (including pitch)
// returns number of output samples produced
           s16 * buffer,                  // ring buffer
           int snr_i, int snr_o,          // enlarge - specification
                                          // (snr_o may not exceed 2^16 for
                                          //  integer optimized version)
           int chnr,                      // # of channels
           s16 * outbuff,                 // output
           int * out_prod,                // # of output-samples produced
           int snr_proc,                  // # of in-samples to process
           int initialize                 // bool
          );


int snd_pitch_speed(
                    /* input */
                    s16 *buff_i, int channels, int snr_proc,
                    /* algorihm parameters */
                    int initialize, double pitch, double speed, int fade_shift,
                    /* output */
                    s16 * buff_o, int * snr_produced
                   );


int snd_stretch_scale(s16 *buff_i, s16 * buff_o,
                      double pitch, double speed, int channels,
                      int snr_proc, int * snr_produced, int initialize );


void InitPitchSpeedJob( PitchSpeedJob * job );

int snd_pitch_speed_job(
                        /* input */
                        s16 *buff_i, int channels, int snr_proc,
                        /* algorihm parameters */
                        int initialize, double pitch, double speed, int fade_shift,
                        /* output */
                        s16 * buff_o, int * snr_produced, PitchSpeedJob * job,
                        int vol_corr
                       );

int snd_stretch_scale_job(s16 *buff_i, s16 * buff_o,
                          double pitch, double speed, int channels,
                          int snr_proc, int * snr_produced, int initialize,
                          PitchSpeedJob * job, int init_job);

void CleanupPitchSpeedJob( PitchSpeedJob * job );
/* cleaning up the allocated mess would be a good idea */

#ifdef __cplusplus
}
#endif

#endif // SNDSTRETCH_H
