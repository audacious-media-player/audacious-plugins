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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <audacious/util.h>
#include <audacious/plugin.h>
#include <glib.h>
#include <string.h>
#include <pthread.h>

#include "sap_plug.h"

InputPlugin sap_ip = {

	NULL,
	NULL,
	"SAP Plugin " VERSION, 
	NULL, 
	sap_about, 
	NULL,
	sap_is_our_file,
	NULL,
	sap_play_file,
	sap_stop,
	sap_pause,
	sap_seek,
	NULL,
	sap_get_time,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,  
	sap_file_info_box, 
	NULL
};

InputPlugin *get_iplugin_info (void) { 
    return &sap_ip; 
}

static int sap_is_our_file (char *filename) {

	char *ext;

	ext = strrchr(filename, '.');
	
	if (ext)
	{
		if (!strcmp(ext, ".SAP"))
			return 1;
		if (!strcmp(ext, ".sap"))
			return 1;
	}

	return 0;
}

static void *play_loop (void *arg) {

	int datasize = N_RENDER << 2; 
 
	while (going) {
	
	    sapRenderBuffer(play_buf,N_RENDER);
	    /* for spectrum analyser */
	    sap_ip.add_vis_pcm(sap_ip.output->written_time(), FMT_S16_NE, 2, N_RENDER, play_buf);
	
		while (sap_ip.output->buffer_free() < ( N_RENDER << 2 ) && going)
		
			xmms_usleep(30000);

		if (going) 
		
			sap_ip.output->write_audio(play_buf, datasize);

		
	}
	
	sap_ip.output->buffer_free();
	sap_ip.output->buffer_free();
	pthread_exit(NULL);

}
		
static void sap_play_file (char *filename) {
	
	GString *titstr;

    	if ((currentFile = sapLoadMusicFile(filename)) == NULL) return;

	/* for tunes */
	tunes = currentFile->numOfSongs;

	if (tunes < 0) tunes = 1;

	/* we always start with 1st tune */
	currentSong = 0;

	sapPlaySong(currentSong);
	
 	going = TRUE;
	audio_error = FALSE;

		if (sap_ip.output->open_audio(FMT_S16_LE, OUTPUT_FREQ, 2) == 0) {
	
			audio_error = TRUE;
			going = FALSE;
	
			return;
		}
	/* delete '.sap' from filename to display */
	titstr = g_string_new(g_basename(filename));
	g_string_truncate(titstr,titstr->len - 4);

	sap_ip.set_info(titstr->str, tunes, tunes*1024, OUTPUT_FREQ, 2);

	g_string_free(titstr,TRUE);
	
	pthread_create(&play_thread, NULL, play_loop, NULL);
	

}

static void sap_stop (void) {

	if (going) {
	
		going = FALSE;
		pthread_join(play_thread, NULL);
		sap_ip.output->close_audio();
	}
}

static void sap_seek(int time) {

       if (currentSong != tunes) {          
        
		 currentSong = (currentSong+1) % currentFile->numOfSongs;
		 sapPlaySong(currentSong);
	         sap_ip.output->flush(currentSong * 1000);
       
       }
}

static void sap_pause(short paused) {

    sap_ip.output->pause(paused);
	
}

static int sap_get_time(void) {

    return sap_ip.output->output_time();
}

static void sap_about (void) {

	static GtkWidget *aboutbox;

	if (aboutbox != NULL)
		return;
	
	aboutbox = xmms_show_message(
		"About SAP Plugin",
		"SAP Player plug-in v"VERSION"\nby Michal Szwaczko <mikey@scene.pl>\n SAP library ver 0.3F by Adam Bienias\n\n"
		"Get more POKEY sound from ASMA at:\n[http://asma.musichall.cz]\n\nEnjoy!",
		"Ok", FALSE, NULL, NULL);

	gtk_signal_connect(GTK_OBJECT(aboutbox), "destroy",
			   GTK_SIGNAL_FUNC(gtk_widget_destroyed), &aboutbox);
}
