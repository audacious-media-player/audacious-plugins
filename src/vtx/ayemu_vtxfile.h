#ifndef _AYEMU_vtxfile_h
#define _AYEMU_vtxfile_h

#include "ayemu_8912.h"

#include <inttypes.h>
#include <libaudcore/vfs.h>

/*
 * The following constants and data structures are from the VTX format
 * description. More information can be found at the libayemu project.
 * https://github.com/asashnov/libayemu/blob/master/contrib/vtx_format.txt
 */

#define AYEMU_VTX_NTSTRING_MAX 255

typedef char NTstring[AYEMU_VTX_NTSTRING_MAX + 1];

/**
 * VTX file format header and status of open file.
 * \internal
 * This structure is used to store VTX file format header's data
 * (song author, title and so on).
 */
struct VTXFileHeader
{
    ayemu_chip_t chiptype; /**< Type of sound chip */
    int stereo;            /**< Type of stereo: 0-ABC, 1-BCA, ... */
    int loop;              /**< Song loop */
    int32_t chipFreq;      /**< AY chip frequency (1773400 for ZX) */
    int playerFreq;        /**< 50 Hz for ZX, 60 Hz for Yamaha */
    int year;              /**< Year song composed */
    NTstring title;        /**< Song title */
    NTstring author;       /**< Song author */
    NTstring from;         /**< Song from */
    NTstring tracker;      /**< Tracker */
    NTstring comment;      /**< Comment */
    size_t regdata_size;   /**< Size of unpacked data, used in uncompression algorithm */
};

/**
 * \defgroup vtxfile Functions to extract data from VTX files.
 */
/*@{*/

/**
 * Structure for VTX file format handler.
 * \internal
 * It stores VTX file header and current state
 * (open file pointer, extracted register data, etc).
 */
struct ayemu_vtx_t
{
    VTXFileHeader hdr;            /**< VTX header data */
    Index<unsigned char> regdata; /**< unpacked song data */
    int pos;                      /**< current data frame offset */

    /**
     * Read VTX file header.
     * \return true on success, else false.
     */
    bool read_header(VFSFile & file);

    /**
     * Read and decode lha data from .vtx file.
     * \return true on success, false when decoding failed.
     */
    bool load_data(VFSFile & file);

    /**
     * Get next 14-bytes frame of AY register data.
     * \return true on success, false if there is not enough data.
     */
    bool get_next_frame(unsigned char * regs);

    /**
     * Print formatted file name.
     * If fmt is nullptr, the default format %a - %t will be used.
     */
    StringBuf sprintname(const char * fmt);
};

/*@}*/

#endif
