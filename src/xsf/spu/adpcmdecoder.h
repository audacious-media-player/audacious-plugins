/**
 * ADPCM decoder
 * Copyright (c) 2020 Adam Higerd <chighland@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef ADPCMDECODER_H
#define ADPCMDECODER_H

#include <vector>
#include <cstdint>

class AdpcmDecoder
{
private:
  int16_t predictor;
  int8_t index;

public:
  AdpcmDecoder(int16_t initialPredictor, int16_t initialStep);
  int16_t getNextSample(uint8_t value);

  std::vector<int16_t> decode(const std::vector<char>& data, uint32_t offset = 0, uint32_t length = 0);
  static std::vector<int16_t> decodeFile(const std::vector<char>& data, uint32_t offset = 0, uint32_t length = 0);
};

#endif
