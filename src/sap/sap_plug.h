/*
 * SAP xmms plug-in. 
 * Copyright 2002/2003 by Michal 'Mikey' Szwaczko <mikey@scene.pl>
 *
 * SAP Library ver. 1.56 by Adam Bienias
 *
 * This is free software. You can modify it and distribute it under the terms
 * of the GNU General Public License. The verbatim text of the license can 
 * be found in file named COPYING in the source directory.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR 
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "saplib/sapLib.h"

/* Default frequency */
#define OUTPUT_FREQ 44100
/* how many samples per one saprender() */
#define N_RENDER 1024 

/* functions */
static int sap_is_our_file (char *);
static void sap_play_file(gchar *);
static void sap_stop(void);
static void sap_pause(short);
static void sap_seek(int);
static int sap_get_time(void);
static void sap_about(void);

extern void sap_file_info_box(char *);

static gboolean going;
static gboolean audio_error;
GThread *play_thread;

int currentSong;
static int tunes;

sapMUSICstrc *currentFile;

signed short play_buf[N_RENDER << 2];
