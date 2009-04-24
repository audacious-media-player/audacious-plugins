/* timing.c
 *
 * get timing info based on file name
 *
 * file name should have a comment like this:
 * R(RMS)-T(in):(out):(end)
 * where:
 * - RMS is an indication of the RMS value in between in and out
 *   (to be compared against maximum sample value, 32767 for 16 bit audio)
 * - in, out and end are floats, giving the mixing start and end points
 *   and the length of the file in seconds
 */

#include "timing.h"
#include "crossfade.h"
#include "debug.h"

#include <stdio.h>
#include <string.h>
#include <locale.h>

#if defined(HAVE_ID3LIB)
#  include <id3.h>
#endif

#undef VERBOSE

int
get_timing_comment(char *filename, quantaudio_t *qa)
/* 
 * check if the file given has a timing comment
 * return 1 if it does and 0 if it doesn't
 * store the relevant data in quantaudio
 */
{
	id3_t id3;
	int nscanned;

	setlocale(LC_NUMERIC, "C");

	get_id3(filename, &id3);
	if ((nscanned = sscanf(id3.comment, "R:%d-T:%f:%f:%f", &qa->RMS, &qa->mix_in, &qa->mix_out, &qa->length)) < 4)
	{
		/* tag not right */
#ifdef VERBOSE
		DEBUG(("[crossfade] get_timing_comment: no quantaudio comment\n"));
		DEBUG(("[crossfade] get_timing_comment: nscanned=%d (\"%s\")\n", nscanned, id3.comment));
		DEBUG(("[crossfade] get_timing_comment: in %.2f, out %.2f, length %.2f, RMS %d\n",
			qa->mix_in, qa->mix_out, qa->length, qa->RMS));
#endif
		return 0;
	}
	else
	{
#ifdef VERBOSE
		DEBUG(("[crossfade] get_timing_comment: in %.2f, out %.2f, length %.2f, RMS %d\n",
			qa->mix_in, qa->mix_out, qa->length, qa->RMS));
#endif
		return 1;
	}
}

int
get_id3(char *filename, id3_t *id3)
#if defined(HAVE_ID3LIB)
/* get the id3 tag of this file.  Return 0 when failed or 1 when ok */
{
	int status = 0;
	ID3Tag *tag;
	memset(id3, 0, sizeof(*id3));

	if ((tag = ID3Tag_New()) != NULL)
	{
		ID3Frame *frame;
		(void) ID3Tag_Link(tag, filename);

		if ((frame = ID3Tag_FindFrameWithID(tag, ID3FID_COMMENT)) != NULL)
		{
			ID3Field *field;
			if ((field = ID3Frame_GetField(frame, ID3FN_TEXT)) != NULL)
			{
				(void) ID3Field_GetASCII(field, id3->comment, sizeof(id3->comment));
				DEBUG(("[crossfade] get_id3: comment: %s\n", id3->comment));
				status = 1;
			}
		}

		if ((frame = ID3Tag_FindFrameWithID(tag, ID3FID_TRACKNUM)) != NULL)
		{
			ID3Field *field;
			if ((field = ID3Frame_GetField(frame, ID3FN_TEXT)) != NULL)
			{
				char buf[32];
				     buf[0] = 0;
				(void) ID3Field_GetASCII(field, buf, sizeof(buf));
				id3->track = atoi(buf);
				DEBUG(("[crossfade] get_id3: track: %d\n", id3->track));
				status = 1;
			}
		}
		
		ID3Tag_Delete(tag);
	}

	return status;
}
#else
{
	FILE *fp;
	memset(id3, 0, sizeof(*id3));
	
	fp = fopen(filename, "r");	/* read only */
	if (fp == NULL)
	{
		/* file didn't open */
		DEBUG(("[crossfade] get_id3: file %s didn't open !\n", filename));
		return 0;
	}
	if (fseek(fp, -128, SEEK_END) < 0)
	{
		/* problem rewinding */
		DEBUG(("[crossfade] get_id3: problem rewinding on %s !\n", filename));
		return 0;
	}
	else
	{
		/* we rewound successfully */
		if (fread(id3, 128, 1, fp) < 0)
		{
			/* read error */
			DEBUG(("[crossfade] get_id3: read error on %s !\n", filename));
			return 0;
		}
	}

	return 1;
}
#endif
