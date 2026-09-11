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

#ifndef __PRELOAD_H__
#define __PRELOAD_H__

#include <atomic>

#include "track_peaks.h"

/* Progress counters, polled by the controls widget's status label. */
extern std::atomic<bool> g_preload_running;
extern std::atomic<int> g_preload_total;
extern std::atomic<int> g_preload_done;

/* Loads peaks for `filename` from cache if possible, otherwise decodes
 * and writes the cache. If `out` is non-null, the resulting peak data
 * is copied into it; otherwise it is decoded/loaded and then discarded.
 * Shared by the bulk preloader and the single-track async loader. */
bool preload_one_file(const char * filename, TrackPeaks * out = nullptr);

/* Kicks off a background preload of every entry in the active
 * playlist using a pool of worker threads sized to the number of CPU
 * cores available. Safe to call repeatedly; a second call while one
 * is already running is ignored. */
void run_playlist_preload();

#endif
