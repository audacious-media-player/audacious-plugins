
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "usf.h"
#include "cpu.h"
#include "memory.h"


int InitalizeApplication ( void )
{

	return 1;
}

void StopEmulation(void)
{
	//asm("int $3");
	//printf("Arrivederci!\n\n");
	//Release_Memory();
	//exit(0);
	cpu_running = 0;
}

void DisplayError (char * Message, ...) {
	char Msg[1000];
	va_list ap;

	va_start( ap, Message );
	vsprintf( Msg, Message, ap );
	va_end( ap );

	printf("Error: %s\n", Msg);
}

void UsfSleep(int32_t time)
{
	usleep(time * 1000);
}
