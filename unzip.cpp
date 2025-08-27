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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "unzip.h"
#include "zlib.h"

//
// Zip file structures
//
#define CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE 0x02014b50
#define LOCAL_FILE_HEADER_SIGNATURE 0x04034b50
#define END_OF_CENTRAL_DIRECTORY_SIGNATURE 0x06054b50

#ifdef _WIN32
#pragma pack(push,1)
#endif
// End of central directory record
struct EOCD
{
    uint16_t u16Disk;
    uint16_t u16StartDisk;
    uint16_t u16Entries;
    uint16_t u16TotalEntries;
    uint32_t u32Size;
    uint32_t u32Offset;
    uint16_t u16CommentLen;
};
// Central directory file header
struct CDFH
{
    uint16_t u16Version;
    uint16_t u16VersionNeeded;
    uint16_t u16Flags;
    uint16_t u16Method;
    uint16_t u16LastTime;
    uint16_t u16LastDate;
    uint32_t u32CRC;
    uint32_t u32Compressed;
    uint32_t u32Uncompressed;
    uint16_t u16FilenameLen;
    uint16_t u16ExtraLen;
    uint16_t u16CommentLen;
    uint16_t u16Disk;
    uint16_t u16Internal;
    uint32_t u32External;
    uint32_t u32Offset;
};

// Local file header
struct LFH
{
    uint16_t u16Version;
    uint16_t u16Flags;
    uint16_t u16Method;
    uint16_t u16LastTime;
    uint16_t u16LastDate;
    uint32_t u32CRC;
    uint32_t u32Compressed;
    uint32_t u32Uncompressed;
    uint16_t u16FilenameLen;
    uint16_t u16ExtraLen;
};
#ifdef _WIN32
#pragma pack(pop)
#endif
// File info for the current file
struct UNZ_FILE_tag
{
    FILE *f;
    struct EOCD eocd;
    struct CDFH cdfh;
    uint32_t u32FileOffset;
    uint32_t u32HeaderOffset;
    int iCurrentFile;
    int iTotalFiles;
    z_stream stream;
};

static int unzlocal_read(UNZ_FILE *pF, void *buf, int iLen)
{
    return (int)fread(buf, 1, iLen, pF->f);
} /* unzlocal_read() */

int unzOpen(const char *path, UNZ_FILE *pF)
{
    uint8_t sig[4];
    long lPos;
    int i, iCommentLen, iEntries;
    struct EOCD *pEOCD;
    uint8_t *pBuf;

    if (pF == NULL || path == NULL)
        return UNZ_BAD_PARAM;

    memset(pF, 0, sizeof(UNZ_FILE));
    pF->f = fopen(path, "rb");
    if (pF->f == NULL)
    {
        return UNZ_CANT_OPEN_FILE;
    }
    //
    // Find the end of central directory record
    // by scanning backwards from the end of the file
    //
    fseek(pF->f, 0, SEEK_END);
    lPos = ftell(pF->f);
    if (lPos < sizeof(struct EOCD))
    {
        fclose(pF->f);
        return UNZ_BAD_ZIP_FILE;
    }
    fseek(pF->f, -sizeof(struct EOCD), SEEK_END);
    lPos = ftell(pF->f);
    i = 0;
    // search backwards for the signature
    while (i < 65535 && i < lPos)
    {
        fseek(pF->f, lPos - i, SEEK_SET);
        if (unzlocal_read(pF, sig, 4) == 4)
        {
            if (read_uint32(sig) == END_OF_CENTRAL_DIRECTORY_SIGNATURE)
            {
                pBuf = (uint8_t *)malloc(65536);
                if (pBuf)
                {
                    fseek(pF->f, lPos-i, SEEK_SET);
                    unzlocal_read(pF, pBuf, (int)ftell(pF->f) - (lPos-i));
                    pEOCD = (struct EOCD *)&pBuf[4]; // skip signature
                    iCommentLen = read_uint16((uint8_t *)&pEOCD->u16CommentLen);
                    iEntries = read_uint16((uint8_t *)&pEOCD->u16Entries);
                    // Check if the comment length and number of entries is plausible
                    if (iCommentLen < 4096 && (i + sizeof(struct EOCD) + iCommentLen) < 65536 && iEntries < 32768)
                    {
                        memcpy(&pF->eocd, pEOCD, sizeof(struct EOCD));
                        free(pBuf);
                        pF->iTotalFiles = read_uint16((uint8_t *)&pF->eocd.u16TotalEntries);
                        return UNZ_OK; // found it!
                    }
                    free(pBuf);
                }
            }
        }
        i++;
    }
    fclose(pF->f);
    return UNZ_BAD_ZIP_FILE;
} /* unzOpen() */

