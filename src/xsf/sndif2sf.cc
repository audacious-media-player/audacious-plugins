/**
 * Desmume sound interface for audio export
 * Copyright (c) 2014 Naram Qashat <cyberbotx@cyberbotx.com>
 * Copyright (c) 2020 Adam Higerd <chighland@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the names of R. Belmont and Richard Bannister nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "sndif2sf.h"
#include "desmume/NDSSystem.h"
#include <vector>

std::list<std::vector<std::uint8_t>> buffer_rope;

static struct
{
  std::vector<uint8_t> buf;
  unsigned filled, used;
  uint32_t bufferbytes, cycles;
  int xfs_load, sync_type;
} sndifwork = {std::vector<uint8_t>(), 0, 0, 0, 0, 0, 0};

static void SNDIFDeInit() {
  int buffersize = sndifwork.buf.size();
  sndifwork.buf.resize(0);
  sndifwork.buf.resize(buffersize);
  buffer_rope.clear();
}

static int SNDIFInit(int buffersize)
{
  uint32_t bufferbytes = buffersize * sizeof(int16_t);
  SNDIFDeInit();
  sndifwork.buf.resize(bufferbytes + 3);
  sndifwork.bufferbytes = bufferbytes;
  sndifwork.filled = sndifwork.used = 0;
  sndifwork.cycles = 0;
  return 0;
}

static void SNDIFMuteAudio() { }
static void SNDIFUnMuteAudio() { }
static void SNDIFSetVolume(int) { }

static uint32_t SNDIFGetAudioSpace()
{
  return sndifwork.bufferbytes >> 2; // bytes to samples
}

static void SNDIFUpdateAudio(int16_t *buffer, uint32_t num_samples)
{
  num_samples <<= 1; // stereo
  uint32_t num_bytes = num_samples << 1;
  if (num_bytes > sndifwork.bufferbytes) {
    num_bytes = sndifwork.bufferbytes;
    num_samples = num_bytes >> 1;
  }
  memcpy(&sndifwork.buf[0], buffer, num_bytes);
  buffer_rope.push_back(std::vector<uint8_t>(reinterpret_cast<uint8_t*>(buffer), reinterpret_cast<uint8_t*>(buffer) + num_bytes));
  sndifwork.filled = num_bytes;
  sndifwork.used = 0;
}

const int SNDIFID_2SF = 1;
SoundInterface_struct SNDIF_2SF =
{
  SNDIFID_2SF,
  "2sf Sound Interface",
  SNDIFInit,
  SNDIFDeInit,
  SNDIFUpdateAudio,
  SNDIFGetAudioSpace,
  SNDIFMuteAudio,
  SNDIFUnMuteAudio,
  SNDIFSetVolume,
  nullptr,
  nullptr,
  nullptr
};

SoundInterface_struct *SNDCoreList[] =
{
  &SNDIF_2SF,
  &SNDDummy,
  nullptr
};
