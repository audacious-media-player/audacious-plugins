#ifdef LINUX
typedef char BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int DWORD;

#ifndef TRUE
# define TRUE    1
# define FALSE   0
#endif

#else
# include <windows.h>
#endif

#ifndef _GSF_C
extern void DisplayError (char *, ...);

extern BOOL IsTagPresent (BYTE *);
extern BOOL IsValidGSF (BYTE *);
extern void setupSound(void);
extern int GSFRun(char *);
extern void GSFClose(void) ;
extern BOOL EmulationLoop(void);
#endif
