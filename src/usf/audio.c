#include "usf.h"
#include "audio_hle.h"
#include "memory.h"
#include "audio.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/soundcard.h>

#include <audacious/util.h>
#include <audacious/plugin.h>
#include <audacious/output.h>
#include <audacious/i18n.h>

#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

extern InputPlayback * pcontext;
int32_t SampleRate = 0;
static int16_t samplebuf[16384];

void OpenSound(void)
{
	if (pcontext->output->open_audio(FMT_S16_NE, SampleRate, 2) == 0) {
		cpu_running = 0;
		printf("Fail Starting audio\n");
		g_thread_exit(NULL);
	} else {
	}
}

void AddBuffer(unsigned char *buf, unsigned int length) {
	int32_t i = 0, out = 0;
	double vol = 1.0;	
	
	if(!cpu_running)
		return;
		
	if(is_seeking) {		
		play_time += (((double)(length >> 2) / (double)SampleRate) * 1000.0);
		if(play_time > (double)seek_time) {
			is_seeking = 0;
		}
		return;
	}
	
	if(play_time > track_time) {
		vol =  1.0f - (((double)play_time - (double)track_time) / (double)fade_time);
	} 
			
	for(out = i = 0; i < (length >> 1); i+=2)
	{
		samplebuf[out++] = (int16_t)(vol * (double)((int16_t*)buf)[i+1]);
		samplebuf[out++] = (int16_t)(vol * (double)((int16_t*)buf)[i]);
	}
	
	pcontext->playing = play_time < (track_time + fade_time);
  	pcontext->eof = play_time >= (track_time + fade_time);
	
	play_time += (((double)(length >> 2) / (double)SampleRate) * 1000.0);
 	
	pcontext->output->write_audio (samplebuf, length);
	
	if(play_time > (track_time + fade_time))
	{
		cpu_running = 0;
	}

}

void AiLenChanged(void) {
	int32_t length = 0;
	uint32_t address = (AI_DRAM_ADDR_REG & 0x00FFFFF8);

	length = AI_LEN_REG & 0x3FFF8;

	while(is_paused && !is_seeking && cpu_running)
		g_usleep(10000);
		
	
	AddBuffer(RDRAM+address, length);

	if(length && !(AI_STATUS_REG&0x80000000)) {
		const float VSyncTiming = 789000.0f;
		double BytesPerSecond = 48681812.0 / (AI_DACRATE_REG + 1) * 4;
		double CountsPerSecond = (double)((((double)VSyncTiming) * (double)60.0)) * 2.0;
		double CountsPerByte = (double)CountsPerSecond / (double)BytesPerSecond;
		unsigned int IntScheduled = (unsigned int)((double)AI_LEN_REG * CountsPerByte);

		ChangeTimer(AiTimer,IntScheduled);
	}

	if(enableFIFOfull) {
		if(AI_STATUS_REG&0x40000000)
			AI_STATUS_REG|=0x80000000;
	}

	AI_STATUS_REG|=0x40000000;

}

unsigned int AiReadLength(void) {
	AI_LEN_REG = 0;
	return AI_LEN_REG;
}

void AiDacrateChanged(unsigned  int value) {
	AI_DACRATE_REG = value;
	SampleRate = 48681812 / (AI_DACRATE_REG + 1);

}