int unzGetFileCount(UNZ_FILE *pF)
{
    return pF->iTotalFiles;
} /* unzGetFileCount() */

int unzClose(UNZ_FILE *pF)
{
    if (pF == NULL)
        return UNZ_BAD_PARAM;
    if (pF->f != NULL)
        fclose(pF->f);
    if (pF->stream.opaque != Z_NULL)
    { // did they forget to close the file?
      inflateEnd(&pF->stream);
    }
    return UNZ_OK;
} /* unzClose() */

int unzGetFileInfo(UNZ_FILE *pF, int i, UNZ_FILE_INFO *pInfo)
{
    uint32_t u32Sig, u32Offset;
    uint8_t temp[4];

    if (i < 0 || i >= pF->iTotalFiles || pInfo == NULL)
        return UNZ_BAD_PARAM;

    u32Offset = read_uint32((uint8_t *)&pF->eocd.u32Offset);
    if (fseek(pF->f, u32Offset, SEEK_SET) != 0)
        return UNZ_BAD_ZIP_FILE;

    while (i >= 0)
    {
        if (unzlocal_read(pF, temp, 4) != 4) return UNZ_BAD_ZIP_FILE;
        u32Sig = read_uint32(temp);
        if (u32Sig != CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE)
            return UNZ_BAD_ZIP_FILE;
        if (unzlocal_read(pF, &pF->cdfh, sizeof(struct CDFH)) != sizeof(struct CDFH))
            return UNZ_BAD_ZIP_FILE;
        if (i == 0) // this is the one
        {
            pInfo->version = read_uint16((uint8_t *)&pF->cdfh.u16Version);
            pInfo->version_needed = read_uint16((uint8_t *)&pF->cdfh.u16VersionNeeded);
            pInfo->flag = read_uint16((uint8_t *)&pF->cdfh.u16Flags);
            pInfo->compression_method = read_uint16((uint8_t *)&pF->cdfh.u16Method);
            pInfo->crc = read_uint32((uint8_t *)&pF->cdfh.u32CRC);
            pInfo->compressed_size = read_uint32((uint8_t *)&pF->cdfh.u32Compressed);
            pInfo->uncompressed_size = read_uint32((uint8_t *)&pF->cdfh.u32Uncompressed);
            pInfo->size_filename = read_uint16((uint8_t *)&pF->cdfh.u16FilenameLen);
            pInfo->size_file_extra = read_uint16((uint8_t *)&pF->cdfh.u16ExtraLen);
            pInfo->size_file_comment = read_uint16((uint8_t *)&pF->cdfh.u16CommentLen);
            pInfo->disk_num_start = read_uint16((uint8_t *)&pF->cdfh.u16Disk);
            pInfo->internal_fa = read_uint16((uint8_t *)&pF->cdfh.u16Internal);
            pInfo->external_fa = read_uint32((uint8_t *)&pF->cdfh.u32External);
            // read filename, extra and comment
            if (pInfo->size_filename > 0)
            {
               if (unzlocal_read(pF, (uint8_t *)pInfo->filename, pInfo->size_filename) != pInfo->size_filename)
                   return UNZ_BAD_ZIP_FILE;
                pInfo->filename[pInfo->size_filename] = 0;
            }
            if (pInfo->size_file_extra > 0)
            {
                fseek(pF->f, pInfo->size_file_extra, SEEK_CUR);
            }
            if (pInfo->size_file_comment > 0)
            {
               if (unzlocal_read(pF, (uint8_t *)pInfo->comment, pInfo->size_file_comment) != pInfo->size_file_comment)
                   return UNZ_BAD_ZIP_FILE;
                pInfo->comment[pInfo->size_file_comment] = 0;
            }
            return UNZ_OK;
        }
        fseek(pF->f, read_uint16((uint8_t *)&pF->cdfh.u16FilenameLen) + read_uint16((uint8_t *)&pF->cdfh.u16ExtraLen) + read_uint16((uint8_t *)&pF->cdfh.u16CommentLen), SEEK_CUR);
        i--;
    }
    return UNZ_OK;

} /* unzGetFileInfo() */

