/*
 * Copyright (c) 2026 0000xFFFF.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "track_peaks.h"

#include <cstdio>
#include <cstring>
#include <math.h>
#include <vector>

#include <QDir>
#include <QString>

#include <libaudcore/runtime.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

char * cache_path_for(const char * filename)
{
    /* simple FNV-1a hash of the path -> cache filename */
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; filename[i] != '\0'; i++)
    {
        h ^= (unsigned char)filename[i];
        h *= 1099511628211ULL;
    }

    char hex[17];
    snprintf(hex, sizeof hex, "%016llx", (unsigned long long)h);

    const char * user_dir = aud_get_path(AudPath::UserDir);
    int dir_len = strlen(user_dir) + 16;
    char * dir = new char[dir_len];
    snprintf(dir, dir_len, "%s/waveform_cache", user_dir);

    QDir().mkpath(QString::fromUtf8(dir));

    int path_len = strlen(dir) + 24;
    char * path = new char[path_len];
    snprintf(path, path_len, "%s/%s.peaks", dir, hex);

    delete[] dir;
    return path;
}

#define CACHE_MAGIC 0x57434232

bool load_cache(const char * path, TrackPeaks * out)
{
    FILE * f = fopen(path, "rb");
    if (!f)
        return false;

    int32_t magic = 0, n = 0;
    bool ok = fread(&magic, sizeof magic, 1, f) == 1 && magic == CACHE_MAGIC &&
              fread(&n, sizeof n, 1, f) == 1 && n == NUM_BUCKETS;
    if (ok)
    {
        out->resize(n);
        ok = fread(&out->global_peak, sizeof out->global_peak, 1, f) == 1 &&
             fread(&out->duration_sec, sizeof out->duration_sec, 1, f) == 1 &&
             fread(out->mins, sizeof(int16_t), n, f) == (size_t)n &&
             fread(out->maxs, sizeof(int16_t), n, f) == (size_t)n &&
             fread(out->low, sizeof(uint8_t), n, f) == (size_t)n &&
             fread(out->mid, sizeof(uint8_t), n, f) == (size_t)n &&
             fread(out->high, sizeof(uint8_t), n, f) == (size_t)n;
    }
    fclose(f);
    return ok;
}

void save_cache(const char * path, const TrackPeaks * in)
{
    FILE * f = fopen(path, "wb");
    if (!f)
        return;
    int32_t magic = CACHE_MAGIC;
    int32_t n = in->num_buckets;
    fwrite(&magic, sizeof magic, 1, f);
    fwrite(&n, sizeof n, 1, f);
    fwrite(&in->global_peak, sizeof in->global_peak, 1, f);
    fwrite(&in->duration_sec, sizeof in->duration_sec, 1, f);
    fwrite(in->mins, sizeof(int16_t), n, f);
    fwrite(in->maxs, sizeof(int16_t), n, f);
    fwrite(in->low, sizeof(uint8_t), n, f);
    fwrite(in->mid, sizeof(uint8_t), n, f);
    fwrite(in->high, sizeof(uint8_t), n, f);
    fclose(f);
}

