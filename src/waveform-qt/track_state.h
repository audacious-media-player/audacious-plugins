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

#ifndef __TRACK_STATE_H__
#define __TRACK_STATE_H__

#include "track_peaks.h"

/* Thread-safe storage for the peak envelope of the currently displayed
 * track. Callers never touch a mutex directly -- everything goes
 * through these functions. */

void track_state_cleanup();

/* True if `filename` matches the file currently stored. */
bool track_state_is_current(const char * filename);

/* Takes ownership of `peaks`, replacing whatever was stored before. */
void track_state_set(const char * filename, TrackPeaks * peaks);

/* Deep-copies the current peaks into `out`. If nothing is loaded,
 * out->num_buckets is left at 0. */
void track_state_copy_into(TrackPeaks * out);

#endif
