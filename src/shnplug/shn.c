/*  shn.c - main functions for xmms-shn
 *  Copyright (C) 2000-2007  Jason Jordan <shnutils@freeshell.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * $Id: shn.c,v 1.38 2007/03/23 05:49:48 jason Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <glib.h>
#include <audacious/util.h>
#include <audacious/configdb.h>
#include "shorten.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static void shn_about(void);
static void shn_configure(void);
static void shn_init(void);
static int  shn_is_our_fd(char *, VFSFile *fd);
static void shn_play(InputPlayback *);
static void shn_stop(InputPlayback *);
static void shn_seek(InputPlayback *, int);
static void shn_pause(InputPlayback *, short);
static void shn_get_file_info(char *,char **,int *);
static void shn_display_file_info(char *);

gchar *shn_fmts[] = { "shn", NULL };

InputPlugin shn_ip =
{
	NULL,
	NULL,
	"SHN Player " VERSION,
	shn_init,
	shn_about,
	shn_configure,
	NULL,
	NULL,
	shn_play,
	shn_stop,
	shn_pause,
	shn_seek,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	shn_get_file_info,
	shn_display_file_info,
	NULL,
	NULL,
	NULL,
	NULL,
	shn_is_our_fd,
	shn_fmts,
};

InputPlugin *shn_iplist[] = { &shn_ip, NULL };

DECLARE_PLUGIN(shnplug, NULL, NULL, shn_iplist, NULL, NULL, NULL, NULL, NULL);

shn_file *shnfile;
shn_config shn_cfg;

static pthread_t decode_thread;
static gboolean audio_error = FALSE;

static void shn_init()
{
	ConfigDb *cfg;

	shn_cfg.error_output_method = ERROR_OUTPUT_DEVNULL;
	shn_cfg.error_output_method_config_name = "error_output_method";
	shn_cfg.seek_tables_path = NULL;
	shn_cfg.seek_tables_path_config_name = "seek_tables_path";
	shn_cfg.relative_seek_tables_path = NULL;
	shn_cfg.relative_seek_tables_path_config_name = "relative_seek_tables_path";
	shn_cfg.verbose = 0;
	shn_cfg.verbose_config_name = "verbose";
	shn_cfg.swap_bytes = 0;
	shn_cfg.swap_bytes_config_name = "swap_bytes";
	shn_cfg.load_textfiles = FALSE;
	shn_cfg.load_textfiles_config_name = "load_textfiles";
	shn_cfg.textfile_extensions = NULL;
	shn_cfg.textfile_extensions_config_name = "textfile_extensions";

	if ((cfg = aud_cfg_db_open()) != 0)
	{
		aud_cfg_db_get_int(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.error_output_method_config_name, &shn_cfg.error_output_method);
		aud_cfg_db_get_bool(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.verbose_config_name, &shn_cfg.verbose);
		if (!aud_cfg_db_get_string(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.seek_tables_path_config_name, &shn_cfg.seek_tables_path))
			shn_cfg.seek_tables_path = g_strdup(g_get_home_dir());
		if (!aud_cfg_db_get_string(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.relative_seek_tables_path_config_name, &shn_cfg.relative_seek_tables_path))
			shn_cfg.relative_seek_tables_path = g_strdup("");
		aud_cfg_db_get_bool(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.swap_bytes_config_name, &shn_cfg.swap_bytes);
		aud_cfg_db_get_bool(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.load_textfiles_config_name, &shn_cfg.load_textfiles);
		if (!aud_cfg_db_get_string(cfg, XMMS_SHN_VERSION_TAG, shn_cfg.textfile_extensions_config_name, &shn_cfg.textfile_extensions))
			shn_cfg.textfile_extensions = g_strdup("txt,nfo");
		aud_cfg_db_close(cfg);
	}
}

static void shn_about()
{
	shn_display_about();
}

static void shn_configure()
{
	shn_display_configure();
}

int init_decode_state(shn_file *this_shn)
{
	if (this_shn->decode_state)
	{
		if (this_shn->decode_state->getbuf)
		{
			free(this_shn->decode_state->getbuf);
			this_shn->decode_state->getbuf = NULL;
		}

		if (this_shn->decode_state->writebuf)
		{
			free(this_shn->decode_state->writebuf);
			this_shn->decode_state->writebuf = NULL;
		}

		if (this_shn->decode_state->writefub)
		{
			free(this_shn->decode_state->writefub);
			this_shn->decode_state->writefub = NULL;
		}

		free(this_shn->decode_state);
		this_shn->decode_state = NULL;
	}

	if (!(this_shn->decode_state = malloc(sizeof(shn_decode_state))))
	{
		shn_debug("Could not allocate memory for decode state data structure");
		return 0;
	}

	this_shn->decode_state->getbuf = NULL;
	this_shn->decode_state->getbufp = NULL;
	this_shn->decode_state->nbitget = 0;
	this_shn->decode_state->nbyteget = 0;
	this_shn->decode_state->gbuffer = 0;
	this_shn->decode_state->writebuf = NULL;
	this_shn->decode_state->writefub = NULL;
	this_shn->decode_state->nwritebuf = 0;

	this_shn->vars.bytes_in_buf = 0;

	return 1;
}

int get_wave_header(shn_file *this_shn)
{
  slong  **buffer = NULL, **offset = NULL;
  slong  lpcqoffset = 0;
  int   version = FORMAT_VERSION, bitshift = 0;
  int   ftype = TYPE_EOF;
  char  *magic = MAGIC;
  int   blocksize = DEFAULT_BLOCK_SIZE, nchan = DEFAULT_NCHAN;
  int   i, chan, nwrap, nskip = DEFAULT_NSKIP;
  int   *qlpc = NULL, maxnlpc = DEFAULT_MAXNLPC, nmean = UNDEFINED_UINT;
  int   cmd;
  int   internal_ftype;
  int   cklen;
  int   retval = 0;

  if (!init_decode_state(this_shn))
    return 0;

    /***********************/
    /* EXTRACT starts here */
    /***********************/

    /* read magic number */
