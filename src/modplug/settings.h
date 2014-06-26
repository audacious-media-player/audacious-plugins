#ifndef MODPLUG_SETTINGS_H
#define MODPLUG_SETTINGS_H

#include <libaudcore/core.h>

typedef struct {
    int mBits;
    int mChannels;
    int mResamplingMode;
    int mFrequency;

    bool mReverb;
    int mReverbDepth;
    int mReverbDelay;

    bool mMegabass;
    int mBassAmount;
    int mBassRange;

    bool mSurround;
    int mSurroundDepth;
    int mSurroundDelay;

    bool mPreamp;
    float mPreampLevel;

    bool mOversamp;
    bool mNoiseReduction;
    bool mGrabAmigaMOD;
    int mLoopCount;
} ModplugSettings;

#endif /* MODPLUG_SETTINGS_H */