int unzLocateFile(UNZ_FILE *pF, const char *szFilename)
{
int i;
UNZ_FILE_INFO info;

    if (pF == NULL || szFilename == NULL)
        return UNZ_BAD_PARAM;

    for (i=0; i<pF->iTotalFiles; i++)
    {
        unzGetFileInfo(pF, i, &info);
        if (strcmp(szFilename, info.filename) == 0)
        {
            pF->iCurrentFile = i;
            return UNZ_OK;
        }
    }
    return UNZ_FILE_NOT_FOUND; // not found
} /* unzLocateFile() */

int unzOpenCurrentFile(UNZ_FILE *pF)
{
    struct LFH lfh;
    uint32_t u32Sig;
    uint8_t temp[4];

    if (pF == NULL)
        return UNZ_BAD_PARAM;

    // The central directory file header is already loaded
    pF->u32HeaderOffset = read_uint32((uint8_t *)&pF->cdfh.u32Offset);
    fseek(pF->f, pF->u32HeaderOffset, SEEK_SET);
    // Read the local file header
    if (unzlocal_read(pF, temp, 4) != 4) return UNZ_BAD_ZIP_FILE;
    u32Sig = read_uint32(temp);
    if (u32Sig != LOCAL_FILE_HEADER_SIGNATURE)
        return UNZ_BAD_ZIP_FILE;
    if (unzlocal_read(pF, (uint8_t *)&lfh, sizeof(lfh)) != sizeof(lfh))
        return UNZ_BAD_ZIP_FILE;
    // Skip filename and extra fields
    fseek(pF->f, read_uint16((uint8_t *)&lfh.u16FilenameLen) + read_uint16((uint8_t *)&lfh.u16ExtraLen), SEEK_CUR);
    pF->u32FileOffset = (uint32_t)ftell(pF->f); // position of the compressed data
    if (read_uint16((uint8_t *)&pF->cdfh.u16Method) != 0) // if compressed
    {
        pF->stream.zalloc = Z_NULL;
        pF->stream.zfree = Z_NULL;
        pF->stream.opaque = Z_NULL;
        pF->stream.avail_in = 0;
        pF->stream.next_in = Z_NULL;
        if (inflateInit2(&pF->stream, -MAX_WBITS) != Z_OK)
            return UNZ_BAD_DECOMP_STATE;
    }
    return UNZ_OK;
} /* unzOpenCurrentFile() */

int unzCloseCurrentFile(UNZ_FILE *pF)
{
    if (pF == NULL) return UNZ_BAD_PARAM;
    if (read_uint16((uint8_t *)&pF->cdfh.u16Method) != 0)
        inflateEnd(&pF->stream);
    return UNZ_OK;
} /* unzCloseCurrentFile() */