#ifdef STRICT_FORMAT_COMPATABILITY
    if(FORMAT_VERSION < 2)
    {
      for(i = 0; i < strlen(magic); i++) {
        if(getc_exit(this_shn->vars.fd) != magic[i])
          return 0;
        this_shn->vars.bytes_read++;
      }

      /* get version number */
      version = getc_exit(this_shn->vars.fd);
      this_shn->vars.bytes_read++;
    }
    else
#endif /* STRICT_FORMAT_COMPATABILITY */
    {
      int nscan = 0;

      version = MAX_VERSION + 1;
      while(version > MAX_VERSION)
      {
        int byte = aud_vfs_getc(this_shn->vars.fd);
        this_shn->vars.bytes_read++;
        if(byte == EOF)
          return 0;
        if(magic[nscan] != '\0' && byte == magic[nscan])
          nscan++;
        else
          if(magic[nscan] == '\0' && byte <= MAX_VERSION)
            version = byte;
        else
        {
          if(byte == magic[0])
            nscan = 1;
          else
          {
            nscan = 0;
          }
         version = MAX_VERSION + 1;
        }
      }
    }

    /* check version number */
    if(version > MAX_SUPPORTED_VERSION)
      return 0;

    /* set up the default nmean, ignoring the command line state */
    nmean = (version < 2) ? DEFAULT_V0NMEAN : DEFAULT_V2NMEAN;

    /* initialise the variable length file read for the compressed stream */
    var_get_init(this_shn);
    if (this_shn->vars.fatal_error)
      return 0;

    /* initialise the fixed length file write for the uncompressed stream */
    fwrite_type_init(this_shn);

    /* get the internal file type */
    internal_ftype = UINT_GET(TYPESIZE, this_shn);

    /* has the user requested a change in file type? */
    if(internal_ftype != ftype) {
      if(ftype == TYPE_EOF) {
        ftype = internal_ftype;    /*  no problems here */
      }
      else {           /* check that the requested conversion is valid */
        if(internal_ftype == TYPE_AU1 || internal_ftype == TYPE_AU2 ||
           internal_ftype == TYPE_AU3 || ftype == TYPE_AU1 ||ftype == TYPE_AU2 || ftype == TYPE_AU3)
        {
          retval = 0;
          goto got_enough_data;
        }
      }
    }

    nchan = UINT_GET(CHANSIZE, this_shn);
    this_shn->vars.actual_nchan = nchan;

    /* get blocksize if version > 0 */
    if(version > 0)
    {
      int byte;
      blocksize = UINT_GET((int) (log((double) DEFAULT_BLOCK_SIZE) / M_LN2),this_shn);
      maxnlpc = UINT_GET(LPCQSIZE, this_shn);
      this_shn->vars.actual_maxnlpc = maxnlpc;
      nmean = UINT_GET(0, this_shn);
      this_shn->vars.actual_nmean = nmean;
      nskip = UINT_GET(NSKIPSIZE, this_shn);
      for(i = 0; i < nskip; i++)
      {
        byte = uvar_get(XBYTESIZE,this_shn);
      }
    }
    else
      blocksize = DEFAULT_BLOCK_SIZE;

    nwrap = MAX(NWRAP, maxnlpc);

    /* grab some space for the input buffer */
    buffer  = long2d((ulong) nchan, (ulong) (blocksize + nwrap),this_shn);
    if (this_shn->vars.fatal_error)
      return 0;
    offset  = long2d((ulong) nchan, (ulong) MAX(1, nmean),this_shn);
    if (this_shn->vars.fatal_error) {
      if (buffer) {
        free(buffer);
        buffer = NULL;
      }
      return 0;
    }

    for(chan = 0; chan < nchan; chan++)
    {
      for(i = 0; i < nwrap; i++)
      	buffer[chan][i] = 0;
      buffer[chan] += nwrap;
    }

    if(maxnlpc > 0) {
      qlpc = (int*) pmalloc((ulong) (maxnlpc * sizeof(*qlpc)),this_shn);
      if (this_shn->vars.fatal_error) {
        if (buffer) {
          free(buffer);
          buffer = NULL;
        }
        if (offset) {
          free(offset);
          buffer = NULL;
        }
        return 0;
      }
    }

    if(version > 1)
      lpcqoffset = V2LPCQOFFSET;

    init_offset(offset, nchan, MAX(1, nmean), internal_ftype);

    /* get commands from file and execute them */
    chan = 0;
    while(1)
    {
      this_shn->vars.reading_function_code = 1;
      cmd = uvar_get(FNSIZE,this_shn);
      this_shn->vars.reading_function_code = 0;

        switch(cmd)
        {
          case FN_ZERO:
          case FN_DIFF0:
          case FN_DIFF1:
          case FN_DIFF2:
          case FN_DIFF3:
          case FN_QLPC:
          {
            slong coffset, *cbuffer = buffer[chan];
            int resn = 0, nlpc, j;

            if(cmd != FN_ZERO)
            {
              resn = uvar_get(ENERGYSIZE,this_shn);
              if (this_shn->vars.fatal_error) {
                retval = 0;
                goto got_enough_data;
              }
              /* this is a hack as version 0 differed in definition of var_get */
              if(version == 0)
                resn--;
            }

            /* find mean offset : N.B. this code duplicated */
            if(nmean == 0)
              coffset = offset[chan][0];
            else
            {
              slong sum = (version < 2) ? 0 : nmean / 2;
              for(i = 0; i < nmean; i++)
                sum += offset[chan][i];
              if(version < 2)
                coffset = sum / nmean;
              else
                coffset = ROUNDEDSHIFTDOWN(sum / nmean, bitshift);
            }

            switch(cmd)
            {
              case FN_ZERO:
                for(i = 0; i < blocksize; i++)
                  cbuffer[i] = 0;
                break;
              case FN_DIFF0:
                for(i = 0; i < blocksize; i++) {
                  cbuffer[i] = var_get(resn,this_shn) + coffset;
                  if (this_shn->vars.fatal_error) {
                    retval = 0;
                    goto got_enough_data;
                  }
                }
                break;
              case FN_DIFF1:
                for(i = 0; i < blocksize; i++) {
                  cbuffer[i] = var_get(resn,this_shn) + cbuffer[i - 1];
                  if (this_shn->vars.fatal_error) {
                    retval = 0;
                    goto got_enough_data;
                  }
                }
                break;
              case FN_DIFF2:
                for(i = 0; i < blocksize; i++) {
                  cbuffer[i] = var_get(resn,this_shn) + (2 * cbuffer[i - 1] -	cbuffer[i - 2]);
                  if (this_shn->vars.fatal_error) {
                    retval = 0;
                    goto got_enough_data;
                  }
                }
                break;
              case FN_DIFF3:
                for(i = 0; i < blocksize; i++) {
                  cbuffer[i] = var_get(resn,this_shn) + 3 * (cbuffer[i - 1] -  cbuffer[i - 2]) + cbuffer[i - 3];
                  if (this_shn->vars.fatal_error) {
                    retval = 0;
                    goto got_enough_data;
                  }
                }
                break;
              case FN_QLPC:
                nlpc = uvar_get(LPCQSIZE,this_shn);
                if (this_shn->vars.fatal_error) {
                  retval = 0;
                  goto got_enough_data;
                }

                for(i = 0; i < nlpc; i++) {
                  qlpc[i] = var_get(LPCQUANT,this_shn);
                  if (this_shn->vars.fatal_error) {
                    retval = 0;
                    goto got_enough_data;
                  }
                }
                for(i = 0; i < nlpc; i++)
                  cbuffer[i - nlpc] -= coffset;
                for(i = 0; i < blocksize; i++)
                {
                  slong sum = lpcqoffset;

                  for(j = 0; j < nlpc; j++)
                    sum += qlpc[j] * cbuffer[i - j - 1];
                  cbuffer[i] = var_get(resn,this_shn) + (sum >> LPCQUANT);
                  if (this_shn->vars.fatal_error) {
                    retval = 0;
                    goto got_enough_data;
                  }
                }
                if(coffset != 0)
                  for(i = 0; i < blocksize; i++)
                    cbuffer[i] += coffset;
                break;
            }

            /* store mean value if appropriate : N.B. Duplicated code */
            if(nmean > 0)
            {
              slong sum = (version < 2) ? 0 : blocksize / 2;

              for(i = 0; i < blocksize; i++)
                sum += cbuffer[i];

              for(i = 1; i < nmean; i++)
                offset[chan][i - 1] = offset[chan][i];
              if(version < 2)
                offset[chan][nmean - 1] = sum / blocksize;
              else
                offset[chan][nmean - 1] = (sum / blocksize) << bitshift;
            }

			if (0 == chan) {
              this_shn->vars.initial_file_position = this_shn->vars.last_file_position_no_really;
              goto got_enough_data;
            }

            /* do the wrap */
            for(i = -nwrap; i < 0; i++)
              cbuffer[i] = cbuffer[i + blocksize];

            fix_bitshift(cbuffer, blocksize, bitshift, internal_ftype);

            if(chan == nchan - 1)
            {
              fwrite_type(buffer, ftype, nchan, blocksize, this_shn);
              this_shn->vars.bytes_in_buf = 0;
            }

            chan = (chan + 1) % nchan;
            break;
          }
          break;

          case FN_BLOCKSIZE:
            UINT_GET((int) (log((double) blocksize) / M_LN2), this_shn);
            break;

          case FN_VERBATIM:
            cklen = uvar_get(VERBATIM_CKSIZE_SIZE,this_shn);

            while (cklen--) {
              if (this_shn->vars.bytes_in_header >= OUT_BUFFER_SIZE) {
                  shn_debug("Unexpectedly large header - " PACKAGE " can only handle a maximum of %d bytes",OUT_BUFFER_SIZE);
                  goto got_enough_data;
              }
              this_shn->vars.bytes_in_buf = 0;
              this_shn->vars.header[this_shn->vars.bytes_in_header++] = (char)uvar_get(VERBATIM_BYTE_SIZE,this_shn);
            }
            retval = 1;
            break;

          case FN_BITSHIFT:
            bitshift = uvar_get(BITSHIFTSIZE,this_shn);
            this_shn->vars.actual_bitshift = bitshift;
            break;

          default:
            goto got_enough_data;
        }
    }