namespace
{

struct PumpContext
{
    int16_t * samples;
    size_t samples_count;
    size_t samples_capacity;
    SwrContext * swr;
    int16_t * conv_buf;
    int conv_buf_capacity;
};

void pump_frame(AVFrame * fr, PumpContext * ctx)
{
    int max_out = swr_get_out_samples(ctx->swr, fr->nb_samples);
    if (ctx->conv_buf_capacity < max_out)
    {
        delete[] ctx->conv_buf;
        ctx->conv_buf_capacity = max_out;
        ctx->conv_buf = new int16_t[max_out];
    }
    uint8_t * planes[1] = {(uint8_t *)ctx->conv_buf};
    int got = swr_convert(ctx->swr, planes, max_out, (const uint8_t **)fr->data,
                          fr->nb_samples);
    if (got > 0)
    {
        if (ctx->samples_count + got > ctx->samples_capacity)
        {
            ctx->samples_capacity = (ctx->samples_count + got) * 2;
            int16_t * new_samples = new int16_t[ctx->samples_capacity];
            memcpy(new_samples, ctx->samples,
                   ctx->samples_count * sizeof(int16_t));
            delete[] ctx->samples;
            ctx->samples = new_samples;
        }
        memcpy(ctx->samples + ctx->samples_count, ctx->conv_buf,
               got * sizeof(int16_t));
        ctx->samples_count += got;
    }
}

/* Splits the mono signal into bass/mid/treble via two simple one-pole
 * filters (cheap, stable, no FFT needed -- this is the same order of
 * rigor as the bass/mid/treble split used in most cheap VU-style
 * visualizers, not a precise crossover).
 * For each bucket, records the peak |amplitude| seen in each
 * band, then normalizes against a shared track-wide max so quiet
 * hi-hats show up just as vividly as loud bass. */
void compute_band_peaks(const int16_t * samples, size_t count, int sample_rate,
                        int num_buckets, uint8_t * low_out, uint8_t * mid_out,
                        uint8_t * high_out)
{
    const float fc_low = 250.0f; /* below this = "low" band */
    const float fc_high =
        4000.0f; /* above this = "high" band; in between = "mid" */
    float alpha_low = expf(-2.0f * (float)M_PI * fc_low / sample_rate);
    float alpha_high = expf(-2.0f * (float)M_PI * fc_high / sample_rate);

    std::vector<float> low_peak(num_buckets, 0.0f);
    std::vector<float> mid_peak(num_buckets, 0.0f);
    std::vector<float> high_peak(num_buckets, 0.0f);

    float low_state = 0.0f, high_state = 0.0f, prev_x = 0.0f;

    for (size_t i = 0; i < count; i++)
    {
        float x = (float)samples[i];

        low_state = alpha_low * low_state + (1.0f - alpha_low) * x;
        high_state = alpha_high * (high_state + x - prev_x);
        prev_x = x;

        float low_val = low_state;
        float high_val = high_state;
        float mid_val = x - low_val - high_val;

        int b = (int)((double)i / count * num_buckets);
        if (b >= num_buckets)
            b = num_buckets - 1;

        float la = fabsf(low_val), ma = fabsf(mid_val), ha = fabsf(high_val);
        if (la > low_peak[b])
            low_peak[b] = la;
        if (ma > mid_peak[b])
            mid_peak[b] = ma;
        if (ha > high_peak[b])
            high_peak[b] = ha;
    }

    /* IMPORTANT: normalize against ONE shared max across all three bands,
     * not each band's own max independently. Per-band normalization would
     * make a pure bass tone's tiny mid/high residual energy *also* read
     * as "fully lit" -- which defeats the purpose: we want color to
     * reflect which band actually dominates at a given moment. */
    float shared_max = 1.0f;
    for (int b = 0; b < num_buckets; b++)
    {
        if (low_peak[b] > shared_max)
            shared_max = low_peak[b];
        if (mid_peak[b] > shared_max)
            shared_max = mid_peak[b];
        if (high_peak[b] > shared_max)
            shared_max = high_peak[b];
    }

    for (int b = 0; b < num_buckets; b++)
    {
        low_out[b] = (uint8_t)(255.0f * low_peak[b] / shared_max);
        mid_out[b] = (uint8_t)(255.0f * mid_peak[b] / shared_max);
        high_out[b] = (uint8_t)(255.0f * high_peak[b] / shared_max);
    }
}

} // namespace

