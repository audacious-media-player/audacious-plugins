// sndstretch.c
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

//#define DEBUG

#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include"sndstretch.h"


static double _1_div_e     = 0.367879441;  // 1/e
static double _1_m_1_div_e = 0.632120558;  // 1-1/e

#define    RESMAX      65536
#define    RESMAXVC    32768
#define    LOG2RESMAX     16
#define    LOG2RESMAXVC   15
static int    _1_div_e_i     = 24109;  // 1/e   * 2^LOG2RESMAX
static int    _1_m_1_div_e_i = 41427;  // 1-1/e * 2^LOG2RESMAX
static int    _1_div_e_i_vc   = 12055;  // 1/e   * 2^LOG2RESMAXVC
static int    _1_m_1_div_e_i_vc = 28333;  // sqrt(1-1/e^2) * 2^LOG2RESMAXVC
//static int    _1_div_e_i     = 12055;  // 1/e   * 2^LOG2RESMAX
//static int    _1_m_1_div_e_i = 20713;  // 1-1/e * 2^LOG2RESMAX


void InitScaleJob( ScaleJob * job )
{
    /* nothing to do */
}


void InitStretchJob( StretchJob * job )
{
    job->is_initialized=0;
    job->snr_rest=0.0;
}


void InitPitchSpeedJob( PitchSpeedJob * job )
{
    job->ring_buff = (s16*)0;        // init me = (s16*)0
    job->ring_buff_old = (s16*)0;    // init me = (s16*)0
    job->buff_help = (s16*)0;        // init me = (s16*)0
    job->ring_size = 1;              // init me = 1
    job->ring_size_old = 0;          // init me = 0
    job->ring_pos_w = 0;             // init me = 0
    job->ring_pos_r = 0;             // init me = 0
    job->is_init = 0;                // init me = 0
    job->speed_act = 0;              // init me = 0
    job->pitch_act = 0;              // init me = 0
    job->fade_shift_act = 0;
    InitStretchJob(&(job->stretch_job));
    InitScaleJob(&(job->scale_job));
}



void CleanupPitchSpeedJob( PitchSpeedJob * job )
{
    free(job->ring_buff);
    free(job->ring_buff_old);
    free(job->buff_help);
}



static inline int ringpos( int pos, int size )
{
    while ( pos >= size ) pos-=size;
    while ( pos <  0    ) pos+=size;
    return( pos );
}


void ringload( s16 * ringbuff, int ring_size, int pos, s16 *buffer, int size )
/* put <size> samples to ring with size <ring_size> on position <pos> */
{
    int i,j;

    j=0;
    if( pos+size > ring_size ){
        for( i=pos; i<ring_size;          i++,j++ ) ringbuff[i]=buffer[j];
        for( i=0;   i<size-ring_size+pos; i++,j++ ) ringbuff[i]=buffer[j];
    }else{
        for( i=pos; i<size+pos;           i++,j++ ) ringbuff[i]=buffer[j];
    }
}


void ringcopy( s16 * src_ring,  int src_size,  int src_from, int src_to,
               s16 * dest_ring, int dest_size, int dest_pos )
{
    int is,id;

    is=src_from; id=dest_pos;
    while( is!=src_to ){
        dest_ring[id]=src_ring[is];
        is=ringpos(is+1,src_size);
        id=ringpos(id+1,dest_size);
    }
}


void ringload_IIR_1_div_e_echo_d( s16 * ringbuff, int ring_size, int pos, s16 *buffer, int size , int delay)
/* put <size> samples to ring with size <ring_size> on position <pos> */
{
    int i,p1,p2;

    p1=pos, p2=ringpos(p1-delay, ring_size);
    for( i=0; i<size; i++ ){
        ringbuff[p1] =(s16)( (double)buffer[i]*_1_m_1_div_e + (double)ringbuff[p2]*_1_div_e );
        p1++; if(p1>=ring_size) p1-=ring_size;
        p2++; if(p2>=ring_size) p2-=ring_size;
    }
}


void ringload_IIR_1_div_e_echo_i( s16 * ringbuff, int ring_size, int pos, s16 *buffer, int size , int delay)
/* put <size> samples to ring with size <ring_size> on position <pos> */
{
    int i,p1,p2;

    p1=pos, p2=ringpos(p1-delay, ring_size);
    for( i=0; i<size; i++ ){
        ringbuff[p1] =(s16)( (
                              _1_m_1_div_e_i * buffer[i] +
                              _1_div_e_i     * ringbuff[p2]
                             ) >> LOG2RESMAX );
        p1++; if(p1>=ring_size) p1-=ring_size;
        p2++; if(p2>=ring_size) p2-=ring_size;
    }
}


void ringload_IIR_1_div_e_echo_i_vc( s16 * ringbuff, int ring_size, int pos, s16 *buffer, int size , int delay)
/* put <size> samples to ring with size <ring_size> on position <pos> */
{
    int i,p1,p2;
    int actval;

    p1=pos, p2=ringpos(p1-delay, ring_size);
    for( i=0; i<size; i++ ){
/*        ringbuff[p1] =(s16)( (
                              _1_m_1_div_e_i_vc * buffer[i] +
                              _1_div_e_i_vc     * ringbuff[p2]
                             ) >> LOG2RESMAXVC );*/
        actval = _1_m_1_div_e_i_vc * buffer[i] +
                 _1_div_e_i_vc     * ringbuff[p2];
        /* prevent overflow */
        if( actval > (int)0x3FFFFFFF ) actval=(int)0x3FFFFFFF;
        if( actval < (int)0xC0000000 ) actval=(int)0xC0000000;
        ringbuff[p1] = actval >> LOG2RESMAXVC;
        p1++; if(p1>=ring_size) p1-=ring_size;
        p2++; if(p2>=ring_size) p2-=ring_size;
    }
}