got_enough_data:

    /* wind up */
    var_get_quit(this_shn);
    fwrite_type_quit(this_shn);

    if (buffer) free((void *) buffer);
    if (offset) free((void *) offset);
    if(maxnlpc > 0 && qlpc)
      free((void *) qlpc);

    this_shn->vars.bytes_in_buf = 0;

    return retval;
}

void shn_unload(shn_file *this_shn)
{
	int this_shn_is_shnfile = (this_shn == shnfile) ? 1 : 0;

	if (this_shn)
	{
		if (this_shn->vars.fd)
		{
			aud_vfs_fclose(this_shn->vars.fd);
			this_shn->vars.fd = NULL;
		}

		if (this_shn->decode_state)
		{
			if (this_shn->decode_state->getbuf)
			{
				free(this_shn->decode_state->getbuf);
				this_shn->decode_state->getbuf = NULL;
			}

			if (this_shn->decode_state->writebuf)
			{
				free(this_shn->decode_state->writebuf);
				this_shn->decode_state->writebuf = NULL;
			}

			if (this_shn->decode_state->writefub)
			{
				free(this_shn->decode_state->writefub);
				this_shn->decode_state->writefub = NULL;
			}

			free(this_shn->decode_state);
			this_shn->decode_state = NULL;
		}

		if (this_shn->seek_table)
		{
			free(this_shn->seek_table);
			this_shn->seek_table = NULL;
		}

		free(this_shn);
		this_shn = NULL;
		if (this_shn_is_shnfile)
			shnfile = NULL;
	}
}