bool decode_peaks(const char * filename, TrackPeaks * out)
{
    AVFormatContext * fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filename, nullptr, nullptr) < 0)
        return false;
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    int audio_idx = -1;
    for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_idx = i;
            break;
        }

    if (audio_idx < 0)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecParameters * params = fmt_ctx->streams[audio_idx]->codecpar;
    const AVCodec * codec = avcodec_find_decoder(params->codec_id);
    if (!codec)
    {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVCodecContext * ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(ctx, params);
    if (avcodec_open2(ctx, codec, nullptr) < 0)
    {
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    SwrContext * swr = nullptr;

#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(59, 37, 100)
    /* Modern FFmpeg (>= 5.1) Channel Layout API */
    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, 1);

    if (swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_S16,
                            ctx->sample_rate, &ctx->ch_layout, ctx->sample_fmt,
                            ctx->sample_rate, 0, nullptr) < 0 ||
        !swr || swr_init(swr) < 0)
    {
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }
#else
    /* Legacy FFmpeg (< 5.1) Channel Layout API */
    uint64_t out_layout = AV_CH_LAYOUT_MONO;
    uint64_t in_layout = ctx->channel_layout;

    if (!in_layout)
        in_layout = av_get_default_channel_layout(ctx->channels);

    swr = swr_alloc_set_opts(nullptr, out_layout, AV_SAMPLE_FMT_S16,
                             ctx->sample_rate, in_layout, ctx->sample_fmt,
                             ctx->sample_rate, 0, nullptr);

    if (!swr || swr_init(swr) < 0)
    {
        if (swr)
            swr_free(&swr);
        avcodec_free_context(&ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }
#endif

    PumpContext pctx;
    pctx.samples_capacity = (size_t)ctx->sample_rate * 60 * 4;
    pctx.samples = new int16_t[pctx.samples_capacity];
    pctx.samples_count = 0;
    pctx.swr = swr;
    pctx.conv_buf_capacity = 1024;
    pctx.conv_buf = new int16_t[pctx.conv_buf_capacity];

    AVPacket * pkt = av_packet_alloc();
    AVFrame * frame = av_frame_alloc();

    while (av_read_frame(fmt_ctx, pkt) >= 0)
    {
        if (pkt->stream_index == audio_idx &&
            avcodec_send_packet(ctx, pkt) == 0)
            while (avcodec_receive_frame(ctx, frame) == 0)
                pump_frame(frame, &pctx);
        av_packet_unref(pkt);
    }
    avcodec_send_packet(ctx, nullptr);
    while (avcodec_receive_frame(ctx, frame) == 0)
        pump_frame(frame, &pctx);

    int drain;
    do
    {
        uint8_t * planes[1] = {(uint8_t *)pctx.conv_buf};
        drain = swr_convert(swr, planes, pctx.conv_buf_capacity, nullptr, 0);
        if (drain > 0)
        {
            if (pctx.samples_count + drain > pctx.samples_capacity)
            {
                pctx.samples_capacity = (pctx.samples_count + drain) * 2;
                int16_t * new_samples = new int16_t[pctx.samples_capacity];
                memcpy(new_samples, pctx.samples,
                       pctx.samples_count * sizeof(int16_t));
                delete[] pctx.samples;
                pctx.samples = new_samples;
            }
            memcpy(pctx.samples + pctx.samples_count, pctx.conv_buf,
                   drain * sizeof(int16_t));
            pctx.samples_count += drain;
        }
    } while (drain > 0);

    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr);
    avcodec_free_context(&ctx);
    int sample_rate = fmt_ctx->streams[audio_idx]->codecpar->sample_rate;
    avformat_close_input(&fmt_ctx);

    if (pctx.samples_count == 0)
    {
        delete[] pctx.samples;
        delete[] pctx.conv_buf;
        return false;
    }

    out->resize(NUM_BUCKETS);
    out->global_peak = 1;
    out->duration_sec = (double)pctx.samples_count / sample_rate;

    for (int b = 0; b < NUM_BUCKETS; b++)
    {
        size_t start = (size_t)((double)b / NUM_BUCKETS * pctx.samples_count);
        size_t end =
            (size_t)((double)(b + 1) / NUM_BUCKETS * pctx.samples_count);
        if (end > pctx.samples_count)
            end = pctx.samples_count;
        if (end <= start)
            end = start + 1;
        if (end > pctx.samples_count)
            end = pctx.samples_count;

        int16_t mn = 0, mx = 0;
        for (size_t i = start; i < end; i++)
        {
            if (pctx.samples[i] < mn)
                mn = pctx.samples[i];
            if (pctx.samples[i] > mx)
                mx = pctx.samples[i];
        }
        out->mins[b] = mn;
        out->maxs[b] = mx;
        if (-mn > out->global_peak)
            out->global_peak = -mn;
        if (mx > out->global_peak)
            out->global_peak = mx;
    }

    compute_band_peaks(pctx.samples, pctx.samples_count, sample_rate,
                       NUM_BUCKETS, out->low, out->mid, out->high);

    delete[] pctx.samples;
    delete[] pctx.conv_buf;
    return true;
}
