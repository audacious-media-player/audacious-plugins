#ifndef MODPLUG_SETTINGS_H
#define MODPLUG_SETTINGS_H

#include <libaudcore/core.h>

typedef struct {
    int mBits;
    int mChannels;
    int mResamplingMode;
    int mFrequency;

    bool_t mReverb;
    int mReverbDepth;
    int mReverbDelay;

    bool_t mMegabass;
    int mBassAmount;
    int mBassRange;

    bool_t mSurround;
    int mSurroundDepth;
    int mSurroundDelay;

    bool_t mPreamp;
    float mPreampLevel;

    bool_t mOversamp;
    bool_t mNoiseReduction;
    bool_t mGrabAmigaMOD;
    int mLoopCount;
} ModplugSettings;

#endif /* MODPLUG_SETTINGS_H */