shn_file *load_shn(InputPlayback *playback, char *filename, VFSFile *fd)
{
	shn_file *tmp_file;
	shn_seek_entry *first_seek_table;

	shn_debug("Loading file: '%s'", filename);

	if (!(tmp_file = malloc(sizeof(shn_file))))
	{
		shn_debug("Could not allocate memory for SHN data structure");
		return NULL;
	}

	memset(tmp_file, 0, sizeof(shn_file));

	tmp_file->vars.fd = NULL;
	tmp_file->vars.seek_to = -1;
	tmp_file->vars.eof = 0;
	tmp_file->vars.going = 0;
	tmp_file->vars.playback = playback;
	tmp_file->vars.seek_table_entries = NO_SEEK_TABLE;
	tmp_file->vars.bytes_in_buf = 0;
	tmp_file->vars.bytes_in_header = 0;
	tmp_file->vars.reading_function_code = 0;
	tmp_file->vars.initial_file_position = 0;
	tmp_file->vars.last_file_position = 0;
	tmp_file->vars.last_file_position_no_really = 0;
	tmp_file->vars.bytes_read = 0;
	tmp_file->vars.actual_bitshift = 0;
	tmp_file->vars.actual_maxnlpc = 0;
	tmp_file->vars.actual_nmean = 0;
	tmp_file->vars.actual_nchan = 0;
	tmp_file->vars.seek_offset = 0;

	tmp_file->decode_state = NULL;

	tmp_file->wave_header.filename = filename;
	tmp_file->wave_header.wave_format = 0;
	tmp_file->wave_header.channels = 0;
	tmp_file->wave_header.block_align = 0;
	tmp_file->wave_header.bits_per_sample = 0;
	tmp_file->wave_header.samples_per_sec = 0;
	tmp_file->wave_header.avg_bytes_per_sec = 0;
	tmp_file->wave_header.rate = 0;
	tmp_file->wave_header.header_size = 0;
	tmp_file->wave_header.data_size = 0;
	tmp_file->wave_header.file_has_id3v2_tag = 0;
	tmp_file->wave_header.id3v2_tag_size = 0;

	tmp_file->seek_header.version = NO_SEEK_TABLE;
	tmp_file->seek_header.shnFileSize = 0;

	tmp_file->seek_trailer.seekTableSize = 0;

	tmp_file->seek_table = NULL;

	if (!fd)
	{
		if (!(tmp_file->vars.fd = shn_open_and_discard_id3v2_tag(filename,&tmp_file->wave_header.file_has_id3v2_tag,&tmp_file->wave_header.id3v2_tag_size)))
		{
			shn_debug("Could not open file: '%s'",filename);
			shn_unload(tmp_file);
			return NULL;
		}
	}
	else
		tmp_file->vars.fd = fd;

	if (0 == get_wave_header(tmp_file))
	{
		shn_debug("Unable to read WAVE header from file '%s'",filename);
		shn_unload(tmp_file);
		return NULL;
	}

	if (tmp_file->wave_header.file_has_id3v2_tag)
	{
		aud_vfs_fseek(tmp_file->vars.fd,tmp_file->wave_header.id3v2_tag_size,SEEK_SET);
		tmp_file->vars.bytes_read += tmp_file->wave_header.id3v2_tag_size;
		tmp_file->vars.seek_offset = tmp_file->wave_header.id3v2_tag_size;
	}
    else
	{
		aud_vfs_fseek(tmp_file->vars.fd,0,SEEK_SET);
	}

	if (0 == shn_verify_header(tmp_file))
	{
		shn_debug("Invalid WAVE header in file: '%s'",filename);
		shn_unload(tmp_file);
		return NULL;
	}

	if (tmp_file->decode_state)
	{
		free(tmp_file->decode_state);
		tmp_file->decode_state = NULL;
	}

	shn_load_seek_table(tmp_file,filename);

	if (NO_SEEK_TABLE != tmp_file->vars.seek_table_entries)
	{
		/* verify seek tables */

		first_seek_table = (shn_seek_entry *)tmp_file->seek_table;

		if (tmp_file->vars.actual_bitshift != shn_uchar_to_ushort_le(first_seek_table->data+22))
		{
			/* initial bitshift value in the file does not match the first bitshift value of the first seektable entry - seeking is broken */
			shn_debug("Broken seek table detected (invalid bitshift) - seeking disabled for this file.");
			tmp_file->vars.seek_table_entries = NO_SEEK_TABLE;
		}
		else if (tmp_file->vars.actual_nchan > 2)
		{
			/* nchan is greater than the number of such entries stored in a seek table entry - seeking won't work */
			shn_debug("Broken seek table detected (nchan %d not in range [1 .. 2]) - seeking disabled for this file.",tmp_file->vars.actual_nchan);
			tmp_file->vars.seek_table_entries = NO_SEEK_TABLE;
		}
		else if (tmp_file->vars.actual_maxnlpc > 3)
		{
			/* maxnlpc is greater than the number of such entries stored in a seek table entry - seeking won't work */
			shn_debug("Broken seek table detected (maxnlpc %d not in range [0 .. 3]) - seeking disabled for this file.",tmp_file->vars.actual_maxnlpc);
			tmp_file->vars.seek_table_entries = NO_SEEK_TABLE;
		}
		else if (tmp_file->vars.actual_nmean > 4)
		{
			/* nmean is greater than the number of such entries stored in a seek table entry - seeking won't work */
			shn_debug("Broken seek table detected (nmean %d not in range [0 .. 4]) - seeking disabled for this file.",tmp_file->vars.actual_nmean);
			tmp_file->vars.seek_table_entries = NO_SEEK_TABLE;
		}
		else
		{
			/* seek table appears to be valid - now adjust byte offsets in seek table to match the file */
			tmp_file->vars.seek_offset += tmp_file->vars.initial_file_position - shn_uchar_to_ulong_le(first_seek_table->data+8);

			if (0 != tmp_file->vars.seek_offset)
			{
				shn_debug("Adjusting seek table offsets by %ld bytes due to mismatch between seek table values and input file - seeking might not work correctly.",
					tmp_file->vars.seek_offset);
			}
		}
	}

	shn_debug("Successfully loaded file: '%s'",filename);

	return tmp_file;
}