/*void ringget ( s16 * ringbuff, int ring_size, int pos, s16 *buffer, int size )
{
    int i;

    if( pos+size > ring_size ){
        for( i=pos; i<ring_size;          i++ ) buffer[i]=ringbuff[i];
        for( i=0;   i<size-ring_size+pos; i++ ) buffer[i]=ringbuff[i];
    }else{
        for( i=pos; i<size+pos;           i++ ) buffer[i]=ringbuff[i];
    }
}
*/

//int sndstretch( // not optimized
int sndstretch_not_optimized(
/* stretches the sound (not changing pitch !!!)                          */
/* returns number of output samples produced                             */
/* chnr simply gives multiples of chnr as snr_prod                       */
           s16 * buffer, int buff_size,   /* ring buffer                    */
           int pos_init,                  /* only initial pos in ringbuffer */
           int snr_i, int snr_o,          /* enlarge - spec                 */
           int chnr,                      /* # of channels                  */
           s16 * outbuff,                 /* output                         */
           int * out_prod,                /* # of output-samples produced   */
           int snr_proc,                  /* # of in-samples to process     */
           int initialize                 /* bool                           */
          )
{
    static int     is_initialized=0;
    static int     snr_o_prod;
    static int     snr_i_act;
    static int     snr_o_act;
/*    static int     snr_offs; */
    static int     pos_act;
    static int     dsnr;
/*    static int     p1,p2;    */
    static double  snr_rest=0.0;

    int            snr;
    double         snr_d;

    int            i,p1,p2;
    double         fade_in, fade_out;
    double         outd;

/*    s16 *          outbuff; */

    /* reset */
    if( !is_initialized || initialize ||
        snr_i!=snr_i_act || snr_o!=snr_o_act ){

        snr_rest   = 0.0;
        snr_o_prod = 0;
        snr_i_act  = snr_i;
        snr_o_act  = snr_o;
        dsnr       = snr_o_act-snr_i_act;
        pos_act    = pos_init;

        is_initialized = 1;

    }

/*    fprintf(stderr,"pos_act=%d\n",pos_act);
*/
    snr_d      = (double)snr_proc*(double)snr_o_act/(double)snr_i_act
                 + snr_rest;
    snr        = (int) (snr_d) / 2 * 2;
    snr_rest   = snr_d - (double)snr;

/*    fprintf(stderr,"snr=%d\n",snr);
*/
/*    outbuff=malloc(snr*sizeof(s16));
*/
    /* produce snr samples */
    i=0;
    do {
        if( snr_o_prod == snr_o_act ) {
            snr_o_prod=0;
/*            fprintf(stderr,"!!!!!!!!!!!!!!!!!!!!!!!!!! dsnr = %d\n",dsnr);*/
            pos_act = ringpos( pos_act-dsnr, buff_size );
        }
        for ( ; snr_o_prod < snr_o_act && i<snr ; snr_o_prod++,i++ ){
            p1=pos_act;
            p2=ringpos( p1-dsnr, buff_size );
//            p3=ringpos( p2-dsnr, buff_size );
//            p4=ringpos( p3-dsnr, buff_size );
//            p2=ringpos( p1-abs(dsnr), buff_size );
//            p3=ringpos( p2-abs(dsnr), buff_size );
//            p4=ringpos( p3-abs(dsnr), buff_size );
            fade_in  = (double)snr_o_prod/(double)snr_o_act;
            fade_out = 1.0-fade_in;
//            fade1 = (dsnr>0)?fade_out:fade_in;
//            fade2 = (dsnr>0)?fade_in:fade_out;
            outd = (
                      (double)buffer[p1] * fade_out  /* fade out */
                    + (double)buffer[p2] * fade_in   /* fade in  */
//                    +( 0.67 * (double)buffer[p1]             /* fade out for lengthen */
//                     + 0.24 * (double)buffer[p2]             /* fade out */
//                     + 0.09 * (double)buffer[p3]) * fade_out /* fade out */
//                    +( 0.67 * (double)buffer[p2]             /* fade in  */
//                     + 0.24 * (double)buffer[p3]             /* fade in  */
//                     + 0.09 * (double)buffer[p4]) * fade_in  /* fade in  */
                   );
            outbuff[i] = outd+0.5;
            pos_act=ringpos( pos_act+1, buff_size );
        }
    } while( i<snr );

/*    fwrite( outbuff, sizeof(s16), snr, stdout );
*/
/*    free(outbuff);
*/
    *out_prod = snr;

/*    fprintf(stderr,"snr = %d\n",snr);
*/
    return( *out_prod );
}


