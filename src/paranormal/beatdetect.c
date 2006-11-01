#include "paranormal.h"

int
pn_is_new_beat(void)
{
  /* quantize the average energy of the pcm_data for beat detection. */
  int fftsum =
    ((pn_sound_data->pcm_data[0][0] + pn_sound_data->pcm_data[1][0]) >> 6);

  /*
   * if the energy's quantization is within this, trigger as a detected
   * beat, otherwise don't.
   */
  return (fftsum >= 300 && fftsum <= 600) ? 1 : 0;
}
