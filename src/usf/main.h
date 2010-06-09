#include <stdint.h>

#define CPU_Default					-1
#define CPU_Interpreter				0
#define CPU_Recompiler				1

int InitalizeApplication ( void );
void DisplayError (char * Message, ...);
void StopEmulation(void);
void UsfSleep(int32_t);