int sndstretch( // optimized
//int sndstretch_optimized( // optimized
/* stretches the sound (not changing pitch !!!)                          */
/* returns number of output samples produced                             */
/* chnr simply gives multiples of chnr as snr_prod                       */
           s16 * buffer, int buff_size,   /* ring buffer                    */
           int pos_init,                  /* only initial pos in ringbuffer */
           int snr_i, int snr_o,          /* enlarge - spec                 */
           int chnr,                      /* # of channels                  */
           s16 * outbuff,                 /* output                         */
           int * out_prod,                /* # of output-samples produced   */
           int snr_proc,                  /* # of in-samples to process     */
           int initialize                 /* bool                           */
          )
{
    static int     is_initialized=0;
    static int     snr_o_prod;
    static int     snr_i_act;
    static int     snr_o_act;
    static int     pos_act;
    static int     dsnr;
    static double  snr_rest=0.0;

    static int     _RESMAX_div_max, _RESMAX_mod_max;
    static int     fade_in_i, fade_out_i, fade_rest_i;

    /* not necessarily static */
    static int            snr;
    static double         snr_d;
    static int            i,p2;

    /* reset */
    if( !is_initialized || initialize ||
        snr_i!=snr_i_act || snr_o!=snr_o_act ){

        snr_rest   = 0.0;
        snr_o_prod = 0;
        snr_i_act  = snr_i;
        snr_o_act  = snr_o;
        dsnr       = snr_o_act-snr_i_act;
        pos_act    = pos_init;

        is_initialized = 1;

    }

    snr_d      = (double)snr_proc*(double)snr_o_act/(double)snr_i_act
                 + snr_rest;
    snr        = (int) (snr_d) / 2 * 2;
    snr_rest   = snr_d - (double)snr;

    /* produce snr samples */
    i=0;
    do {
        if( snr_o_prod == snr_o_act ) {
            snr_o_prod=0;
            pos_act = ringpos( pos_act-dsnr, buff_size );
        }
        fade_in_i  = RESMAX*((double)snr_o_prod/(double)snr_o_act);
        fade_out_i = RESMAX-fade_in_i;
        fade_rest_i= (RESMAX*snr_o_prod)%snr_o_act;
        _RESMAX_div_max=RESMAX/snr_o_act;
        _RESMAX_mod_max=RESMAX%snr_o_act;
        p2=ringpos( pos_act-dsnr, buff_size );
        for ( ; snr_o_prod < snr_o_act && i<snr ; snr_o_prod++,i++ ){
            fade_in_i  +=_RESMAX_div_max;
            fade_out_i -=_RESMAX_div_max;
            fade_rest_i+=_RESMAX_mod_max;
            if(fade_rest_i>snr_o_act){
                fade_rest_i-=snr_o_act;
                fade_in_i++;
                fade_out_i--;
            }
            outbuff[i] = (s16)( (
                                 + (int)buffer[pos_act] * fade_out_i
                                 + (int)buffer[p2]      * fade_in_i
                                ) >> LOG2RESMAX );
            pos_act++; if(pos_act>=buff_size) pos_act-=buff_size;
            p2++;      if(p2     >=buff_size) p2     -=buff_size;
        }
    } while( i<snr );

    *out_prod = snr;

    return( *out_prod );
}


int sndstretch_job( // optimized
//int sndstretch_optimized( // optimized
/* stretches the sound (not changing pitch !!!)                          */
/* returns number of output samples produced                             */
/* chnr simply gives multiples of chnr as snr_prod                       */
           s16 * buffer, int buff_size,   /* ring buffer                    */
           int pos_init,                  /* only initial pos in ringbuffer */
           int snr_i, int snr_o,          /* enlarge - spec                 */
           int chnr,                      /* # of channels                  */
           s16 * outbuff,                 /* output                         */
           int * out_prod,                /* # of output-samples produced   */
           int snr_proc,                  /* # of in-samples to process     */
           int initialize,                 /* bool                           */
           StretchJob * job
          )
{
/*    static int     is_initialized=0;
    static int     snr_o_prod;
    static int     snr_i_act;
    static int     snr_o_act;
    static int     pos_act;
    static int     dsnr;
    static double  snr_rest=0.0;

    static int     _RESMAX_div_max, _RESMAX_mod_max;
    static int     fade_in_i, fade_out_i, fade_rest_i;*/

    /* not necessarily static */
    static int            snr;
    static double         snr_d;
    static int            i,p2;

#define    is_initialized    (job->is_initialized )
#define    snr_o_prod        (job->snr_o_prod     )
#define    snr_i_act         (job->snr_i_act      )
#define    snr_o_act         (job->snr_o_act      )
#define    pos_act           (job->pos_act        )
#define    dsnr              (job->dsnr           )
#define    snr_rest          (job->snr_rest       )
#define    _RESMAX_div_max   (job->_RESMAX_div_max)
#define    _RESMAX_mod_max   (job->_RESMAX_mod_max)
#define    fade_in_i         (job->fade_in_i      )
#define    fade_out_i        (job->fade_out_i     )
#define    fade_rest_i       (job->fade_rest_i    )

    /* reset */
    if( !is_initialized || initialize ||
        snr_i!=snr_i_act || snr_o!=snr_o_act ){

        snr_rest   = 0.0;
        snr_o_prod = 0;
        snr_i_act  = snr_i;
        snr_o_act  = snr_o;
        dsnr       = snr_o_act-snr_i_act;
        pos_act    = pos_init;

        is_initialized = 1;

    }

    snr_d      = (double)snr_proc*(double)snr_o_act/(double)snr_i_act
                 + snr_rest;
    snr        = (int) (snr_d) / 2 * 2;
    snr_rest   = snr_d - (double)snr;

    /* produce snr samples */
    i=0;
    do {
        if( snr_o_prod == snr_o_act ) {
            snr_o_prod=0;
            pos_act = ringpos( pos_act-dsnr, buff_size );
        }
        fade_in_i  = RESMAX*((double)snr_o_prod/(double)snr_o_act);
        fade_out_i = RESMAX-fade_in_i;
        fade_rest_i= (RESMAX*snr_o_prod)%snr_o_act;
        _RESMAX_div_max=RESMAX/snr_o_act;
        _RESMAX_mod_max=RESMAX%snr_o_act;
        p2=ringpos( pos_act-dsnr, buff_size );
        for ( ; snr_o_prod < snr_o_act && i<snr ; snr_o_prod++,i++ ){
            fade_in_i  +=_RESMAX_div_max;
            fade_out_i -=_RESMAX_div_max;
            fade_rest_i+=_RESMAX_mod_max;
            if(fade_rest_i>snr_o_act){
                fade_rest_i-=snr_o_act;
                fade_in_i++;
                fade_out_i--;
            }
            outbuff[i] = (s16)( (
                                 + (int)buffer[pos_act] * fade_out_i
                                 + (int)buffer[p2]      * fade_in_i
                                ) >> LOG2RESMAX );
            pos_act++; if(pos_act>=buff_size) pos_act-=buff_size;
            p2++;      if(p2     >=buff_size) p2     -=buff_size;
        }
    } while( i<snr );

    *out_prod = snr;

    return( *out_prod );

#undef    is_initialized
#undef    snr_o_prod
#undef    snr_i_act
#undef    snr_o_act
#undef    pos_act
#undef    dsnr
#undef    snr_rest
#undef    _RESMAX_div_max
#undef    _RESMAX_mod_max
#undef    fade_in_i
#undef    fade_out_i
#undef    fade_rest_i
}


