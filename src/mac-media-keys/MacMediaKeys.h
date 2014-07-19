/*
 * MacMediaKeys.h
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

#ifndef MAC_MEDIA_KEYS_H
#define MAC_MEDIA_KEYS_H

#import "SPMediaKeyTap.h"

@interface SPMediaKeyTapAppDelegate : NSObject <NSApplicationDelegate>
@end

class MacMediaKeys
{
public:
    MacMediaKeys ();
    ~MacMediaKeys ();
    SPMediaKeyTapAppDelegate * delegate;
    SPMediaKeyTap * keyTap;

private:
    void run ();
};

#endif
