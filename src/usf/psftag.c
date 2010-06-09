
/////////////////////////////////////////////////////////////////////////////
//
// tag handling
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psftag.h"

#if !defined(__WIN32__) && !defined(_MSC_VER)
#include <unistd.h>
#else
#include <windows.h>
static void truncate(const char *filename, int size) {

  HANDLE f = CreateFile(
    filename,
    GENERIC_WRITE,
    0,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL
  );
  if(f == INVALID_HANDLE_VALUE) return;
  SetFilePointer(f, size, NULL, FILE_BEGIN);
  if(GetLastError() == NO_ERROR) SetEndOfFile(f);
  CloseHandle(f);
}
#endif

/////////////////////////////////////////////////////////////////////////////
/*
** Returns index, or -1 if not found
** Returns the index starting at the first non-whitespace character of
** the actual variable name
*/
static int find_tag_var_start(const char *tagbuffer, const char *varname) {
  int i, j;
  if(!tagbuffer || !varname) return -1;
  for(i = 0;;) {
    /*
    ** Find first non-whitespace
    ** (this is the variable name on the current line)
    */
    for(;; i++) {
      unsigned u = ((unsigned)(tagbuffer[i])) & 0xFF;
      /* If the tag ends here, we wouldn't have had any data anyway */
      if(!u) return -1;
      if(u <= 0x20) continue;
      break;
    }
    /*
    ** Compare case-insensitively to the var name
    */
    for(j = 0;; j++) {
      unsigned ucmp = ((unsigned)(varname  [    j])) & 0xFF;
      unsigned u    = ((unsigned)(tagbuffer[i + j])) & 0xFF;
      /* if the varname ends, we'll break here and do the equals check */
      if(!ucmp) break;
      /* If the tag ends here, we wouldn't have had any data anyway */
      if(!u) return -1;
      /* lowercase */
      if(ucmp >= 'A' && ucmp <= 'Z') { ucmp -= 'A'; ucmp += 'a'; }
      if(u    >= 'A' && u    <= 'Z') { u    -= 'A'; u    += 'a'; }
      /* if they're unequal, break... */
      if(u != ucmp) break;
    }
    /*
    ** Only if we exhausted the varname will we do the equals check
    */
    if(!varname[j]) {
      /*
      ** Ensure that the next non-whitespace character in [i+j] is an '='
      */
      for(;; j++) {
        unsigned u = ((unsigned)(tagbuffer[i + j])) & 0xFF;
        /* If the tag ends here, we wouldn't have had any data anyway */
        if(!u) return -1;
        /* quit at the first '=' - success! */
        if(u == '=') return i;
        /* shouldn't be a newline here! */
        if(u == 0x0A) break;
        /* ignore whitespace */
        if(u <= 0x20) continue;
        /* any other character is an error */
        break;
      }
    }
    i += j + 1;
    /*
    ** Find newline or end-of-tag
    */
    for(;; i++) {
      unsigned u = ((unsigned)(tagbuffer[i])) & 0xFF;
      /* If the tag ends here, we wouldn't have had any data anyway */
      if(!u) return -1;
      if(u == 0x0A) break;
    }
  }
  return -1;
}