//int sndscale(
int sndscale_not_optimized(
/* rescales the sound (including pitch)       */
/* returns number of output samples produced  */
           s16 * buffer,                  /* ring buffer                  */
           int snr_i, int snr_o,          /* enlarge - spec               */
           int chnr,                      /* # of channels                */
           s16 * outbuff,                 /* output                       */
           int * out_prod,                /* # of output-samples produced */
           int snr_proc,                  /* # of in-samples to process   */
                                          /* must be manifold of channels */
           int initialize                 /* bool                         */
          )
{
    static s16     last_samp[10];        /* 10 channels maximum ;) */
    static double  pos_d     = 0.0;

    int            snr;
    int            pos1, pos2;
    s16            samp1, samp2;
    int            index1, index2;
    int            ch;
    double         ds;
    double         ratio1, ratio2, outd;


    if ( initialize ){
        for( ch=0; ch<chnr; ch++ ){
            last_samp[ch] = 0;
        }
        pos_d     = 0.0;
    }

    ds         = 1.0*(double)snr_i/(double)snr_o;

/*    fprintf(stderr,"ds=%f\n",ds); */
/*    fprintf(stderr,"pos_d    =%f\n",pos_d);*/

    /* produce proper amount of samples */
    for( snr=0 ; pos_d < snr_proc/chnr-1 ; pos_d+=ds ){

        pos1  = (int)floor(pos_d);
        pos2  = pos1+1;
        ratio1 = 1-pos_d+floor(pos_d);
        ratio2 = pos_d-floor(pos_d);

        for( ch=0; ch<chnr; ch++ ){

            index1    = pos1*chnr+ch;
            index2    = pos2*chnr+ch;

            samp1 = (pos_d<0) ? last_samp[ch] : buffer[index1] ;
            samp2 = buffer[index2];

            outd = (double)samp1 * ratio1
                 + (double)samp2 * ratio2 ;

            outbuff[snr+ch] = outd+0.5;

        }

        snr+=chnr;

    }

    pos_d -= (double)(snr_proc/chnr);

    for( ch=0; ch<chnr; ch++ ){
        last_samp[ch] = buffer[snr_proc-chnr+ch];
    }

    *out_prod = snr;

/*    fprintf(stderr,"snr = %d\n",snr);*/

/*    last_samp = buffer[snr_proc-1]; */

    return( snr );
}



int sndscale( //optimized
//int sndscale2p_optimized(
/* rescales the sound (including pitch)       */
/* returns number of output samples produced  */
           s16 * buffer,                  /* ring buffer                  */
           int snr_i, int snr_o,          /* enlarge - spec   snr_o must not exceed RESMAX */
           int chnr,                      /* # of channels                */
           s16 * outbuff,                 /* output                       */
           int * out_prod,                /* # of output-samples produced */
           int snr_proc,                  /* # of in-samples to process   */
                                          /* must be manifold of channels */
           int initialize                 /* bool                         */
          )
{
    static s16     last_samp[10];        /* 10 channels maximum ;) */
    static int            pos_rest;

    static int            snr;
    static int            pos1, pos2;
    static int            ch;
    static int            ratio1_i;
    static int            ds_li, ds_li_c, ds_rest;
    static int            snr_proc_m_chnr;


    if ( initialize ){
        for( ch=0; ch<chnr; ch++ ){
            last_samp[ch] = 0;
        }
        pos1    = 0;
    }

    ds_li      = snr_i/snr_o;
    ds_li_c    = ds_li*chnr;
    ds_rest    = snr_i%snr_o;

    snr_proc_m_chnr = snr_proc-chnr;
    for( snr=0 ; pos1 < snr_proc_m_chnr ; pos1+=ds_li_c ){

        pos2  = pos1+chnr;
        ratio1_i = snr_o-pos_rest;

        if (pos1<0){
            for( ch=0; ch<chnr; ch++ ){
                outbuff[snr+ch] = (s16)
                    ( ( (int)last_samp[ch]   * ratio1_i +
                        (int)buffer[pos2+ch] * pos_rest ) / snr_o ) ;
            }
        } else {
            for( ch=0; ch<chnr; ch++ ){
                outbuff[snr+ch] = (s16)
                    ( ( (int)buffer[pos1+ch] * ratio1_i +
                        (int)buffer[pos2+ch] * pos_rest ) / snr_o ) ;
            }
        }

        snr+=chnr;

        pos_rest+=ds_rest;
        if( pos_rest>=snr_o ){ pos_rest-=snr_o; pos1+=chnr; }
    }

    pos1 -= snr_proc;

    for( ch=0; ch<chnr; ch++ ){
        last_samp[ch] = buffer[snr_proc-chnr+ch];
    }

    *out_prod = snr;

    return( snr );
}



