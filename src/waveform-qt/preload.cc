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

#include "preload.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#include <libaudcore/audstrings.h>
#include <libaudcore/playlist.h>

std::atomic<bool> g_preload_running{false};
std::atomic<int> g_preload_total{0};
std::atomic<int> g_preload_done{0};

bool preload_one_file(const char * filename, TrackPeaks * out)
{
    TrackPeaks local;
    TrackPeaks * peaks = out ? out : &local;

    char * cpath = cache_path_for(filename);

    bool ok = load_cache(cpath, peaks);
    if (!ok)
    {
        StringBuf local_path = uri_to_filename(filename);
        const char * decode_path =
            local_path ? (const char *)local_path : filename;

        ok = decode_peaks(decode_path, peaks);
        if (ok)
            save_cache(cpath, peaks);
    }

    delete[] cpath;
    return ok;
}

/* Filenames are snapshotted on the calling (UI) thread -- Playlist's
 * API is not meant to be called from worker threads -- then handed off
 * to the worker pool below, so a beefier machine churns through the
 * playlist proportionally faster. */
void run_playlist_preload()
{
    if (g_preload_running.exchange(true))
        return; /* already running */

    Playlist pl = Playlist::active_playlist();
    int n = pl.n_entries();

    g_preload_total = n;
    g_preload_done = 0;

    if (n <= 0)
    {
        g_preload_running = false;
        return;
    }

    auto * filenames = new std::vector<char *>();
    filenames->reserve(n);
    for (int i = 0; i < n; i++)
    {
        String fn = pl.entry_filename(i);
        filenames->push_back(strdup(fn ? (const char *)fn : ""));
    }

    std::thread([filenames]() {
        unsigned n_threads = std::thread::hardware_concurrency();
        if (n_threads == 0)
            n_threads = 2; /* hardware_concurrency() can return 0; fall back */

        size_t total = filenames->size();
        if (n_threads > total)
            n_threads = (unsigned)total;

        std::atomic<size_t> next_idx{0};

        std::vector<std::thread> workers;
        workers.reserve(n_threads);
        for (unsigned t = 0; t < n_threads; t++)
        {
            workers.emplace_back([filenames, &next_idx, total]() {
                for (;;)
                {
                    size_t idx = next_idx.fetch_add(1);
                    if (idx >= total)
                        break;

                    const char * fn = (*filenames)[idx];
                    if (fn && fn[0])
                        preload_one_file(fn);

                    g_preload_done.fetch_add(1);
                }
            });
        }

        for (auto & w : workers)
            w.join();

        for (char * fn : *filenames)
            free(fn);
        delete filenames;

        g_preload_running = false;
    }).detach();
}
