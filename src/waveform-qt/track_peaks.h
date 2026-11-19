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

#ifndef __TRACK_PEAKS_H__
#define __TRACK_PEAKS_H__

#include <cstdint>

#define NUM_BUCKETS 2000

/* per-bucket band intensities, 0-255, normalized against a shared max
 * across all three bands (see compute_band_peaks in the .cpp for why
 * shared rather than per-band normalization is used) */
struct TrackPeaks
{
    int16_t * mins;
    int16_t * maxs;
    uint8_t * low;  /* bass energy,  -> red channel   */
    uint8_t * mid;  /* mid energy,   -> green channel */
    uint8_t * high; /* treble energy -> blue channel  */
    int32_t num_buckets;
    int32_t global_peak;
    double duration_sec;

    TrackPeaks()
        : mins(nullptr), maxs(nullptr), low(nullptr), mid(nullptr),
          high(nullptr), num_buckets(0), global_peak(1), duration_sec(0)
    {
    }

    ~TrackPeaks()
    {
        delete[] mins;
        delete[] maxs;
        delete[] low;
        delete[] mid;
        delete[] high;
    }

    void resize(int32_t n)
    {
        delete[] mins;
        delete[] maxs;
        delete[] low;
        delete[] mid;
        delete[] high;
        num_buckets = n;
        mins = new int16_t[n]();
        maxs = new int16_t[n]();
        low = new uint8_t[n]();
        mid = new uint8_t[n]();
        high = new uint8_t[n]();
    }
};

/* Decodes the given audio file into a fresh peak envelope. `filename`
 * must already be a local filesystem path (callers resolve URIs via
 * uri_to_filename() before calling this). Returns false on failure. */
bool decode_peaks(const char * filename, TrackPeaks * out);

/* Returns the on-disk cache path for `filename`. Caller owns the
 * returned buffer and must delete[] it. */
char * cache_path_for(const char * filename);

bool load_cache(const char * path, TrackPeaks * out);
void save_cache(const char * path, const TrackPeaks * in);

#endif