int sndscale_job( //optimized
//int sndscale2p_optimized(
/* rescales the sound (including pitch)       */
/* returns number of output samples produced  */
           s16 * buffer,                  /* ring buffer                  */
           int snr_i, int snr_o,          /* enlarge - spec   snr_o must not exceed RESMAX */
           int chnr,                      /* # of channels                */
           s16 * outbuff,                 /* output                       */
           int * out_prod,                /* # of output-samples produced */
           int snr_proc,                  /* # of in-samples to process   */
                                          /* must be manifold of channels */
           int initialize,                /* bool                         */
           ScaleJob * job
          )
{
/*    static s16  *         last_samp;
    static int            pos_rest;

    static int            snr;
    static int            pos1, pos2;
    static int            ch;
    static int            ratio1_i;
    static int            ds_li, ds_li_c, ds_rest;
    static int            snr_proc_m_chnr;*/

#define    last_samp          job->last_samp
#define    pos_rest           job->pos_rest
#define    snr                job->snr
#define    pos1               job->pos1
#define    pos2               job->pos2
#define    ch                 job->ch
#define    ratio1_i           job->ratio1_i
#define    ds_li              job->ds_li
#define    ds_li_c            job->ds_li_c
#define    ds_rest            job->ds_rest
#define    snr_proc_m_chnr    job->snr_proc_m_chnr

    if ( initialize ){
        for( ch=0; ch<chnr; ch++ ){
            last_samp[ch] = 0;
        }
        pos1    = 0;
    }

    ds_li      = snr_i/snr_o;
    ds_li_c    = ds_li*chnr;
    ds_rest    = snr_i%snr_o;

    snr_proc_m_chnr = snr_proc-chnr;
    for( snr=0 ; pos1 < snr_proc_m_chnr ; pos1+=ds_li_c ){

        pos2  = pos1+chnr;
        ratio1_i = snr_o-pos_rest;

        if (pos1<0){
            for( ch=0; ch<chnr; ch++ ){
                outbuff[snr+ch] = (s16)
                    ( ( (int)last_samp[ch]   * ratio1_i +
                        (int)buffer[pos2+ch] * pos_rest ) / snr_o ) ;
            }
        } else {
            for( ch=0; ch<chnr; ch++ ){
                outbuff[snr+ch] = (s16)
                    ( ( (int)buffer[pos1+ch] * ratio1_i +
                        (int)buffer[pos2+ch] * pos_rest ) / snr_o ) ;
            }
        }

        snr+=chnr;

        pos_rest+=ds_rest;
        if( pos_rest>=snr_o ){ pos_rest-=snr_o; pos1+=chnr; }
    }

    pos1 -= snr_proc;

    for( ch=0; ch<chnr; ch++ ){
        last_samp[ch] = buffer[snr_proc-chnr+ch];
    }

    *out_prod = snr;

    return( snr );

#undef    last_samp
#undef    pos_rest
#undef    snr
#undef    pos1
#undef    pos2
#undef    ch
#undef    ratio1_i
#undef    ds_li
#undef    ds_li_c
#undef    ds_rest
#undef    snr_proc_m_chnr
}



