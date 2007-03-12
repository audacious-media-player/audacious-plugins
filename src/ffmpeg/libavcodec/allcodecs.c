/*
 * Utils for libavcodec
 * Copyright (c) 2002 Fabrice Bellard.
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file allcodecs.c
 * Utils for libavcodec.
 */

#include "avcodec.h"

/* If you do not call this function, then you can select exactly which
   formats you want to support */

/**
 * simple call to register all the codecs.
 */
void avcodec_register_all(void)
{
    static int inited = 0;

    if (inited != 0)
        return;
    inited = 1;

#ifdef CONFIG_SHORTEN_DECODER
    register_avcodec(&shorten_decoder);
#endif //CONFIG_SHORTEN_DECODER
#ifdef CONFIG_ALAC_DECODER
    register_avcodec(&alac_decoder);
#endif //CONFIG_ALAC_DECODER
#ifdef CONFIG_WS_SND1_DECODER
    register_avcodec(&ws_snd1_decoder);
#endif //CONFIG_WS_SND1_DECODER
#ifdef CONFIG_COOK_DECODER
    register_avcodec(&cook_decoder);
#endif //CONFIG_COOK_DECODER
#ifdef CONFIG_TRUESPEECH_DECODER
    register_avcodec(&truespeech_decoder);
#endif //CONFIG_TRUESPEECH_DECODER
#ifdef CONFIG_TTA_DECODER
    register_avcodec(&tta_decoder);
#endif //CONFIG_TTA_DECODER
#ifdef CONFIG_WAVPACK_DECODER
    register_avcodec(&wavpack_decoder);
#endif //CONFIG_WAVPACK_DECODER

    /* pcm codecs */
#ifdef CONFIG_PCM_S32LE_DECODER
    register_avcodec(&pcm_s32le_decoder);
#endif
#ifdef CONFIG_PCM_S32LE_ENCODER
    register_avcodec(&pcm_s32le_encoder);
#endif
#ifdef CONFIG_PCM_S32BE_DECODER
    register_avcodec(&pcm_s32be_decoder);
#endif
#ifdef CONFIG_PCM_S32BE_ENCODER
    register_avcodec(&pcm_s32be_encoder);
#endif
#ifdef CONFIG_PCM_U32LE_DECODER
    register_avcodec(&pcm_u32le_decoder);
#endif
#ifdef CONFIG_PCM_U32LE_ENCODER
    register_avcodec(&pcm_u32le_encoder);
#endif
#ifdef CONFIG_PCM_U32BE_DECODER
    register_avcodec(&pcm_u32be_decoder);
#endif
#ifdef CONFIG_PCM_U32BE_ENCODER
    register_avcodec(&pcm_u32be_encoder);
#endif
#ifdef CONFIG_PCM_S24LE_DECODER
    register_avcodec(&pcm_s24le_decoder);
#endif
#ifdef CONFIG_PCM_S24LE_ENCODER
    register_avcodec(&pcm_s24le_encoder);
#endif
#ifdef CONFIG_PCM_S24BE_DECODER
    register_avcodec(&pcm_s24be_decoder);
#endif
#ifdef CONFIG_PCM_S24BE_ENCODER
    register_avcodec(&pcm_s24be_encoder);
#endif
#ifdef CONFIG_PCM_U24LE_DECODER
    register_avcodec(&pcm_u24le_decoder);
#endif
#ifdef CONFIG_PCM_U24LE_ENCODER
    register_avcodec(&pcm_u24le_encoder);
#endif
#ifdef CONFIG_PCM_U24BE_DECODER
    register_avcodec(&pcm_u24be_decoder);
#endif
#ifdef CONFIG_PCM_U24BE_ENCODER
    register_avcodec(&pcm_u24be_encoder);
#endif
#ifdef CONFIG_PCM_S24DAUD_DECODER
    register_avcodec(&pcm_s24daud_decoder);
#endif
#ifdef CONFIG_PCM_S24DAUD_ENCODER
    register_avcodec(&pcm_s24daud_encoder);
#endif
#ifdef CONFIG_PCM_S16LE_DECODER
    register_avcodec(&pcm_s16le_decoder);
#endif
#ifdef CONFIG_PCM_S16LE_ENCODER
    register_avcodec(&pcm_s16le_encoder);
#endif
#ifdef CONFIG_PCM_S16BE_DECODER
    register_avcodec(&pcm_s16be_decoder);
#endif
#ifdef CONFIG_PCM_S16BE_ENCODER
    register_avcodec(&pcm_s16be_encoder);
#endif
#ifdef CONFIG_PCM_U16LE_DECODER
    register_avcodec(&pcm_u16le_decoder);
#endif
#ifdef CONFIG_PCM_U16LE_ENCODER
    register_avcodec(&pcm_u16le_encoder);
#endif
#ifdef CONFIG_PCM_U16BE_DECODER
    register_avcodec(&pcm_u16be_decoder);
#endif
#ifdef CONFIG_PCM_U16BE_ENCODER
    register_avcodec(&pcm_u16be_encoder);
#endif
#ifdef CONFIG_PCM_S8_DECODER
    register_avcodec(&pcm_s8_decoder);
#endif
#ifdef CONFIG_PCM_S8_ENCODER
    register_avcodec(&pcm_s8_encoder);
#endif
#ifdef CONFIG_PCM_U8_DECODER
    register_avcodec(&pcm_u8_decoder);
#endif
#ifdef CONFIG_PCM_U8_ENCODER
    register_avcodec(&pcm_u8_encoder);
#endif
#ifdef CONFIG_PCM_ALAW_DECODER
    register_avcodec(&pcm_alaw_decoder);
#endif
#ifdef CONFIG_PCM_ALAW_ENCODER
    register_avcodec(&pcm_alaw_encoder);
#endif
#ifdef CONFIG_PCM_MULAW_DECODER
    register_avcodec(&pcm_mulaw_decoder);
#endif
#ifdef CONFIG_PCM_MULAW_ENCODER
    register_avcodec(&pcm_mulaw_encoder);
#endif

   /* adpcm codecs */
#ifdef CONFIG_ADPCM_IMA_QT_DECODER
    register_avcodec(&adpcm_ima_qt_decoder);
#endif
#ifdef CONFIG_ADPCM_IMA_QT_ENCODER
    register_avcodec(&adpcm_ima_qt_encoder);
#endif
#ifdef CONFIG_ADPCM_IMA_WAV_DECODER
    register_avcodec(&adpcm_ima_wav_decoder);
#endif
#ifdef CONFIG_ADPCM_IMA_WAV_ENCODER
    register_avcodec(&adpcm_ima_wav_encoder);
#endif
#ifdef CONFIG_ADPCM_IMA_DK3_DECODER
    register_avcodec(&adpcm_ima_dk3_decoder);
#endif
#ifdef CONFIG_ADPCM_IMA_DK3_ENCODER
    register_avcodec(&adpcm_ima_dk3_encoder);
#endif
#ifdef CONFIG_ADPCM_IMA_DK4_DECODER
    register_avcodec(&adpcm_ima_dk4_decoder);
#endif
#ifdef CONFIG_ADPCM_IMA_DK4_ENCODER
    register_avcodec(&adpcm_ima_dk4_encoder);
#endif
#ifdef CONFIG_ADPCM_IMA_WS_DECODER
    register_avcodec(&adpcm_ima_ws_decoder);
#endif
#ifdef CONFIG_ADPCM_IMA_WS_ENCODER
    register_avcodec(&adpcm_ima_ws_encoder);
#endif
#ifdef CONFIG_ADPCM_IMA_SMJPEG_DECODER
    register_avcodec(&adpcm_ima_smjpeg_decoder);
#endif
#ifdef CONFIG_ADPCM_IMA_SMJPEG_ENCODER
    register_avcodec(&adpcm_ima_smjpeg_encoder);
#endif
#ifdef CONFIG_ADPCM_MS_DECODER
    register_avcodec(&adpcm_ms_decoder);
#endif
#ifdef CONFIG_ADPCM_MS_ENCODER
    register_avcodec(&adpcm_ms_encoder);
#endif
#ifdef CONFIG_ADPCM_4XM_DECODER
    register_avcodec(&adpcm_4xm_decoder);
#endif
#ifdef CONFIG_ADPCM_4XM_ENCODER
    register_avcodec(&adpcm_4xm_encoder);
#endif
#ifdef CONFIG_ADPCM_XA_DECODER
    register_avcodec(&adpcm_xa_decoder);
#endif
#ifdef CONFIG_ADPCM_XA_ENCODER
    register_avcodec(&adpcm_xa_encoder);
#endif
#ifdef CONFIG_ADPCM_ADX_DECODER
    register_avcodec(&adpcm_adx_decoder);
#endif
#ifdef CONFIG_ADPCM_ADX_ENCODER
    register_avcodec(&adpcm_adx_encoder);
#endif
#ifdef CONFIG_ADPCM_EA_DECODER
    register_avcodec(&adpcm_ea_decoder);
#endif
#ifdef CONFIG_ADPCM_EA_ENCODER
    register_avcodec(&adpcm_ea_encoder);
#endif
#ifdef CONFIG_ADPCM_G726_DECODER
    register_avcodec(&adpcm_g726_decoder);
#endif
#ifdef CONFIG_ADPCM_G726_ENCODER
    register_avcodec(&adpcm_g726_encoder);
#endif
#ifdef CONFIG_ADPCM_CT_DECODER
    register_avcodec(&adpcm_ct_decoder);
#endif
#ifdef CONFIG_ADPCM_CT_ENCODER
    register_avcodec(&adpcm_ct_encoder);
#endif
#ifdef CONFIG_ADPCM_SWF_DECODER
    register_avcodec(&adpcm_swf_decoder);
#endif
#ifdef CONFIG_ADPCM_SWF_ENCODER
    register_avcodec(&adpcm_swf_encoder);
#endif
#ifdef CONFIG_ADPCM_YAMAHA_DECODER
    register_avcodec(&adpcm_yamaha_decoder);
#endif
#ifdef CONFIG_ADPCM_YAMAHA_ENCODER
    register_avcodec(&adpcm_yamaha_encoder);
#endif
#ifdef CONFIG_ADPCM_SBPRO_4_DECODER
    register_avcodec(&adpcm_sbpro_4_decoder);
#endif
#ifdef CONFIG_ADPCM_SBPRO_4_ENCODER
    register_avcodec(&adpcm_sbpro_4_encoder);
#endif
#ifdef CONFIG_ADPCM_SBPRO_3_DECODER
    register_avcodec(&adpcm_sbpro_3_decoder);
#endif
#ifdef CONFIG_ADPCM_SBPRO_3_ENCODER
    register_avcodec(&adpcm_sbpro_3_encoder);
#endif
#ifdef CONFIG_ADPCM_SBPRO_2_DECODER
    register_avcodec(&adpcm_sbpro_2_decoder);
#endif
#ifdef CONFIG_ADPCM_SBPRO_2_ENCODER
    register_avcodec(&adpcm_sbpro_2_encoder);
#endif

    /* parsers */

    av_register_bitstream_filter(&dump_extradata_bsf);
    av_register_bitstream_filter(&remove_extradata_bsf);
    av_register_bitstream_filter(&noise_bsf);
}

