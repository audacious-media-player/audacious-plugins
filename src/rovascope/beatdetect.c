#include "paranormal.h"

/*
 * This algorithm is by Janusz Gregorcyzk, the implementation is
 * mine, however.
 *
 *   -- nenolod
 */
int
pn_is_new_beat(void)
{
  gint i;
  gint total = 0;
  gboolean ret = FALSE;
  static gint previous;

  for (i = 1; i < 512; i++)
    {
       total += abs (pn_sound_data->pcm_data[0][i] -
		     pn_sound_data->pcm_data[0][i - 1]);
    }

  total /= 512;

  ret = (total > (2 * previous));

  previous = total;

  return ret;
}
