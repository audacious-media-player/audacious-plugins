/**
 * Caching sample decoder
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
#ifndef TWOSF2WAV_SAMPLEDATA_H
#define TWOSF2WAV_SAMPLEDATA_H

#include <vector>
#include <cstdint>
class IInterpolator;

class SampleData : public std::vector<int32_t>
{
public:
  enum Format {
    Pcm8,
    Pcm16,
    Adpcm
  };

  SampleData();
  SampleData(uint32_t baseAddr, uint16_t loopStart, uint32_t loopLength, Format format);
  SampleData(const SampleData&) = default;
  SampleData(SampleData&&) = default;
  ~SampleData() = default;

  SampleData& operator=(const SampleData&) = default;
  SampleData& operator=(SampleData&&) = default;

  int32_t sampleAt(double time, IInterpolator* interp = nullptr) const;

  uint32_t baseAddr;
  uint16_t loopStart;
  uint32_t loopLength;

private:
  void loadPcm8();
  void loadPcm16();
  void loadAdpcm();
};

#endif