static int shn_is_our_fd(char *fn, VFSFile *fd)
{
	char data[4];

	if (aud_vfs_fread((void *)data,1,4,fd) != 4)
		return FALSE;

	if (memcmp(data,MAGIC,4))
		return FALSE;

#if 0
	if (!(tmp_file = load_shn(NULL, filename, fd)))
		return FALSE;

	shn_unload(tmp_file);
#endif

	return TRUE;
}

void swap_bytes(shn_file *this_shn,int bytes)
{
	int i;
	uchar tmp;

	for (i=0;i<bytes;i=i+2) {
		tmp = this_shn->vars.buffer[i+1];
		this_shn->vars.buffer[i+1] = this_shn->vars.buffer[i];
		this_shn->vars.buffer[i] = tmp;
	}
}

void write_and_wait(shn_file *this_shn,int block_size)
{
	int bytes_to_write,bytes_in_block,i;
	InputPlayback *playback = this_shn->vars.playback;

	if (this_shn->vars.bytes_in_buf < block_size)
		return;

	bytes_in_block = min(this_shn->vars.bytes_in_buf, block_size);

	if (bytes_in_block <= 0)
		return;

	bytes_to_write = bytes_in_block;
	while ((bytes_to_write + bytes_in_block) <= this_shn->vars.bytes_in_buf)
		bytes_to_write += bytes_in_block;

	shn_ip.add_vis_pcm(shn_ip.output->written_time(), (this_shn->wave_header.bits_per_sample == 16) ? FMT_S16_LE : FMT_U8,
		this_shn->wave_header.channels, bytes_to_write, this_shn->vars.buffer);

	while(shn_ip.output->buffer_free() < bytes_to_write && playback->playing && this_shn->vars.seek_to == -1)
		g_usleep(10000);

	if(playback->playing && this_shn->vars.seek_to == -1) {
		if (shn_cfg.swap_bytes)
			swap_bytes(this_shn, bytes_to_write);
		shn_ip.output->write_audio(this_shn->vars.buffer, bytes_to_write);
	} else
		return;

	/* shift data from end of buffer to the front */
	this_shn->vars.bytes_in_buf -= bytes_to_write;

	for(i=0;i<this_shn->vars.bytes_in_buf;i++)
		this_shn->vars.buffer[i] = this_shn->vars.buffer[i+bytes_to_write];
}

