/*
 * MacMediaKeys.mm
 * Copyright 2014-2015 Michał Lipski
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

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/plugin.h>

#import "SPMediaKeyTap.h"

@interface MacMediaKeys : NSObject {
    SPMediaKeyTap * keyTap;
}
- (void)cleanup;
@end

@implementation MacMediaKeys
- (id)init
{
    keyTap = [[SPMediaKeyTap alloc] initWithDelegate:self];

    // Register defaults for the whitelist of apps that want to use media keys
    [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
        [SPMediaKeyTap defaultMediaKeyUserBundleIdentifiers], kMediaKeyUsingBundleIdentifiersDefaultsKey,
    nil]];

    if ([SPMediaKeyTap usesGlobalMediaKeyTap])
        [keyTap startWatchingMediaKeys];

    return self;
}

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

- (void)cleanup;
{
    if (keyTap)
    {
        [keyTap stopWatchingMediaKeys];
        [keyTap release];
    }
}
@end

class MacMediaKeysPlugin : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Mac Media Keys"),
        PACKAGE
    };

    constexpr MacMediaKeysPlugin () : GeneralPlugin (info, false) {}

    bool init ();
    void cleanup ();

private:
    MacMediaKeys * m_obj = nullptr;
};

EXPORT MacMediaKeysPlugin aud_plugin_instance;

bool MacMediaKeysPlugin::init ()
{
    m_obj = [[MacMediaKeys alloc] init];
    return true;
}

void MacMediaKeysPlugin::cleanup ()
{
    [m_obj cleanup];
    [m_obj release];
    m_obj = nullptr;
}