int unzReadCurrentFile(UNZ_FILE *pF, uint8_t *pBuf, int iLen)
{
    uint8_t temp[1024];

    if (pF == NULL || pBuf == NULL || iLen <= 0) return UNZ_BAD_PARAM;

    if (read_uint16((uint8_t *)&pF->cdfh.u16Method) == 0) // not compressed
    {
        uint32_t u32ToRead = read_uint32((uint8_t *)&pF->cdfh.u32Uncompressed);
        if (iLen > u32ToRead)
            iLen = u32ToRead;
        return unzlocal_read(pF, pBuf, iLen);
    }
    else // compressed, need to decompress
    {
        pF->stream.avail_out = iLen;
        pF->stream.next_out = pBuf;
        while (pF->stream.avail_out > 0)
        {
            int iRead;
            if (pF->stream.avail_in == 0) // need to read more from the file
            {
                uint32_t u32Compressed = read_uint32((uint8_t *)&pF->cdfh.u32Compressed);
                uint32_t u32ToRead = u32Compressed - pF->stream.total_in;
                if (u32ToRead > sizeof(temp))
                    u32ToRead = sizeof(temp);
                iRead = unzlocal_read(pF, temp, u32ToRead);
                if (iRead <= 0) // nothing left to read, but we need more
                    return iLen - pF->stream.avail_out;
                pF->stream.avail_in = iRead;
                pF->stream.next_in = temp;
            }
            int ret = inflate(&pF->stream, Z_NO_FLUSH);
            if (ret == Z_STREAM_END)
                return iLen - pF->stream.avail_out;
            if (ret != Z_OK)
                return UNZ_BAD_DECOMP_STATE;
        }
        return iLen;
    }
} /* unzReadCurrentFile() */

int unzEndOfFile(UNZ_FILE *pF)
{
    return (pF->stream.total_out == read_uint32((uint8_t *)&pF->cdfh.u32Uncompressed));
} /* unzEndOfFile() */

//
// C++ wrapper
//
Unzip::Unzip()
{
    m_pZip = NULL;
}
Unzip::~Unzip()
{
    if (m_pZip)
    {
        unzClose(m_pZip);
        free(m_pZip);
        m_pZip = NULL;
    }
}
int Unzip::open(const char *filepath)
{
    if (m_pZip) // already open
    {
        unzClose(m_pZip);
    }
    else
    {
        m_pZip = (UNZ_FILE *)malloc(sizeof(UNZ_FILE));
    }
    if (m_pZip == NULL) return UNZ_BAD_PARAM;
    return unzOpen(filepath, m_pZip);
}
void Unzip::close()
{
    if (m_pZip)
    {
        unzClose(m_pZip);
        free(m_pZip);
        m_pZip = NULL;
    }
}
int Unzip::getFileCount()
{
    if (m_pZip == NULL) return 0;
    return unzGetFileCount(m_pZip);
}
int Unzip::getFileInfo(int i, UNZ_FILE_INFO *pInfo)
{
    if (m_pZip == NULL) return UNZ_BAD_PARAM;
    return unzGetFileInfo(m_pZip, i, pInfo);
}
int Unzip::locateFile(const char *szFilename)
{
    if (m_pZip == NULL) return UNZ_BAD_PARAM;
    return unzLocateFile(m_pZip, szFilename);
}
int Unzip::openCurrentFile()
{
    if (m_pZip == NULL) return UNZ_BAD_PARAM;
    return unzOpenCurrentFile(m_pZip);
}
void Unzip::closeCurrentFile()
{
    if (m_pZip)
        unzCloseCurrentFile(m_pZip);
}
int Unzip::readCurrentFile(uint8_t *pBuf, int iLen)
{
    if (m_pZip == NULL) return UNZ_BAD_PARAM;
    return unzReadCurrentFile(m_pZip, pBuf, iLen);
}
int Unzip::eof()
{
    if (m_pZip == NULL) return 0;
    return unzEndOfFile(m_pZip);
}