static void *play_loop_shn(void *arg)
{
  slong  **buffer = NULL, **offset = NULL;
  slong  lpcqoffset = 0;
  int   version = FORMAT_VERSION, bitshift = 0;
  int   ftype = TYPE_EOF;
  char  *magic = MAGIC;
  int   blocksize = DEFAULT_BLOCK_SIZE, nchan = DEFAULT_NCHAN;
  int   i, chan, nwrap, nskip = DEFAULT_NSKIP;
  int   *qlpc = NULL, maxnlpc = DEFAULT_MAXNLPC, nmean = UNDEFINED_UINT;
  int   cmd;
  int   internal_ftype;
  shn_file *this_shn = shnfile;
  int   blk_size;
  int   cklen;
  uchar tmp;
  ulong seekto_offset;
  InputPlayback *playback = this_shn->vars.playback;

restart:

  this_shn->vars.bytes_in_buf = 0;

  if (!init_decode_state(this_shn))
    goto exit_thread;

  blk_size = 512 * (this_shn->wave_header.bits_per_sample / 8) * this_shn->wave_header.channels;

    /***********************/
    /* EXTRACT starts here */
    /***********************/

    /* read magic number */
#ifdef STRICT_FORMAT_COMPATABILITY
    if(FORMAT_VERSION < 2)
    {
      for(i = 0; i < strlen(magic); i++)
        if(getc_exit(this_shn->vars.fd) != magic[i]) {
          shn_error_fatal(this_shn,"Bad magic number");
          goto exit_thread;
        }

      /* get version number */
      version = getc_exit(this_shn->vars.fd);
    }
    else
#endif /* STRICT_FORMAT_COMPATABILITY */
    {
      int nscan = 0;

      version = MAX_VERSION + 1;
      while(version > MAX_VERSION)
      {
  	int byte = aud_vfs_getc(this_shn->vars.fd);
 	if(byte == EOF) {
	  shn_error_fatal(this_shn,"No magic number");
          goto exit_thread;
        }
	if(magic[nscan] != '\0' && byte == magic[nscan])
          nscan++;
	else
        if(magic[nscan] == '\0' && byte <= MAX_VERSION)
          version = byte;
	else
        {
	  if(byte == magic[0])
  	    nscan = 1;
	  else
          {
	    nscan = 0;
	  }
	  version = MAX_VERSION + 1;
	}
      }
    }

    /* check version number */
    if(version > MAX_SUPPORTED_VERSION) {
      shn_error_fatal(this_shn,"Can't decode version %d", version);
      goto exit_thread;
    }

    /* set up the default nmean, ignoring the command line state */
    nmean = (version < 2) ? DEFAULT_V0NMEAN : DEFAULT_V2NMEAN;

    /* initialise the variable length file read for the compressed stream */
    var_get_init(this_shn);
    if (this_shn->vars.fatal_error)
      goto exit_thread;

    /* initialise the fixed length file write for the uncompressed stream */
    fwrite_type_init(this_shn);

    /* get the internal file type */
    internal_ftype = UINT_GET(TYPESIZE, this_shn);

    /* has the user requested a change in file type? */
    if(internal_ftype != ftype) {
      if(ftype == TYPE_EOF)
	ftype = internal_ftype;    /*  no problems here */
      else             /* check that the requested conversion is valid */
	if(internal_ftype == TYPE_AU1 || internal_ftype == TYPE_AU2 ||
	   internal_ftype == TYPE_AU3 || ftype == TYPE_AU1 ||ftype == TYPE_AU2 || ftype == TYPE_AU3) {
	  shn_error_fatal(this_shn,"Not able to perform requested output format conversion");
          goto cleanup;
        }
    }

    nchan = UINT_GET(CHANSIZE, this_shn);

    /* get blocksize if version > 0 */
    if(version > 0)
    {
      int byte;
      blocksize = UINT_GET((int) (log((double) DEFAULT_BLOCK_SIZE) / M_LN2),this_shn);
      maxnlpc = UINT_GET(LPCQSIZE, this_shn);
      nmean = UINT_GET(0, this_shn);
      nskip = UINT_GET(NSKIPSIZE, this_shn);
      for(i = 0; i < nskip; i++)
      {
        byte = uvar_get(XBYTESIZE,this_shn);
      }
    }
    else
      blocksize = DEFAULT_BLOCK_SIZE;

    nwrap = MAX(NWRAP, maxnlpc);

    /* grab some space for the input buffer */
    buffer  = long2d((ulong) nchan, (ulong) (blocksize + nwrap),this_shn);
    if (this_shn->vars.fatal_error)
      goto exit_thread;
    offset  = long2d((ulong) nchan, (ulong) MAX(1, nmean),this_shn);
    if (this_shn->vars.fatal_error) {
      if (buffer) {
        free(buffer);
        buffer = NULL;
      }
      goto exit_thread;
    }

    for(chan = 0; chan < nchan; chan++)
    {
      for(i = 0; i < nwrap; i++)
      	buffer[chan][i] = 0;
      buffer[chan] += nwrap;
    }

    if(maxnlpc > 0) {
      qlpc = (int*) pmalloc((ulong) (maxnlpc * sizeof(*qlpc)),this_shn);
      if (this_shn->vars.fatal_error) {
        if (buffer) {
          free(buffer);
          buffer = NULL;
        }
        if (offset) {
          free(offset);
          buffer = NULL;
        }
        goto exit_thread;
      }
    }

    if(version > 1)
      lpcqoffset = V2LPCQOFFSET;

    init_offset(offset, nchan, MAX(1, nmean), internal_ftype);

    /* get commands from file and execute them */
    chan = 0;
    while(1)
    {
        cmd = uvar_get(FNSIZE,this_shn);
        if (this_shn->vars.fatal_error)
          goto cleanup;

        switch(cmd)
        {
          case FN_ZERO:
          case FN_DIFF0:
          case FN_DIFF1:
          case FN_DIFF2:
          case FN_DIFF3:
          case FN_QLPC:
          {
            slong coffset, *cbuffer = buffer[chan];
            int resn = 0, nlpc, j;

            if(cmd != FN_ZERO)
            {
              resn = uvar_get(ENERGYSIZE,this_shn);
              if (this_shn->vars.fatal_error)
                goto cleanup;
              /* this is a hack as version 0 differed in definition of var_get */
              if(version == 0)
                resn--;
            }

            /* find mean offset : N.B. this code duplicated */
            if(nmean == 0)
              coffset = offset[chan][0];
            else
            {
              slong sum = (version < 2) ? 0 : nmean / 2;
              for(i = 0; i < nmean; i++)
                sum += offset[chan][i];
              if(version < 2)
                coffset = sum / nmean;
              else
                coffset = ROUNDEDSHIFTDOWN(sum / nmean, bitshift);
            }

            switch(cmd)
            {
              case FN_ZERO:
                for(i = 0; i < blocksize; i++)
                  cbuffer[i] = 0;
                break;
              case FN_DIFF0:
                for(i = 0; i < blocksize; i++) {
                  cbuffer[i] = var_get(resn,this_shn) + coffset;
                  if (this_shn->vars.fatal_error)
                    goto cleanup;
                }
                break;
              case FN_DIFF1:
                for(i = 0; i < blocksize; i++) {
                  cbuffer[i] = var_get(resn,this_shn) + cbuffer[i - 1];
                  if (this_shn->vars.fatal_error)
                    goto cleanup;
                }
                break;
              case FN_DIFF2:
                for(i = 0; i < blocksize; i++) {
                  cbuffer[i] = var_get(resn,this_shn) + (2 * cbuffer[i - 1] -	cbuffer[i - 2]);
                  if (this_shn->vars.fatal_error)
                    goto cleanup;
                }
                break;
              case FN_DIFF3:
                for(i = 0; i < blocksize; i++) {
                  cbuffer[i] = var_get(resn,this_shn) + 3 * (cbuffer[i - 1] -  cbuffer[i - 2]) + cbuffer[i - 3];
                  if (this_shn->vars.fatal_error)
                    goto cleanup;
                }
                break;
              case FN_QLPC:
                nlpc = uvar_get(LPCQSIZE,this_shn);
                if (this_shn->vars.fatal_error)
                  goto cleanup;

                for(i = 0; i < nlpc; i++) {
                  qlpc[i] = var_get(LPCQUANT,this_shn);
                  if (this_shn->vars.fatal_error)
                    goto cleanup;
                }
                for(i = 0; i < nlpc; i++)
                  cbuffer[i - nlpc] -= coffset;
                for(i = 0; i < blocksize; i++)
                {
                  slong sum = lpcqoffset;

                  for(j = 0; j < nlpc; j++)
                    sum += qlpc[j] * cbuffer[i - j - 1];
                  cbuffer[i] = var_get(resn,this_shn) + (sum >> LPCQUANT);
                  if (this_shn->vars.fatal_error)
                    goto cleanup;
                }
                if(coffset != 0)
                  for(i = 0; i < blocksize; i++)
                    cbuffer[i] += coffset;
                break;
            }

            /* store mean value if appropriate : N.B. Duplicated code */
            if(nmean > 0)
            {
              slong sum = (version < 2) ? 0 : blocksize / 2;

              for(i = 0; i < blocksize; i++)
                sum += cbuffer[i];

              for(i = 1; i < nmean; i++)
                offset[chan][i - 1] = offset[chan][i];
              if(version < 2)
                offset[chan][nmean - 1] = sum / blocksize;
              else
                offset[chan][nmean - 1] = (sum / blocksize) << bitshift;
            }

            /* do the wrap */
            for(i = -nwrap; i < 0; i++)
              cbuffer[i] = cbuffer[i + blocksize];

            fix_bitshift(cbuffer, blocksize, bitshift, internal_ftype);

            if(chan == nchan - 1)
            {
              if (!playback->playing || this_shn->vars.fatal_error)
                goto cleanup;

              fwrite_type(buffer, ftype, nchan, blocksize, this_shn);

              write_and_wait(this_shn,blk_size);

              if (this_shn->vars.seek_to != -1)
              {
                shn_seek_entry *seek_info;
                int j;

                shn_debug("Seeking to %d:%02d",this_shn->vars.seek_to/60,this_shn->vars.seek_to%60);

                seek_info = shn_seek_entry_search(this_shn->seek_table,this_shn->vars.seek_to * (ulong)this_shn->wave_header.samples_per_sec,0,
							      (ulong)(this_shn->vars.seek_table_entries - 1),this_shn->vars.seek_resolution);

                /* loop through number of channels in this file */
                for (i=0;i<nchan;i++) {
                  /* load the three sample buffer values for this channel */
                  for (j=0;j<3;j++)
                    buffer[i][j-3] = shn_uchar_to_slong_le(seek_info->data+32+12*i-4*j);

                  /* load the variable number of offset history values for this channel */
                  for (j=0;j<MAX(1,nmean);j++)
                    offset[i][j]  = shn_uchar_to_slong_le(seek_info->data+48+16*i+4*j);
                }

                bitshift = shn_uchar_to_ushort_le(seek_info->data+22);

                seekto_offset = shn_uchar_to_ulong_le(seek_info->data+8) + this_shn->vars.seek_offset;

                aud_vfs_fseek(this_shn->vars.fd,(slong)seekto_offset,SEEK_SET);
                aud_vfs_fread((uchar*) this_shn->decode_state->getbuf, 1, BUFSIZ, this_shn->vars.fd);

                this_shn->decode_state->getbufp = this_shn->decode_state->getbuf + shn_uchar_to_ushort_le(seek_info->data+14);
                this_shn->decode_state->nbitget = shn_uchar_to_ushort_le(seek_info->data+16);
                this_shn->decode_state->nbyteget = shn_uchar_to_ushort_le(seek_info->data+12);
                this_shn->decode_state->gbuffer = shn_uchar_to_ulong_le(seek_info->data+18);

                this_shn->vars.bytes_in_buf = 0;

                shn_ip.output->flush(this_shn->vars.seek_to * 1000);
                this_shn->vars.seek_to = -1;
              }

            }
            chan = (chan + 1) % nchan;
            break;
          }

          break;

          case FN_QUIT:
            /* empty out last of buffer */
            write_and_wait(this_shn,this_shn->vars.bytes_in_buf);

            playback->eof = TRUE;

            while (1)
            {
              if (!playback->playing)
                goto finish;
              if (this_shn->vars.seek_to != -1)
              {
                var_get_quit(this_shn);
                fwrite_type_quit(this_shn);

                if (buffer) free((void *) buffer);
                if (offset) free((void *) offset);
                if(maxnlpc > 0 && qlpc)
                  free((void *) qlpc);

                aud_vfs_fseek(this_shn->vars.fd,0,SEEK_SET);
                goto restart;
              }
              else
                g_usleep(10000);
            }

            goto cleanup;
            break;

          case FN_BLOCKSIZE:
            blocksize = UINT_GET((int) (log((double) blocksize) / M_LN2), this_shn);
            if (this_shn->vars.fatal_error)
              goto cleanup;
            break;
          case FN_BITSHIFT:
            bitshift = uvar_get(BITSHIFTSIZE,this_shn);
            if (this_shn->vars.fatal_error)
              goto cleanup;
            break;
          case FN_VERBATIM:
            cklen = uvar_get(VERBATIM_CKSIZE_SIZE,this_shn);
            if (this_shn->vars.fatal_error)
              goto cleanup;

            while (cklen--) {
              tmp = (uchar)uvar_get(VERBATIM_BYTE_SIZE,this_shn);
              if (this_shn->vars.fatal_error)
                goto cleanup;
            }

            break;

          default:
            shn_error_fatal(this_shn,"Sanity check fails trying to decode function: %d",cmd);
            goto cleanup;
        }
    }

cleanup:

    write_and_wait(this_shn,this_shn->vars.bytes_in_buf);
    shn_ip.output->buffer_free();
    shn_ip.output->buffer_free();
    g_usleep(10000);

finish:

    this_shn->vars.seek_to = -1;
    playback->eof = TRUE;

    /* wind up */
    var_get_quit(this_shn);
    fwrite_type_quit(this_shn);

    if (buffer) free((void *) buffer);
    if (offset) free((void *) offset);
    if(maxnlpc > 0 && qlpc)
      free((void *) qlpc);

exit_thread:

    pthread_exit(NULL);
}

