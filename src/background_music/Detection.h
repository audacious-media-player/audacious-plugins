#ifndef AUDACIOUS_PLUGINS_BGM_DETECTION_H
#define AUDACIOUS_PLUGINS_BGM_DETECTION_H
/*
 * Detection framework.
 * Copyright 2023 Michel Fleur
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
#include "utils.h"

class Detection {
public:
    [[nodiscard]] virtual int read_ahead() const = 0;
    virtual void init() = 0;
    virtual void start(int channels, int rate) = 0;
    virtual void update_config() = 0;
    virtual void detect(const Index<float> & frame_in) = 0;
    virtual void apply_detect(Index<float> & frame_out) = 0;
    virtual ~Detection() = default;
};

#endif // AUDACIOUS_PLUGINS_BGM_DETECTION_H
