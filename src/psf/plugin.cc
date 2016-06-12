/*
    Audio Overload SDK - main driver.  for demonstration only, not user friendly!

    Copyright (c) 2007-2008 R. Belmont and Richard Bannister.

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
    * Neither the names of R. Belmont and Richard Bannister nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>
#include <libaudcore/audstrings.h>

#include "ao.h"
#include "corlett.h"
#include "eng_protos.h"

class PSFPlugin : public InputPlugin
{
public:
    static const char *const exts[];

    static constexpr PluginInfo info = {
        N_("OpenPSF PSF1/PSF2 Decoder"),
        PACKAGE
    };

    constexpr PSFPlugin() : InputPlugin(info, InputInfo()
        .with_exts(exts)) {}

    bool is_our_file(const char *filename, VFSFile &file);
    bool read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image);
    bool play(const char *filename, VFSFile &file);

protected:
    static void update(const void *data, int bytes);
};

EXPORT PSFPlugin aud_plugin_instance;

typedef enum {
    ENG_NONE = 0,
    ENG_PSF1,
    ENG_PSF2,
    ENG_SPX,
    ENG_COUNT
} PSFEngine;

typedef struct {
    int32_t (*start)(uint8_t *buffer, uint32_t length);
    int32_t (*stop)(void);
    int32_t (*seek)(uint32_t);
    int32_t (*execute)(void (*update)(const void *, int));
} PSFEngineFunctors;

static PSFEngineFunctors psf_functor_map[ENG_COUNT] = {
    {nullptr, nullptr, nullptr, nullptr},
    {psf_start, psf_stop, psf_seek, psf_execute},
    {psf2_start, psf2_stop, psf2_seek, psf2_execute},
    {spx_start, spx_stop, psf_seek, spx_execute},
};

static PSFEngineFunctors *f;
static String dirpath;

bool stop_flag = false;

/* The emulation engine can only seek forward, not back.  This variable is set
 * a non-negative time (milliseconds) when the song is to be restarted in order
 * to seek backward. */
static int reverse_seek;

static PSFEngine psf_probe(const char *buf, int len)
{
    if (len < 4)
        return ENG_NONE;

    if (!memcmp(buf, "PSF\x01", 4))
        return ENG_PSF1;

    if (!memcmp(buf, "PSF\x02", 4))
        return ENG_PSF2;

    if (!memcmp(buf, "SPU", 3))
        return ENG_SPX;

    if (!memcmp(buf, "SPX", 3))
        return ENG_SPX;

    return ENG_NONE;
}

/* ao_get_lib: called to load secondary files */
Index<char> ao_get_lib(char *filename)
{
    VFSFile file(filename_build({dirpath, filename}), "r");
    return file ? file.read_all() : Index<char>();
}

bool PSFPlugin::read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image)
{
    Index<char> buf = file.read_all ();
    if (!buf.len())
        return false;

    corlett_t *c;
    if (corlett_decode((uint8_t *)buf.begin(), buf.len(), nullptr, nullptr, &c) != AO_SUCCESS)
        return false;

    tuple.set_int(Tuple::Length, psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade));
    tuple.set_str(Tuple::Artist, c->inf_artist);
    tuple.set_str(Tuple::Album, c->inf_game);
    tuple.set_str(Tuple::Title, c->inf_title);
    tuple.set_str(Tuple::Copyright, c->inf_copy);
    tuple.set_str(Tuple::Quality, _("sequenced"));
    tuple.set_str(Tuple::Codec, "PlayStation 1/2 Audio");

    free(c);

    return true;
}

bool PSFPlugin::play(const char *filename, VFSFile &file)
{
    bool error = false;

    const char * slash = strrchr (filename, '/');
    if (! slash)
        return false;

    dirpath = String (str_copy (filename, slash + 1 - filename));

    Index<char> buf = file.read_all ();

    PSFEngine eng = psf_probe(buf.begin(), buf.len());
    if (eng == ENG_NONE || eng == ENG_COUNT)
    {
        error = true;
        goto cleanup;
    }

    f = &psf_functor_map[eng];

    set_stream_bitrate(44100*2*2*8);
    open_audio(FMT_S16_NE, 44100, 2);

    reverse_seek = -1;

    /* This loop will restart playback from the beginning when necessary to seek
     * backwards in the file (reverse_seek >= 0). */
    do
    {
        if (f->start((uint8_t *)buf.begin(), buf.len()) != AO_SUCCESS)
        {
            error = true;
            goto cleanup;
        }

        if (reverse_seek >= 0)
        {
            f->seek(reverse_seek); /* should never fail here */
            reverse_seek = -1;
        }

        stop_flag = false;

        f->execute(update);
        f->stop();
    }
    while (reverse_seek >= 0);

cleanup:
    f = nullptr;
    dirpath = String ();

    return ! error;
}

void PSFPlugin::update(const void *data, int bytes)
{
    if (!data || check_stop())
    {
        stop_flag = true;
        return;
    }

    int seek = check_seek();

    if (seek >= 0)
    {
        if (!f->seek(seek))
        {
            reverse_seek = seek;
            stop_flag = true;
        }

        return;
    }

    write_audio(data, bytes);
}

bool PSFPlugin::is_our_file(const char *filename, VFSFile &file)
{
    char magic[4];
    if (file.fread (magic, 1, 4) < 4)
        return false;

    return (psf_probe(magic, 4) != ENG_NONE);
}

const char *const PSFPlugin::exts[] = { "psf", "minipsf", "psf2", "minipsf2", "spu", "spx", nullptr };
