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

#include "track_state.h"

#include <cstring>
#include <pthread.h>

static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static TrackPeaks * g_peaks = nullptr;
static char * g_current_file = nullptr;

void track_state_cleanup()
{
    pthread_mutex_lock(&g_mutex);
    delete[] g_current_file;
    g_current_file = nullptr;
    delete g_peaks;
    g_peaks = nullptr;
    pthread_mutex_unlock(&g_mutex);
}

bool track_state_is_current(const char * filename)
{
    pthread_mutex_lock(&g_mutex);
    bool same = g_current_file && strcmp(filename, g_current_file) == 0;
    pthread_mutex_unlock(&g_mutex);
    return same;
}

void track_state_set(const char * filename, TrackPeaks * peaks)
{
    pthread_mutex_lock(&g_mutex);
    delete[] g_current_file;
    g_current_file = strdup(filename);
    delete g_peaks;
    g_peaks = peaks;
    pthread_mutex_unlock(&g_mutex);
}

void track_state_copy_into(TrackPeaks * out)
{
    pthread_mutex_lock(&g_mutex);
    if (g_peaks && g_peaks->num_buckets > 0)
    {
        out->resize(g_peaks->num_buckets);
        out->global_peak = g_peaks->global_peak;
        out->duration_sec = g_peaks->duration_sec;
        memcpy(out->mins, g_peaks->mins,
               g_peaks->num_buckets * sizeof(int16_t));
        memcpy(out->maxs, g_peaks->maxs,
               g_peaks->num_buckets * sizeof(int16_t));
        memcpy(out->low, g_peaks->low, g_peaks->num_buckets * sizeof(uint8_t));
        memcpy(out->mid, g_peaks->mid, g_peaks->num_buckets * sizeof(uint8_t));
        memcpy(out->high, g_peaks->high,
               g_peaks->num_buckets * sizeof(uint8_t));
    }
    pthread_mutex_unlock(&g_mutex);
}
