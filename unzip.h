//
// Unzip library
// Written by Larry Bank
// Copyright (c) 2021 BitBank Software, Inc.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
//   This library is free software; you can redistribute it and/or
//   modify it under the terms of the GNU Lesser General Public
//   License as published by the Free Software Foundation; either
//   version 2.1 of the License, or (at your option) any later version.
//
//   This library is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   Lesser General Public License for more details.
//
//   You should have received a copy of the GNU Lesser General Public
//   License along with this library; if not, write to the Free Software
//   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
#ifndef __UNZIP__
#define __UNZIP__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "zlib.h"


#ifdef __cplusplus
extern "C" {
#endif

// Defines and typedefs
#define UNZ_MAX_FILENAME_LEN 256
#define UNZ_MAX_COMMENT_LEN 256
#define UNZ_MAX_EXTRA_LEN 256

enum {
    UNZ_OK = 0,
    UNZ_BAD_PARAM,
    UNZ_BAD_ZIP_FILE,
    UNZ_UNSUPPORTED_METHOD,
    UNZ_CRC_ERROR,
    UNZ_BAD_DECOMP_STATE,
    UNZ_FILE_NOT_FOUND,
    UNZ_CANT_OPEN_FILE
};

#define UNZ_DEFLATE_METHOD 8

// Structure forward declarations
typedef struct unz_file_info_tag UNZ_FILE_INFO;
typedef struct unz_file_tag UNZ_FILE;
//
// Read a 32-bit signed integer from a byte array
//
static int32_t read_int32(const uint8_t *p)
{
    return (int32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}
//
// Read a 16-bit unsigned integer from a byte array
//
static uint16_t read_uint16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}
//
// Read a 32-bit unsigned integer from a byte array
//
static uint32_t read_uint32(const uint8_t *p)
{
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

// unz_file_info_t
// Information about a file in the zip archive
//
struct unz_file_info_tag
{
    uint16_t version;
    uint16_t version_needed;
    uint16_t flag;
    uint16_t compression_method;
    uint32_t dosDate;
    uint32_t crc;
    uint64_t compressed_size;
    uint64_t uncompressed_size;
    uint16_t size_filename;
    uint16_t size_file_extra;
    uint16_t size_file_comment;
    uint16_t disk_num_start;
    uint16_t internal_fa;
    uint32_t external_fa;
    char filename[UNZ_MAX_FILENAME_LEN];
    char comment[UNZ_MAX_COMMENT_LEN];
};


// C functions
int unzOpen(const char *path, UNZ_FILE *pF);
int unzClose(UNZ_FILE *p F);
int unzGetFileCount(UNZ_FILE *pF);
int unzGetFileInfo(UNZ_FILE *pF, int i, UNZ_FILE_INFO *pInfo);
int unzLocateFile(UNZ_FILE *pF, const char *szFilename);
int unzOpenCurrentFile(UNZ_FILE *pF);
int unzCloseCurrentFile(UNZ_FILE *pF);
int unzReadCurrentFile(UNZ_FILE *pF, uint8_t *pBuf, int iLen);
int unzEndOfFile(UNZ_FILE *pF);

#ifdef __cplusplus
}

// C++ Class
class Unzip
{
public:
    Unzip();
    ~Unzip();
    int open(const char *filepath);
    void close();
    int getFileCount();
    int getFileInfo(int i, UNZ_FILE_INFO *pInfo);
    int locateFile(const char *szFilename);
    int openCurrentFile();
    void closeCurrentFile();
    int readCurrentFile(uint8_t *pBuf, int iLen);
    int eof();

private:
    UNZ_FILE *m_pZip;
};

#endif // __cplusplus
#endif // __UNZIP__