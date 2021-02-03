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
