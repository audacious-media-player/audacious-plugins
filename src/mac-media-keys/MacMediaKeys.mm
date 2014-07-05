/*
 * MacMediaKeys.mm
 * Copyright 2014 Michał Lipski
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <thread>

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

#import "MacMediaKeys.h"

static MacMediaKeys * media;

@implementation SPMediaKeyTapAppDelegate
- (void)mediaKeyTap:(SPMediaKeyTap *)keyTap receivedMediaKeyEvent:(NSEvent *)event;
{
    assert([event type] == NSSystemDefined && [event subtype] == SPSystemDefinedEventMediaKeys);

    int keyCode = (([event data1] & 0xFFFF0000) >> 16);
    int keyFlags = ([event data1] & 0x0000FFFF);
    int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
    // int keyRepeat = (keyFlags & 0x1);

    if (keyState == 1)
    {
        switch (keyCode)
        {
            case NX_KEYTYPE_PLAY:
                aud_drct_play_pause ();
                return;

            case NX_KEYTYPE_FAST:
            case NX_KEYTYPE_NEXT:
                aud_drct_pl_next ();
                return;

            case NX_KEYTYPE_REWIND:
            case NX_KEYTYPE_PREVIOUS:
                aud_drct_pl_prev ();
                return;
        }
    }
}
@end

MacMediaKeys::MacMediaKeys ()
{
    std::thread thread (&MacMediaKeys::run, this);
    thread.detach ();
}

MacMediaKeys::~MacMediaKeys ()
{
    [keyTap stopWatchingMediaKeys];
}

void MacMediaKeys::run ()
{
    // Register defaults for the whitelist of apps that want to use media keys
    [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
        [SPMediaKeyTap defaultMediaKeyUserBundleIdentifiers], kMediaKeyUsingBundleIdentifiersDefaultsKey,
    nil]];

    delegate = [SPMediaKeyTapAppDelegate alloc];
    keyTap   = [[SPMediaKeyTap alloc] initWithDelegate: delegate];

    if ([SPMediaKeyTap usesGlobalMediaKeyTap])
        [keyTap startWatchingMediaKeys];
}

bool mac_media_keys_init ()
{
    media = new MacMediaKeys;
    return true;
}

void mac_media_keys_cleanup ()
{
    delete media;
}

#define AUD_PLUGIN_NAME        N_("Mac Media Keys")
#define AUD_PLUGIN_INIT        mac_media_keys_init
#define AUD_PLUGIN_CLEANUP     mac_media_keys_cleanup

#define AUD_DECLARE_GENERAL
#include <libaudcore/plugin-declare.h>