static void shn_play(InputPlayback *playback)
{
	char *name, *temp;
	char *filename = playback->filename;

	audio_error = FALSE;

	if (!(shnfile = load_shn(playback, playback->filename, NULL)))
	{
		shn_debug("Could not load file for playing: '%s'", playback->filename);
		return;
	}

	aud_vfs_fseek(shnfile->vars.fd,0,SEEK_SET);

	playback->playing = TRUE;

	if (shn_ip.output->open_audio((shnfile->wave_header.bits_per_sample == 16) ? FMT_S16_LE : FMT_U8, shnfile->wave_header.samples_per_sec, shnfile->wave_header.channels) == 0)
	{
		audio_error = TRUE;
		shn_debug("Could not open audio device for playback (check your output plugin configuration)");
		return;
	}
	temp = strrchr(filename, '/');
	if (!temp)
		temp = filename;
	else
		temp++;
	name = malloc(strlen(temp) + 1);
	strcpy(name, temp);
	if (shn_filename_contains_a_dot(name))
		*(strrchr(name,'.')) = '\0';
	shn_ip.set_info(name, 1000 * shnfile->wave_header.length, 8 * shnfile->wave_header.rate, shnfile->wave_header.samples_per_sec, shnfile->wave_header.channels);
	free(name);
	shnfile->vars.seek_to = -1;
	pthread_create(&decode_thread, NULL, play_loop_shn, NULL);
}

