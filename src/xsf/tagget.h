static int getdwordle(const unsigned char *pData)
{
	return pData[0] | ((pData[1]) << 8) | ((pData[2]) << 16) | ((pData[3]) << 24);
}

static int xsf_tagsearchraw(const unsigned char *pData, int dwSize)
{
	int dwPos;
	int dwReservedAreaSize;
	int dwProgramLength;
/*	int dwProgramCRC; */
	if (dwSize < 16 + 5 + 1) return 0;
	if (pData[0] != 'P') return 0;
	if (pData[1] != 'S') return 0;
	if (pData[2] != 'F') return 0;
	dwReservedAreaSize = getdwordle(pData + 4);
	dwProgramLength = getdwordle(pData + 8);
/*	dwProgramCRC = getdwordle(pData + 12); */
	dwPos = 16 + dwReservedAreaSize + dwProgramLength;
	if (dwPos >= dwSize) return 0;
	return dwPos;
}
static int xsf_tagsearch(int *pdwRet, const unsigned char *pData, int dwSize)
{
	int dwPos = xsf_tagsearchraw(pData, dwSize);
	if (dwSize < dwPos + 5) return 0;
	if (memcmp(pData + dwPos, "[TAG]", 5)) return 0;
	*pdwRet = dwPos + 5;
	return 1;
}

enum xsf_tagenum_callback_returnvalue
{
	xsf_tagenum_callback_returnvaluecontinue = 0,
	xsf_tagenum_callback_returnvaluebreak = 1
};
typedef int (*pfnxsf_tagenum_callback_t)(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd);
static int xsf_tagenumraw(pfnxsf_tagenum_callback_t pCallBack, void *pWork, const unsigned char *pData, int dwSize)
{
	int dwPos = 0;
	while (dwPos < dwSize)
	{
		int dwNameTop;
		int dwNameEnd;
		int dwValueTop;
		int dwValueEnd;
		if (dwPos < dwSize && pData[dwPos] == 0x0a) dwPos++;
		while (dwPos < dwSize && pData[dwPos] != 0x0a && 0x01 <= pData[dwPos] && pData[dwPos] <= 0x20)
			dwPos++;
		if (dwPos >= dwSize || pData[dwPos] == 0x0a) continue;
		dwNameTop = dwPos;
		while (dwPos < dwSize && pData[dwPos] != 0x0a && pData[dwPos] != '=')
			dwPos++;
		if (dwPos >= dwSize || pData[dwPos] == 0x0a) continue;
		dwNameEnd = dwPos;
		while (dwNameTop < dwNameEnd &&  0x01 <= pData[dwNameEnd - 1] && pData[dwNameEnd - 1] <= 0x20)
			dwNameEnd--;
		if (dwPos < dwSize && pData[dwPos] == '=') dwPos++;
		while (dwPos < dwSize && pData[dwPos] != 0x0a && 0x01 <= pData[dwPos] && pData[dwPos] <= 0x20)
			dwPos++;
		dwValueTop = dwPos;
		while (dwPos < dwSize && pData[dwPos] != 0x0a)
			dwPos++;
		dwValueEnd = dwPos;
		while (dwValueTop < dwValueEnd &&  0x01 <= pData[dwValueEnd - 1] && pData[dwValueEnd - 1] <= 0x20)
			dwValueEnd--;

		if (pCallBack)
		{
			if (xsf_tagenum_callback_returnvaluecontinue != pCallBack(pWork, (const char *)pData + dwNameTop, (const char *)pData + dwNameEnd, (const char *)pData + dwValueTop, (const char *)pData + dwValueEnd))
				return -1;
		}
	}
	return 1;
}

static int xsf_tagenum(pfnxsf_tagenum_callback_t pCallBack, void *pWork, const unsigned char *pData, int dwSize)
{
	int dwPos = 0;
	if (!xsf_tagsearch(&dwPos, pData, dwSize))
		return 0;
	return xsf_tagenumraw(pCallBack, pWork, pData + dwPos, dwSize - dwPos);
}

typedef struct
{
	int taglen;
	const char *tag;
	char *ret;
} xsf_tagget_work_t;

static int xsf_tagenum_callback_tagget(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
{
	xsf_tagget_work_t *pwork = (xsf_tagget_work_t *)pWork;
	if (pwork->taglen == pNameEnd - pNameTop && !strcmp_nocase(pNameTop, pwork->tag, pwork->taglen))
	{
		char *ret = (char *)malloc(pValueEnd - pValueTop + 1);
		if (!ret) return xsf_tagenum_callback_returnvaluecontinue;
		memcpy(ret, pValueTop, pValueEnd - pValueTop);
		ret[pValueEnd - pValueTop] = 0;
		pwork->ret = ret;
		return xsf_tagenum_callback_returnvaluebreak;
	}
	return xsf_tagenum_callback_returnvaluecontinue;
}

static char *xsf_tagget(const char *tag, const unsigned char *pData, int dwSize)
{
	xsf_tagget_work_t work;
	work.ret = 0;
	work.tag = tag;
	work.taglen = (int)strlen(tag);
	xsf_tagenum(xsf_tagenum_callback_tagget, &work, pData, dwSize);
	return work.ret;
}

static int xsf_tagget_int(const char *tag, const unsigned char *pData, int dwSize, int value_default)
{
	int ret = value_default;
	char *value = xsf_tagget(tag, pData, dwSize);
	if (value)
	{
		if (*value) ret = atoi(value);
		free(value);
	}
	return ret;
}