int snd_pitch_speed(
                    /* input */
                    s16 *buff_i, int channels, int snr_proc,
                    /* algorihm parameters */
                    int initialize, double pitch, double speed, int fade_shift,
                    /* output */
                    s16 * buff_o, int * snr_produced
                   )
{
    static s16 *  ring_buff = (s16*)0;
    static s16 *  ring_buff_old = (s16*)0;
    static s16 *  buff_help = (s16*)0;
    static int    ring_size = 1;
    static int    ring_size_old = 0;
    static int    ring_pos_w = 0;
    static int    ring_pos_r = 0;
    static int    snr_scale_i;
    static int    snr_scale_o;
    static int    snr_stretch_i;
    static int    snr_stretch_o;
    static int    snr_proc_scale;
    static int    snr_proc_stretch;
    static int    is_init = 0;
    static int    dsnr;
    static double speed_act = 0;
    static double pitch_act = 0;
    static double fade_shift_act = 0;

    int       snr_prod;
    double    speed_eff;
    double    pitch_eff;
    int       scaling_first;
    int       snr_stretching_i_max;
    int       snr_stretching_o_max;

//    int       channels = 2;
    int       init_me  = 0;

    speed_eff = speed/pitch;
    pitch_eff = pitch;

    scaling_first=0;
//    if( pitch > 1.0 ) scaling_first=0; else scaling_first=1;

    if ( !is_init || initialize || speed!=speed_act || pitch!=pitch_act ||
         fade_shift!=fade_shift_act ){

        if( !is_init || initialize ) init_me=1; else init_me=0;

#ifdef DEBUG
        fprintf(stderr,"snd_stretch_scale - init - pitch:%f, speed:%f\n",pitch,speed);
#endif

        speed_act = speed;
        pitch_act = pitch;
        fade_shift_act = fade_shift;

//        if (ring_buff!=0) free(ring_buff);
//        if (buff_help!=0) free(buff_help);

        if (initialize != -1){

            dsnr       = fade_shift;
//            dsnr       = 1764; // 25Hz
//            dsnr       = 1536; // 30Hz
//            dsnr       = 1102; // 40Hz
//            dsnr       = 882;  // 50Hz

            if( scaling_first ){
                snr_stretching_i_max = ceil((double)snr_proc/pitch_act);
                snr_stretching_i_max += channels-1;
                snr_stretching_i_max /= channels;
                snr_stretching_i_max *= channels;
            } else {
                snr_stretching_i_max = (snr_proc+channels-1)/channels*channels;
            }

            snr_stretching_o_max =
                ceil((double)snr_stretching_i_max / speed_eff);
            snr_stretching_o_max =
                (snr_stretching_o_max+channels-1)/channels*channels;

            ring_size  = snr_stretching_o_max     /* for reading */
                       + dsnr*channels            /* for grabbing back */
                       + dsnr*channels            /* for readpos catching */
                                                  /* up with writepos     */
                       + 2*dsnr*channels ;          /* for 2 pre - echos */
//            ring_size  = snr_stretching_o_max+3*dsnr*channels;

            if( ring_size <= ring_size_old ){
                ring_size = ring_size_old;
#ifdef DEBUG
                fprintf(stderr," ring_size =  %d \n",ring_size);
#endif
            } else {
/*                if (ring_buff!=0) free(ring_buff);
                if (buff_help!=0) free(buff_help);
                ring_buff  = calloc( ring_size, sizeof(s16) );
                buff_help  = calloc( 65536, sizeof(s16) );
                ring_pos_r = 0;*/
                if (buff_help!=0) free(buff_help);
                ring_buff_old = ring_buff;
                ring_buff = calloc( ring_size, sizeof(s16) );
                buff_help = calloc( 65536, sizeof(s16) );
                if (ring_buff_old!=0) ringcopy( ring_buff, ring_size, ring_pos_r, ring_pos_w, ring_buff_old, ring_size_old, ring_pos_r );
                if (ring_buff_old!=0) free(ring_buff_old);
//                ring_pos_w=ringpos(ring_pos_w+ring_size_old,ring_size);
#ifdef DEBUG
                fprintf(stderr,"    %d s16-samples reserved\n",ring_size);
#endif
            }

            ring_pos_w =
                ringpos( ring_pos_r +         /* base             */
                         dsnr*channels        /* for grabing back */
//                         dsnr*2             /* for grabing back */ // why *2
                         , ring_size );
#ifdef DEBUG
            fprintf(stderr,"    ring_pos_w = %d\n",ring_pos_w);
#endif
            ring_pos_w = (ring_pos_w+(channels-1))/channels*channels;

            ring_size_old = ring_size;

            is_init = 1;

        } else {  /* initialize == -1 */
            if (ring_buff!=0) free(ring_buff);
            if (buff_help!=0) free(buff_help);
            /* buffers are released -> leave the function */
            return 0;                            /* !!! sloppy */
        }

    }

    if ( fabs(speed_eff-1.0)>0.001 ){ /*
                                     0.001 ?!
                                     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                                       this should be examined to the limits
                                     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                                  */
        snr_stretch_i = (double)dsnr/(1.0/speed_eff-1.0);
        snr_stretch_o = fabs(snr_stretch_i + dsnr);
        snr_stretch_i = abs(snr_stretch_i);
//        fprintf(stderr,"snd_stretch_scale - snr_str_i=%d\n",snr_stretch_i);
//        fprintf(stderr,"snd_stretch_scale - snr_str_o=%d\n",snr_stretch_o);
    } else {
        snr_stretch_i = 10;
        snr_stretch_o = 10;
    }

    if ( pitch_eff!=1.0 ){
        snr_scale_i   = (double)dsnr/(1.0/pitch_eff-1.0);
        snr_scale_o   = fabs(snr_scale_i + dsnr);
        snr_scale_i   = abs(snr_scale_i);
        if( 1 /* optimized version doesnt support all scaling */ ){
            if( snr_scale_o > 65536 ){
                snr_scale_i = (int)((double)snr_scale_i*((double)65536/(double)snr_scale_o)+0.5);
                snr_scale_o = 65536;
                // make sure that snr_o for sndscale wont exceed 2^16
            }
        }
//        fprintf(stderr,"snd_stretch_scale - snr_sc_i=%d\n",snr_stretch_i);
//        fprintf(stderr,"snd_stretch_scale - snr_sc_o=%d\n",snr_stretch_o);
    } else {
//        fprintf(stderr,"snd_stretch_scale - snr_scale_i=snr_scale_o=10\n");
        snr_scale_i   = 65536;
        snr_scale_o   = 65536;
    }

    if( 0/*scaling_first*/ ){

        snr_proc_scale = snr_proc;
        sndscale ( buff_i,
                   snr_scale_i, snr_scale_o, channels,
                   buff_help, &snr_prod, snr_proc_scale, init_me );
/*                   buff_o, &snr_prod, snr_proc_scale, 0 ); */

        if( speed_eff!=1.0 ){   /* add echo only when really stretching */
            ringload_IIR_1_div_e_echo_i( ring_buff, ring_size, ring_pos_w, buff_help, snr_prod, dsnr*channels );
        } else {
            ringload( ring_buff, ring_size, ring_pos_w, buff_help, snr_prod );
        }
        ring_pos_w = ringpos( ring_pos_w+snr_prod, ring_size );

        snr_proc_stretch = snr_prod;
        sndstretch ( ring_buff, ring_size, ring_pos_r,
                     snr_stretch_i*channels, snr_stretch_o*channels, channels,
                     buff_o, &snr_prod, snr_proc_stretch, init_me );

        ring_pos_r = ringpos( ring_pos_r+snr_prod, ring_size );

    } else {

//        fprintf(stderr,"sndstretch: ring_pos_r = %d\n",ring_pos_r);
//        fprintf(stderr,"sndstretch: ring_pos_w = %d\n\n",ring_pos_w);
        snr_prod = snr_proc;
        if( speed_eff!=1.0 ){   /* add echo only when really stretching */
            ringload_IIR_1_div_e_echo_i( ring_buff, ring_size, ring_pos_w, buff_i, snr_proc, dsnr*channels );
        } else {
            ringload( ring_buff, ring_size, ring_pos_w, buff_i, snr_proc );
        }
        ring_pos_w = ringpos( ring_pos_w+snr_proc, ring_size );

        snr_proc_stretch = snr_proc;
        sndstretch ( ring_buff, ring_size, ring_pos_r,
                     snr_stretch_i*channels, snr_stretch_o*channels, channels,
                     buff_help, &snr_prod, snr_proc_stretch, init_me );

        ring_pos_r = ringpos( ring_pos_r+snr_prod, ring_size );

        snr_proc_scale = snr_prod;
//        fprintf(stderr,"snr_scale_i, snr_scale_o = %d,%d\n",snr_scale_i, snr_scale_o);
        sndscale ( buff_help,
                   snr_scale_i, snr_scale_o, channels,
                   buff_o, &snr_prod, snr_proc_scale, init_me );

    }

    *snr_produced = snr_prod;

    return snr_prod;
}


