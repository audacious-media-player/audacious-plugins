#ifndef _AUDIO_H_
#define _AUDIO_H_

#include "usf.h"
#include "cpu.h"
#include "memory.h"

uint32_t AiReadLength(void);
void AiLenChanged(void);
void AiDacrateChanged(uint32_t value);
void OpenSound(void);

#endif
