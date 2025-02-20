/*
 * Copyright (c) 2010-2019 Ariadne Conill <ariadne@dereferenced.org>
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

#ifndef AUDACIOUS_LYRICS_PREFERENCES_H
#define AUDACIOUS_LYRICS_PREFERENCES_H

#include <libaudcore/i18n.h>
#include <libaudcore/preferences.h>

// Use former plugin name to stay backwards compatible
static constexpr const char * CFG_SECTION = "lyricwiki";

static const char * const defaults[] = {
    "remote-source", "lyrics.ovh",
    "enable-file-provider", "TRUE",
    "enable-cache", "TRUE",
    "split-title-on-chars", "FALSE",
    "split-on-chars", "-",
    "truncate-fields-on-chars", "FALSE",
    "truncate-on-chars", "|",
    "use-embedded", "TRUE",
    nullptr
};

static const ComboItem remote_sources[] = {
    ComboItem (N_("Nowhere"), "nowhere"),
    ComboItem ("chartlyrics.com", "chartlyrics.com"),
    ComboItem ("lrclib.net", "lrclib.net"),
    ComboItem ("lyrics.ovh", "lyrics.ovh")
};

static const PreferencesWidget truncate_elements[] = {
    WidgetLabel (N_("<small>Artist is truncated at the start, Title -- at the end</small>")),
    WidgetEntry (N_("Chars to truncate on:"), WidgetString (CFG_SECTION, "truncate-on-chars"))
};

static const PreferencesWidget split_elements[] = {
    WidgetLabel (N_("<small>Chars are ORed in RegExp, surrounded by whitespace</small>")),
    WidgetEntry (N_("Chars to split on:"), WidgetString (CFG_SECTION, "split-on-chars")),
    WidgetCheck (N_("Further truncate those on chars"),
        WidgetBool (CFG_SECTION, "truncate-fields-on-chars")),
    WidgetTable ({{truncate_elements}}, WIDGET_CHILD)
};

static const PreferencesWidget widgets[] = {
    WidgetLabel (N_("<b>General</b>")),
    WidgetCheck (N_("Split title into artist and title on chars"),
        WidgetBool (CFG_SECTION, "split-title-on-chars")),
    WidgetTable ({{split_elements}}, WIDGET_CHILD),
    WidgetLabel (N_("<b>Sources</b>")),
    WidgetCheck (N_("Use embedded lyrics (from Lyrics tag)"),
        WidgetBool (CFG_SECTION, "use-embedded")),
    WidgetCombo (N_("Fetch lyrics from internet:"),
        WidgetString (CFG_SECTION, "remote-source"),
        {{remote_sources}}),
    WidgetCheck (N_("Store fetched lyrics in local cache"),
        WidgetBool (CFG_SECTION, "enable-cache")),
    WidgetLabel (N_("<b>Local Storage</b>")),
    WidgetCheck (N_("Load lyric files (.lrc) from local storage"),
        WidgetBool (CFG_SECTION, "enable-file-provider"))
};

#endif // AUDACIOUS_LYRICS_PREFERENCES_H