int snd_pitch_speed_job(
                    /* input */
                    s16 *buff_i, int channels, int snr_proc,
                    /* algorihm parameters */
                    int initialize, double pitch, double speed, int fade_shift,
                    /* output */
                    s16 * buff_o, int * snr_produced, PitchSpeedJob * job,
                    int vol_corr
                   )
{
/*    static s16 *  ring_buff = (s16*)0;
    static s16 *  ring_buff_old = (s16*)0;
    static s16 *  buff_help = (s16*)0;
    static int    ring_size = 1;
    static int    ring_size_old = 0;
    static int    ring_pos_w = 0;
    static int    ring_pos_r = 0;
    static int    snr_scale_i;
    static int    snr_scale_o;
    static int    snr_stretch_i;
    static int    snr_stretch_o;
    static int    snr_proc_scale;
    static int    snr_proc_stretch;
    static int    is_init = 0;
    static int    dsnr;
    static double speed_act = 0;
    static double pitch_act = 0;*/

    int       snr_prod;
    double    speed_eff;
    double    pitch_eff;
    int       scaling_first;
    int       snr_stretching_i_max;
    int       snr_stretching_o_max;

//    int       channels = 2;
    int       init_me  = 0;

#define    ring_buff         job->ring_buff
#define    ring_buff_old     job->ring_buff_old
#define    buff_help         job->buff_help
#define    ring_size         job->ring_size
#define    ring_size_old     job->ring_size_old
#define    ring_pos_w        job->ring_pos_w
#define    ring_pos_r        job->ring_pos_r
#define    snr_scale_i       job->snr_scale_i
#define    snr_scale_o       job->snr_scale_o
#define    snr_stretch_i     job->snr_stretch_i
#define    snr_stretch_o     job->snr_stretch_o
#define    snr_proc_scale    job->snr_proc_scale
#define    snr_proc_stretch  job->snr_proc_stretch
#define    is_init           job->is_init
#define    dsnr              job->dsnr
#define    speed_act         job->speed_act
#define    pitch_act         job->pitch_act
#define    fade_shift_act    job->fade_shift_act

    speed_eff = speed/pitch;
    pitch_eff = pitch;

    scaling_first=0;
//    if( pitch > 1.0 ) scaling_first=0; else scaling_first=1;

    if ( !is_init || initialize || speed!=speed_act || pitch!=pitch_act ||
         fade_shift != fade_shift_act ){

        if( !is_init || initialize ) init_me=1; else init_me=0;

#ifdef DEBUG
        fprintf(stderr,"snd_stretch_scale - init - pitch:%f, speed:%f\n",pitch,speed);
#endif

        speed_act = speed;
        pitch_act = pitch;
#ifdef DEBUG
        if ( fade_shift != fade_shift_act ){
            fprintf(stderr,"changed fade_shift_act\n");
        }
#endif
        fade_shift_act = fade_shift;

//        if (ring_buff!=0) free(ring_buff);
//        if (buff_help!=0) free(buff_help);

        if (initialize != -1){

            dsnr       = fade_shift;
//            dsnr       = 1764; // 25Hz
//            dsnr       = 1536; // 30Hz
//            dsnr       = 1102; // 40Hz
//            dsnr       = 882;  // 50Hz

            if( scaling_first ){
                snr_stretching_i_max = ceil((double)snr_proc/pitch_act);
                snr_stretching_i_max += channels-1;
                snr_stretching_i_max /= channels;
                snr_stretching_i_max *= channels;
            } else {
                snr_stretching_i_max = (snr_proc+channels-1)/channels*channels;
            }

            snr_stretching_o_max =
                ceil((double)snr_stretching_i_max / speed_eff);
            snr_stretching_o_max =
                (snr_stretching_o_max+channels-1)/channels*channels;

            ring_size  = snr_stretching_o_max     /* for reading */
                       + dsnr*channels            /* for grabbing back */
                       + dsnr*channels            /* for readpos catching */
                                                  /* up with writepos     */
                       + 2*dsnr*channels ;          /* for 2 pre - echos */
//            ring_size  = snr_stretching_o_max+3*dsnr*channels;

            if( ring_size <= ring_size_old ){
                ring_size = ring_size_old;
#ifdef DEBUG
                fprintf(stderr," ring_size =  %d \n",ring_size);
#endif
            } else {
/*                if (ring_buff!=0) free(ring_buff);
                if (buff_help!=0) free(buff_help);
                ring_buff  = calloc( ring_size, sizeof(s16) );
                buff_help  = calloc( 65536, sizeof(s16) );
                ring_pos_r = 0;*/
                if (buff_help!=0) free(buff_help);
                ring_buff_old = ring_buff;
                ring_buff = calloc( ring_size, sizeof(s16) );
                buff_help = calloc( 65536, sizeof(s16) );
                if (ring_buff_old!=0) ringcopy( ring_buff, ring_size, ring_pos_r, ring_pos_w, ring_buff_old, ring_size_old, ring_pos_r );
                if (ring_buff_old!=0) free(ring_buff_old);
//                ring_pos_w=ringpos(ring_pos_w+ring_size_old,ring_size);
#ifdef DEBUG
                fprintf(stderr,"    %d s16-samples reserved\n",ring_size);
#endif
            }

            ring_pos_w =
                ringpos( ring_pos_r +         /* base             */
                         dsnr*channels        /* for grabing back */
//                         dsnr*2             /* for grabing back */ // why *2
                         , ring_size );
#ifdef DEBUG
            fprintf(stderr,"    ring_pos_w = %d\n",ring_pos_w);
#endif
            ring_pos_w = (ring_pos_w+(channels-1))/channels*channels;

            ring_size_old = ring_size;

            is_init = 1;

        } else {  /* initialize == -1 */
            if (ring_buff!=0) free(ring_buff);
            if (buff_help!=0) free(buff_help);
            /* buffers are released -> leave the function */
            return 0;                            /* !!! sloppy */
        }

    }

    if ( fabs(speed_eff-1.0)>0.001 ){ /*
                                     0.001 ?!
                                     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                                       this should be examined to the limits
                                     !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                                  */
        snr_stretch_i = (double)dsnr/(1.0/speed_eff-1.0);
        snr_stretch_o = fabs(snr_stretch_i + dsnr);
        snr_stretch_i = abs(snr_stretch_i);
//        fprintf(stderr,"snd_stretch_scale - snr_str_i=%d\n",snr_stretch_i);
//        fprintf(stderr,"snd_stretch_scale - snr_str_o=%d\n",snr_stretch_o);
    } else {
        snr_stretch_i = 10;
        snr_stretch_o = 10;
    }

    if ( pitch_eff!=1.0 ){
        snr_scale_i   = (double)dsnr/(1.0/pitch_eff-1.0);
        snr_scale_o   = fabs(snr_scale_i + dsnr);
        snr_scale_i   = abs(snr_scale_i);
        if( 1 /* optimized version doesnt support all scaling */ ){
            if( snr_scale_o > 65536 ){
                snr_scale_i = (int)((double)snr_scale_i*((double)65536/(double)snr_scale_o)+0.5);
                snr_scale_o = 65536;
                // make sure that snr_o for sndscale wont exceed 2^16
            }
        }
//        fprintf(stderr,"snd_stretch_scale - snr_sc_i=%d\n",snr_stretch_i);
//        fprintf(stderr,"snd_stretch_scale - snr_sc_o=%d\n",snr_stretch_o);
    } else {
//        fprintf(stderr,"snd_stretch_scale - snr_scale_i=snr_scale_o=10\n");
        snr_scale_i   = 65536;
        snr_scale_o   = 65536;
    }

    if( 0/*scaling_first*/ ){

        snr_proc_scale = snr_proc;
        sndscale_job ( buff_i,
                       snr_scale_i, snr_scale_o, channels,
                       buff_help, &snr_prod, snr_proc_scale, init_me,
                       &(job->scale_job) );
/*                   buff_o, &snr_prod, snr_proc_scale, 0 ); */

        if( speed_eff!=1.0 ){   /* add echo only when really stretching */
            if( !vol_corr ){
                ringload_IIR_1_div_e_echo_i( ring_buff, ring_size, ring_pos_w, buff_help, snr_prod, dsnr*channels );
            } else {
                ringload_IIR_1_div_e_echo_i_vc( ring_buff, ring_size, ring_pos_w, buff_help, snr_prod, dsnr*channels );
            }
        } else {
            ringload( ring_buff, ring_size, ring_pos_w, buff_help, snr_prod );
        }
        ring_pos_w = ringpos( ring_pos_w+snr_prod, ring_size );

        snr_proc_stretch = snr_prod;
        sndstretch_job ( ring_buff, ring_size, ring_pos_r,
                         snr_stretch_i*channels, snr_stretch_o*channels, channels,
                         buff_o, &snr_prod, snr_proc_stretch, init_me,
                         &(job->stretch_job)
                       );

        ring_pos_r = ringpos( ring_pos_r+snr_prod, ring_size );

    } else {

//        fprintf(stderr,"sndstretch: ring_pos_r = %d\n",ring_pos_r);
//        fprintf(stderr,"sndstretch: ring_pos_w = %d\n\n",ring_pos_w);
        snr_prod = snr_proc;
        if( speed_eff!=1.0 ){   /* add echo only when really stretching */
            if( !vol_corr ){
                ringload_IIR_1_div_e_echo_i( ring_buff, ring_size, ring_pos_w, buff_i, snr_proc, dsnr*channels );
            }else{
                ringload_IIR_1_div_e_echo_i_vc( ring_buff, ring_size, ring_pos_w, buff_i, snr_proc, dsnr*channels );
            }
        } else {
            ringload( ring_buff, ring_size, ring_pos_w, buff_i, snr_proc );
        }
        ring_pos_w = ringpos( ring_pos_w+snr_proc, ring_size );

        snr_proc_stretch = snr_proc;
        sndstretch_job ( ring_buff, ring_size, ring_pos_r,
                         snr_stretch_i*channels, snr_stretch_o*channels, channels,
                         buff_help, &snr_prod, snr_proc_stretch, init_me,
                         &(job->stretch_job)
                       );

        ring_pos_r = ringpos( ring_pos_r+snr_prod, ring_size );

        snr_proc_scale = snr_prod;
//        fprintf(stderr,"snr_scale_i, snr_scale_o = %d,%d\n",snr_scale_i, snr_scale_o);
        sndscale_job ( buff_help,
                       snr_scale_i, snr_scale_o, channels,
                       buff_o, &snr_prod, snr_proc_scale, init_me,
                       &(job->scale_job)
                     );

    }

    *snr_produced = snr_prod;

    return snr_prod;
#undef    ring_buff
#undef    ring_buff_old
#undef    buff_help
#undef    ring_size
#undef    ring_size_old
#undef    ring_pos_w
#undef    ring_pos_r
#undef    snr_scale_i
#undef    snr_scale_o
#undef    snr_stretch_i
#undef    snr_stretch_o
#undef    snr_proc_scale
#undef    snr_proc_stretch
#undef    is_init
#undef    dsnr
#undef    speed_act
#undef    pitch_act
#undef    fade_shift_act
}


int snd_stretch_scale(s16 *buff_i, s16 * buff_o,
                      double pitch, double speed, int channels,
                      int snr_proc, int * snr_produced, int initialize
                     )
{
    return snd_pitch_speed(
                           /* input */
                           buff_i, channels, snr_proc,
                           /* algorihm parameters */
                           initialize, pitch, speed, 1764,
                           /* output */
                           buff_o, snr_produced
                          );
}


int snd_stretch_scale_job(s16 *buff_i, s16 * buff_o,
                      double pitch, double speed, int channels,
                      int snr_proc, int * snr_produced, int initialize,
                      PitchSpeedJob * job, int init_job)
/* init_job should be set to one the first time the routine is called with
   the actual job (e.g. for creating a new job) */
{
    if(init_job){
        InitPitchSpeedJob(job);
    }
    return snd_pitch_speed_job(
                               /* input */
                               buff_i, channels, snr_proc,
                               /* algorihm parameters */
                               initialize, pitch, speed, 1764 ,
                               /* output */
                               buff_o, snr_produced,
                               /* job data */
                               job, 0
                              );
}
