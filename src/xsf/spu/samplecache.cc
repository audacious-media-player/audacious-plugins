/**
 * Sample cache
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
#include "samplecache.h"
#include <tuple>

static inline constexpr uint64_t makeKey(uint32_t base, uint16_t loop, uint32_t length)
{
  return
    // has to be 32-bit aligned, so 2 low bits aren't needed
    // has to fit in the memory map, so high 7 bits aren't needed
    (uint64_t(base & 0x01FFFFFC) >> 2) |
    // loop uses the full 16 bits
    uint64_t(loop << 23) |
    // max length is 21 bits
    (uint64_t(length & 0x1FFFFF) << 39);
}

const SampleData& SampleCache::getSample(uint32_t baseAddr, uint16_t loopStartWords, uint32_t loopLengthWords, SampleData::Format format)
{
  uint64_t key = makeKey(baseAddr, loopStartWords, loopLengthWords);
  auto iter = samples.find(key);
  if (iter == samples.end()) {
    iter = samples.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(key),
      std::forward_as_tuple(baseAddr, loopStartWords << 2, (loopStartWords + loopLengthWords) << 2, format)
    ).first;
  }
  return iter->second;
}

void SampleCache::clear()
{
  samples.clear();
}
