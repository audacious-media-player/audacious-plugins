#ifndef _AYEMU_vtxfile_h
#define _AYEMU_vtxfile_h

#include <inttypes.h>
#include <libaudcore/vfs.h>
#include "ayemu_8912.h"

/* The following constants and data structure comes from
   VTX file format description. You can see them on
   http://
*/

#define AYEMU_VTX_NTSTRING_MAX 255

typedef char NTstring[AYEMU_VTX_NTSTRING_MAX+1];

/** VTX file format header and status of open file
 * \internal
 *
 * This structure used for save VTX file format header's data
 * (song author, title and so on).
 */
struct VTXFileHeader
{
  ayemu_chip_t chiptype;    /**< Type of sound chip */
  int      stereo;	    /**< Type of stereo: 0-ABC, 1-BCA... (see VTX format description) */
  int      loop;	    /**< song loop */
  int32_t    chipFreq;	    /**< AY chip freq (1773400 for ZX) */
  int      playerFreq;	    /**< 50 Hz for ZX, 60 Hz for yamaha */
  int      year;            /**< year song composed */
  NTstring title;		/**< song title  */
  NTstring author;		/**< song author  */
  NTstring from;		/**< song from */
  NTstring tracker;		/**< tracker */
  NTstring comment;		/**< comment */
  size_t   regdata_size;	/**< size of unpacked data. Used in uncompression alhoritm. */
};

/**
 * \defgroup vtxfile Functions for extract data from vtx files
 */
/*@{*/


/** structure for VTX file format handler
 * \internal
 * It stores VTX file header and current state
 * (open file pointer, extracted register data, etc).
 */
struct ayemu_vtx_t
{
  VTXFileHeader hdr;             /**< VTX header data */
  Index<unsigned char> regdata;  /**< unpacked song data */
  int pos;                       /**< current data frame offset */

  /** Read vtx file header
      \return Return true if success, else false
  */
  bool read_header(VFSFile &file);

  /** Read and encode lha data from .vtx file.
      \return Return true if success, else false
   */
  bool load_data(VFSFile &file);

  /** Get next 14-bytes frame of AY register data.
   * \return Return value: true if success, false if not enough data.
   */
  bool get_next_frame(unsigned char *regs);

  /** Print formatted file name. If fmt is nullptr the default format %a - %t will be used
   * \return none.
   */
  StringBuf sprintname(const char *fmt);
};

/*@}*/

#endif
