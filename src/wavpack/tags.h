#ifndef _tags_h
#define _tags_h

#include <stdio.h>

const int MAX_LEN = 2048;
const int MAX_LEN2 = 128;
const int TAG_NONE = 0;
const int TAG_ID3 = 1;
const int TAG_APE = 2;

typedef struct {
    char    title           [MAX_LEN];
    char    artist          [MAX_LEN];
    char    album           [MAX_LEN];
    char    comment         [MAX_LEN];
    char    genre           [MAX_LEN];
    char    track           [MAX_LEN2];
    char    year            [MAX_LEN2];
    int     _genre;
} ape_tag;

int utf8ToUnicode ( const char* lpMultiByteStr, wchar_t* lpWideCharStr, int cmbChars );

int GetTageType ( FILE *fp );

int DeleteTag ( char* filename);

int WriteAPE2Tag ( char* fp, ape_tag *Tag );

int ReadAPE2Tag ( FILE *fp, ape_tag *Tag );

int ReadID3Tag ( FILE *fp, ape_tag *Tag );

#endif
