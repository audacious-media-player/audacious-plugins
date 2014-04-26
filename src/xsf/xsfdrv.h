typedef int (PASCAL * LPFNGETLIB_XSFDRV)(void *lpWork, LPSTR lpszFilename, void **ppBuffer, DWORD *pdwSize);
typedef struct
{
	void * (PASCAL * LibAlloc)(DWORD dwSize);
	void (PASCAL * LibFree)(void *lpPtr);
	int (PASCAL * Start)(void *lpPtr, DWORD dwSize);
	void (PASCAL * Gen)(void *lpPtr, DWORD dwSamples);
	void (PASCAL * Term)(void);
} IXSFDRV;

typedef IXSFDRV * (PASCAL * LPFNXSFDRVSETUP)(LPFNGETLIB_XSFDRV lpfn, void *lpWork);
/* IXSFDRV * PASCAL XSFDRVSetup(LPFNGETLIB_XSFDRV lpfn, void *lpWork); */