static void shn_stop(InputPlayback *playback)
{
	int was_fatal;
	char error_msg[BUF_SIZE];

	if (!shnfile)
		return;

	if ((was_fatal = shnfile->vars.fatal_error))
		shn_snprintf(error_msg,BUF_SIZE,"%s.\nAffected file was:\n%s",shnfile->vars.fatal_error_msg,shnfile->wave_header.filename);

	if (playback->playing || was_fatal)
	{
		playback->playing = FALSE;
		pthread_join(decode_thread, NULL);
		shn_ip.output->close_audio();
		shn_unload(shnfile);
	}

	if (was_fatal)
		shn_error(error_msg);
}

static void shn_pause(InputPlayback *playback, short p)
{
	playback->output->pause(p);
}

static void shn_seek(InputPlayback *playback, int time)
{
	if (NULL == shnfile)
		return;

	if (shnfile->vars.seek_table_entries == NO_SEEK_TABLE)
	{
		shn_error("Cannot seek to %d:%02d because there is no seek information for this file.",time/60,time%60);
		return;
	}

	shnfile->vars.seek_to = time;

	while (shnfile->vars.seek_to != -1)
		g_usleep(10000);
}

static void shn_get_file_info(char *filename, char **title, int *length)
{
	char *name, *temp;
	shn_file *tmp_file;

	temp = strrchr(filename, '/');
	if (!temp)
		temp = filename;
	else
		temp++;

	name = g_malloc(strlen(temp) + 1);
	strcpy(name, temp);

	if (shn_filename_contains_a_dot(name))
		*(strrchr(name,'.')) = '\0';

	*title = name;

	*length = 0;

	if (!(tmp_file = load_shn(NULL, filename, NULL)))
	{
		shn_debug("Could not get information from file: '%s'",filename);
		return;
	}

	*length = 1000 * tmp_file->wave_header.length;

	shn_unload(tmp_file);
}

static void shn_display_file_info(char *filename)
{
	shn_file *tmp_file;

	if (!(tmp_file = load_shn(NULL, filename, NULL)))
	{
		shn_debug("Could not get information from file: '%s'",filename);
		return;
	}

	shn_display_info(tmp_file);

	shn_unload(tmp_file);
}
