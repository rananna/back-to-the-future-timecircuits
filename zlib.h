#ifndef __ZLIB_H__
#define __ZLIB_H__

// ZLIB simple decompressor
// written by Larry Bank
// bitbank@pobox.com
//
// A simple decompressor for GZIP files and raw DEFLATE'd data
// It has a very small memory footprint and is designed for embedded systems
//
#ifdef __cplusplus
extern "C" {
#endif

/*
 * The zlib data format is specified in RFC 1950.
 * The DEFLATE compressed data format is specified in RFC 1951.
 * The GZIP file format is specified in RFC 1952.
 *
 */
// Return codes
enum {
    Z_OK = 0,
    Z_DONE,
    Z_ERROR_FILE,
    Z_ERROR_HEADER,
    Z_ERROR_UNSUPPORTED,
    Z_ERROR_CRC,
    Z_ERROR_MEM,
    Z_ERROR_BTYPE,
    Z_ERROR_HUFFMAN,
    Z_ERROR_INFLATE
};

typedef struct z_file
{
    unsigned char *pData; // pointer to data in memory
    int iDataSize; // size of data
    int iDataIndex; // current byte index
} ZFILE;

// GZIP header flags
#define GZIP_FLAG_FTEXT (1<<0)
#define GZIP_FLAG_FHCRC (1<<1)
#define GZIP_FLAG_FEXTRA (1<<2)
#define GZIP_FLAG_FNAME (1<<3)
#define GZIP_FLAG_FCOMMENT (1<<4)

int zlib_get_version(void);

// For raw DEFLATE'd data
int zlib_inflate(unsigned char *inbuf, int inlen, unsigned char *outbuf, int outlen);
// For GZIP files
int zlib_unzip(unsigned char *inbuf, int inlen, unsigned char *outbuf, int outlen);

#ifdef __cplusplus
}
#endif

#endif // __ZLIB_H__