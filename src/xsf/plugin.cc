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
#include <libaudcore/preferences.h>
#include <libaudcore/audstrings.h>
#include <libaudcore/runtime.h>

#include "ao.h"
#include "corlett.h"
#include "vio2sf.h"

class XSFPlugin : public InputPlugin
{
public:
	static const char *const exts[];
	static const char *const defaults[];
	static const PreferencesWidget widgets[];
	static const PluginPreferences prefs;

	static constexpr PluginInfo info = {
		N_("2SF Decoder"),
		PACKAGE,
		nullptr,
		& prefs
	};

	constexpr XSFPlugin() : InputPlugin(info, InputInfo()
		.with_exts(exts)) {}

	bool init();

	bool is_our_file(const char *filename, VFSFile &file);
	bool read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image);
	bool play(const char *filename, VFSFile &file);
};

EXPORT XSFPlugin aud_plugin_instance;

/* xsf_get_lib: called to load secondary files */
static String dirpath;

#define CFG_ID "xsf"

const char* const XSFPlugin::defaults[] =
{
	"ignore_length", "FALSE",
	nullptr
};

bool XSFPlugin::init()
{
	aud_config_set_defaults(CFG_ID, defaults);
	return true;
}

Index<char> xsf_get_lib(char *filename)
{
	VFSFile file(filename_build({dirpath, filename}), "r");
	return file ? file.read_all() : Index<char>();
}

bool XSFPlugin::read_tag(const char *filename, VFSFile &file, Tuple &tuple, Index<char> *image)
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
	tuple.set_str(Tuple::Codec, "GBA/Nintendo DS Audio");

	free(c);

	return true;
}

static int xsf_get_length(const Index<char> &buf)
{
	corlett_t *c;

	if (corlett_decode((uint8_t *)buf.begin(), buf.len(), nullptr, nullptr, &c) != AO_SUCCESS)
		return -1;

	bool ignore_length = aud_get_bool(CFG_ID, "ignore_length");
	int length = (!ignore_length) ? psfTimeToMS(c->inf_length) + psfTimeToMS(c->inf_fade) : -1;

	free(c);

	return length;
}

bool XSFPlugin::play(const char *filename, VFSFile &file)
{
	int length = -1;
	int16_t samples[44100*2];
	int seglen = 44100 / 60;
	float pos = 0.0;
	bool error = false;

	const char * slash = strrchr (filename, '/');
	if (! slash)
		return false;

	dirpath = String (str_copy (filename, slash + 1 - filename));

	Index<char> buf = file.read_all ();

	if (!buf.len())
	{
		error = true;
		goto ERR_NO_CLOSE;
	}

	length = xsf_get_length(buf);

	if (xsf_start(buf.begin(), buf.len()) != AO_SUCCESS)
	{
		error = true;
		goto ERR_NO_CLOSE;
	}

	set_stream_bitrate(44100*2*2*8);
	open_audio(FMT_S16_NE, 44100, 2);

	while (! check_stop ())
	{
		int seek_value = check_seek ();

		if (seek_value >= 0)
		{
			if (seek_value > pos)
			{
				while (pos < seek_value)
				{
					xsf_gen(samples, seglen);
					pos += 16.666;
				}
			}
			else if (seek_value < pos)
			{
				xsf_term();

				if (xsf_start(buf.begin(), buf.len()) == AO_SUCCESS)
				{
					pos = 0.0;
					while (pos < seek_value)
					{
						xsf_gen(samples, seglen);
						pos += 16.666; /* each segment is 16.666ms */
					}
				}
			   	else
				{
					error = true;
					goto CLEANUP;
				}
			}
		}

		xsf_gen(samples, seglen);
		pos += 16.666;

		write_audio(samples, seglen * 4);

		bool ignore_length = aud_get_bool(CFG_ID, "ignore_length");
		if (pos >= length && !ignore_length)
			goto CLEANUP;
	}

CLEANUP:
	xsf_term();

ERR_NO_CLOSE:
	dirpath = String ();

	return !error;
}

bool XSFPlugin::is_our_file(const char *filename, VFSFile &file)
{
	char magic[4];
	if (file.fread (magic, 1, 4) < 4)
		return false;

	if (!memcmp(magic, "PSF$", 4))
		return 1;

	return 0;
}

const char *const XSFPlugin::exts[] = { "2sf", "mini2sf", nullptr };

const PreferencesWidget XSFPlugin::widgets[] = {
	WidgetLabel(N_("<b>XSF Configuration</b>")),
	WidgetCheck(N_("Ignore length from file"), WidgetBool(CFG_ID, "ignore_length")),
};

const PluginPreferences XSFPlugin::prefs = {{widgets}};