/*
** Returns the index at which the current variable ends
** (Includes any ending newline and possibly whitespace after that)
**
** Buffer points to the first non-whitespace character of the variable name
*/
static int find_tag_var_end(const char *tagbuffer) {
  int i, j;
  if(!tagbuffer) return 0;
  for(i = 0;;) {
    /*
    ** Find first non-whitespace
    ** (this is the variable name on the current line)
    */
    for(;; i++) {
      unsigned u = ((unsigned)(tagbuffer[i])) & 0xFF;
      if(!u) return i;
      if(u <= 0x20) continue;
      break;
    }
    /*
    ** Compare case-insensitively to the original var name
    */
    for(j = 0;; j++) {
      unsigned ucmp = ((unsigned)(tagbuffer[    j])) & 0xFF;
      unsigned u    = ((unsigned)(tagbuffer[i + j])) & 0xFF;
      /* If the tag ends here, we wouldn't have had any data anyway */
      if(!u) return i;
      /* lowercase */
      if(ucmp >= 'A' && ucmp <= 'Z') { ucmp -= 'A'; ucmp += 'a'; }
      if(u    >= 'A' && u    <= 'Z') { u    -= 'A'; u    += 'a'; }
      /* if they're both whitespace or '=', we win */
      if((u <= 0x20 || u == '=') && (ucmp <= 0x20 || ucmp == '=')) break;
      /* if they're unequal, we lose */
      if(u != ucmp) return i;
      /* otherwise, keep trying */
    }
    /*
    ** Ensure that the next non-whitespace character in [i+j] is an '='
    */
    for(;; j++) {
      unsigned u = ((unsigned)(tagbuffer[i + j])) & 0xFF;
      if(!u) return i;
      /* quit at the first '=' */
      if(u == '=') break;
      /* shouldn't be a newline here! */
      if(u == 0x0A) return i;
      /* ignore whitespace */
      if(u <= 0x20) continue;
      /* any other character is an error */
      break;
    }
    i += j + 1;
    /*
    ** Find newline or end-of-tag
    */
    for(;; i++) {
      unsigned u = ((unsigned)(tagbuffer[i])) & 0xFF;
      if(!u) return i;
      if(u == 0x0A) break;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
/*
** Get tag variable
** The destination value buffer must be as big as the entire tag
*/
int psftag_raw_getvar(
  const char *tag,
  const char *variable,
  char *value_out,
  int value_out_size
) {
  char *v = value_out;
  char *vmax = v + value_out_size;
  char *v_linebegin;
  int i, i_end;
  //
  // Safety check
  //
  if(value_out_size < 1) return -1;
  /*
  ** Default to empty string
  */
  *v = 0;
  /*
  ** Find the variable start/end index
  */
  i = find_tag_var_start(tag, variable);
  if(i < 0) return -1;
  i_end = i + find_tag_var_end(tag + i);
  /*
  ** Extract the variable data
  */
  while(i < i_end) {
    /*
    ** Skip to first '='
    */
    while((tag[i] != '=') && (i < i_end)) { i++; }
    if(i >= i_end) break;
    /*
    ** If this is not the first line, add a newline
    */
    if(v > value_out) {
      if(v < vmax) { *v++ = 0x0A; }
    }
    /*
    ** Now that we're at a '=', skip past it
    */
    i++;
    if(i >= i_end) break;
    /*
    ** Skip past any whitespace except newlines
    */
    for(; i < i_end; i++) {
      unsigned u = ((unsigned)(tag[i])) & 0xFF;
      if(u == 0x0A) break;
      if(u <= 0x20) continue;
      break;
    }
    if(i >= i_end) break;
    /*
    ** Consume line data
    */
    v_linebegin = v;
    while(i < i_end) {
      unsigned u = ((unsigned)(tag[i++])) & 0xFF;
      if(u == 0x0A) break;
      if(v < vmax) { *v++ = u; }
    }
    /*
    ** Eat end-of-line whitespace
    */
    while(v > v_linebegin && (((unsigned)(v[-1]))&0xFF) <= 0x20) {
      v--;
    }
  }
  /*
  ** Set variable end
  */
  if(v >= vmax) { v = vmax - 1; }
  *v = 0;
  return 0;
}

/////////////////////////////////////////////////////////////////////////////

void psftag_raw_setvar(
  char *tag,
  int tag_max_size,
  const char *variable,
  const char *value
) {
  int tag_l = strlen(tag);
  int i, i_end, z;
  int insert_i;
  int insert_l;
  int value_exists = 0;
  int tag_max_usable_size = tag_max_size - 1;
  //
  // Safety check
  //
  if(tag_max_size < 1) return;
  //
  // We will assume we can at least use what's there
  //
  if(tag_max_usable_size < tag_l) {
    tag_max_usable_size = tag_l;
    tag_max_size = tag_l + 1;
  }
  /*
  ** Determine the insertion length of the new variable
  */
  { const char *v;
    int nl = strlen(variable);
    insert_l = nl + 2;
    for(v = value; *v; v++) {
      insert_l++;
      if(*v == 0x0A) {
        /* Value exists if it's multi-line */
        value_exists = 1;
        insert_l += nl + 1;
      } else if((((unsigned)(*v))&0xFF) > 0x20) {
        /* Value exists if there are non-whitespace characters */
        value_exists = 1;
      }
    }
  }
  /*
  ** If the value is blank, force the insert length to zero
  */
  if(!value_exists) insert_l = 0;
  /*
  ** Find the variable start index
  */
  i = find_tag_var_start(tag, variable);
  /*
  ** If not found, add a new variable
  */
  if(i < 0) {
    /* Insert position is at the end */
    insert_i = tag_l;
    /* Eat trailing whitespace in the file */
    while(insert_i && (((unsigned)(tag[insert_i - 1]))&0xFF) <= 0x20) { insert_i--; }
    /* Insert a newline if there's room and if there's stuff before */
    if(insert_i && (insert_i < tag_max_usable_size)) { tag[insert_i++] = 0x0A; }
    /* Clamp insert length */
    if((insert_i + insert_l) > tag_max_usable_size) { insert_l = tag_max_usable_size - insert_i; }
    z = insert_i + insert_l;
  /*
  ** Otherwise, find the variable end index
  */
  } else {
    int movel;
    insert_i = i;
    /* Clamp insert length */
    if((insert_i + insert_l) > tag_max_usable_size) { insert_l = tag_max_usable_size - insert_i; }
    i_end = i + find_tag_var_end(tag + i);
    /* Move remaining file data */
    movel = tag_l - i_end;
    if(movel > (tag_max_usable_size-(insert_i+insert_l))) { movel = tag_max_usable_size - (insert_i+insert_l); }
    /* perform the move */
    if(movel && ((insert_i+insert_l) != i_end)) {
      memmove(tag+insert_i+insert_l, tag+i_end, movel);
    }
    z = insert_i+insert_l+movel;
  }
  /* Add terminating null ahead of time */
  if(z > tag_max_usable_size) z = tag_max_usable_size;
  tag[z] = 0;
  /*
  ** Write the variable to index insert_i, max length insert_l
  */
  insert_l += insert_i;
  while(insert_i < insert_l) {
    const char *v;
    for(v = variable; (*v) && (insert_i < insert_l); v++) {
      tag[insert_i++] = *v;
    }
    if(insert_i >= insert_l) break;
    tag[insert_i++] = '=';
    if(insert_i >= insert_l) break;
    for(; (*value) && ((*value) != 0x0A) && (insert_i < insert_l); value++) {
      tag[insert_i++] = *value;
    }
    if(insert_i >= insert_l) break;
    tag[insert_i++] = 0x0A;
    if(insert_i >= insert_l) break;
    if(!(*value)) break;
    if((*value) == 0x0A) value++;
  }

}

/////////////////////////////////////////////////////////////////////////////

#define TAGMAX (50000)
#define ERRORMAX (256)

struct PSFTAG {
  char str[TAGMAX + 1];
};

/*void *psftag_create(void) {
  struct PSFTAG *p = malloc(sizeof(struct PSFTAG));
  if(!p) return NULL;
  p->str[0] = 0;
  //p->errorstring[0] = 0;
  return p;
}

void psftag_delete(void *psftag) {
  free(psftag);
}*/

/////////////////////////////////////////////////////////////////////////////

/*const char *psftag_getlasterror(void *psftag) {
  return ((struct PSFTAG*)psftag)->errorstring;
}*/

/////////////////////////////////////////////////////////////////////////////

void psftag_getraw(void *psftag, char *raw_out, int raw_out_size) {
  if(raw_out_size < 1) return;
  strncpy(raw_out, ((struct PSFTAG*)psftag)->str, raw_out_size);
  raw_out[raw_out_size - 1] = 0;
}

void psftag_setraw(void *psftag, const char *raw_in) {
  strncpy(((struct PSFTAG*)psftag)->str, raw_in, TAGMAX + 1);
  ((struct PSFTAG*)psftag)->str[TAGMAX] = 0;
}

/////////////////////////////////////////////////////////////////////////////

int psftag_getvar(void *psftag, const char *variable, char *value_out, int value_out_size) {
  return psftag_raw_getvar(((struct PSFTAG*)psftag)->str, variable, value_out, value_out_size);
}

void psftag_setvar(void *psftag, const char *variable, const char *value) {
  psftag_raw_setvar(((struct PSFTAG*)psftag)->str, TAGMAX + 1, variable, value);
}

/////////////////////////////////////////////////////////////////////////////

int psftag_readfromfile(void *psftag, const char *path) {
  struct PSFTAG *t = (struct PSFTAG*)psftag;
  FILE *f = NULL;
  int l;
  int rsize, exesize, tagstart;
  char hdr[12];

  f = fopen(path, "rb");
  if(!f) {
    //strncpy(t->errorstring, strerror(errno), ERRORMAX);
    //t->errorstring[ERRORMAX-1] = 0;
    return -1;
  }

  if(fread(hdr, 1, 12, f) != 12) goto invalidformat;
  if(memcmp(hdr, "PSF", 3)) goto invalidformat;

  rsize =
    ((((unsigned)(hdr[ 4])) & 0xFF) <<  0) |
    ((((unsigned)(hdr[ 5])) & 0xFF) <<  8) |
    ((((unsigned)(hdr[ 6])) & 0xFF) << 16) |
    ((((unsigned)(hdr[ 7])) & 0xFF) << 24);
  exesize =
    ((((unsigned)(hdr[ 8])) & 0xFF) <<  0) |
    ((((unsigned)(hdr[ 9])) & 0xFF) <<  8) |
    ((((unsigned)(hdr[10])) & 0xFF) << 16) |
    ((((unsigned)(hdr[11])) & 0xFF) << 24);

  tagstart = 16 + rsize + exesize;

  fseek(f, tagstart, SEEK_SET);

  if(fread(hdr, 1, 5, f) != 5) goto notpresent;
  if(memcmp(hdr, "[TAG]", 5)) goto invalidformat;

  tagstart += 5;
  fseek(f, 0, SEEK_END);
  l = ftell(f);
  fseek(f, tagstart, SEEK_SET);
  l -= tagstart;
  if(l < 0) l = 0;
  if(l > TAGMAX) l = TAGMAX;

  memset(t->str, 0, TAGMAX + 1);
  fread(t->str, 1, l, f);

  fclose(f);
  return 0;

invalidformat:
//  strcpy(t->errorstring, "Invalid file format");
  goto error;
notpresent:
//  strcpy(t->errorstring, "Tag not present");
  goto error;
error:
  if(f) fclose(f);
  return -1;
}

/////////////////////////////////////////////////////////////////////////////

int psftag_writetofile(void *psftag, const char *path) {
  struct PSFTAG *t = (struct PSFTAG*)psftag;
  FILE *f = NULL;
  int l;
  int rsize, exesize, tagstart;
  char hdr[12];

  f = fopen(path, "r+b");
  if(!f) {
    //strncpy(t->errorstring, strerror(errno), ERRORMAX);
    //t->errorstring[ERRORMAX-1] = 0;
    return -1;
  }

  if(fread(hdr, 1, 12, f) != 12) goto invalidformat;
  if(memcmp(hdr, "PSF", 3)) goto invalidformat;

  rsize =
    ((((unsigned)(hdr[ 4])) & 0xFF) <<  0) |
    ((((unsigned)(hdr[ 5])) & 0xFF) <<  8) |
    ((((unsigned)(hdr[ 6])) & 0xFF) << 16) |
    ((((unsigned)(hdr[ 7])) & 0xFF) << 24);
  exesize =
    ((((unsigned)(hdr[ 8])) & 0xFF) <<  0) |
    ((((unsigned)(hdr[ 9])) & 0xFF) <<  8) |
    ((((unsigned)(hdr[10])) & 0xFF) << 16) |
    ((((unsigned)(hdr[11])) & 0xFF) << 24);

  tagstart = 16 + rsize + exesize;

  fseek(f, tagstart, SEEK_SET);

  l = strlen(t->str);

  fwrite("[TAG]", 1, 5, f);
  fwrite(t->str, 1, l, f);
  fclose(f);

  truncate(path, tagstart + 5 + l);

  return 0;

invalidformat:
//  strcpy(t->errorstring, "Invalid file format");
  goto error;
error:
  if(f) fclose(f);
  return -1;
}

/////////////////////////////////////////////////////////////////////////////
