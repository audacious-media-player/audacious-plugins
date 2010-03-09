#ifndef _USF_H_
#define _USF_H_
#define _CRT_SECURE_NO_WARNINGS


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>



#include "usf.h"
#include "cpu.h"
#include "memory.h"

extern int8_t filename[512], title[100];
extern uint32_t cpu_running, use_interpreter, use_audiohle, is_paused, cpu_stopped, fake_seek_stopping;
extern uint32_t is_fading, fade_type, fade_time, is_seeking, seek_backwards, track_time;
extern double seek_time, play_time, rel_volume;

extern uint32_t enablecompare, enableFIFOfull;


#endif

