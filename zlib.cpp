//
// Simple ZLIB DEFLATE decompressor
// written by Larry Bank
//
// Copyright 2021 BitBank Software, Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"

// internal (un-exported) function
extern int tinfl_decompress_mem_to_mem(void *pOut_buf, int out_buf_len, const void *pSrc_buf, int src_buf_len, int flags);

//
// Given a buffer with compressed GZIP data, decompress it into a new buffer
//
int zlib_unzip(unsigned char *inbuf, int inlen, unsigned char *outbuf, int outlen)
{
    int i, flags;
    
    if (inlen < 10) return Z_ERROR_HEADER; // not enough length for a GZIP header
    // check for valid GZIP header
    if (inbuf[0] != 0x1f || inbuf[1] != 0x8b || inbuf[2] != 8)
        return Z_ERROR_HEADER;
    
    flags = inbuf[3];
    // skip over the variable length parts of the header
    i = 10;
    if (flags & GZIP_FLAG_FEXTRA)
    {
        int l = inbuf[i] | (inbuf[i+1] << 8);
        i += (l+2);
    }
    if (flags & GZIP_FLAG_FNAME)
    {
        while (i < inlen && inbuf[i])
            i++;
        i++; // skip null terminator too
    }
    if (flags & GZIP_FLAG_FCOMMENT)
    {
        while (i < inlen && inbuf[i])
            i++;
        i++; // skip null
    }
    if (flags & GZIP_FLAG_FHCRC)
        i += 2;
    
    if (i >= inlen)
        return Z_ERROR_HEADER;
    
    // send the uncompressed data to the callback function
    return zlib_inflate(&inbuf[i], inlen - i, outbuf, outlen);
    
} /* zlib_unzip() */

//
// tinfl.c
//
typedef unsigned char mz_uint8;
typedef signed short mz_int16;
typedef unsigned short mz_uint16;
typedef unsigned int mz_uint32;
typedef unsigned int mz_uint;
typedef long long mz_int64;
typedef unsigned long long mz_uint64;
typedef int mz_bool;

void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags);
size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);
// ------------------- Low-level Decompression API Definitions

typedef struct
{
    mz_uint8 *m_pIn_buf, *m_pOut_buf, *m_pIn_buf_start, *m_pOut_buf_start;
    int m_in_buf_size, m_out_buf_size;
    mz_uint8 *m_pIn_buf_cur, *m_pOut_buf_cur;
    mz_uint8 *m_pIn_buf_stop, *m_pOut_buf_stop;
    mz_uint32 m_adler32, m_flags;
    int m_num_bits, m_zhdr0, m_zhdr1, m_z_adler32, m_final, m_type, m_dist, m_counter, m_num_extra, m_table_sizes[1];
    mz_uint32 m_bit_buf;
    size_t m_huff_count[1][257], m_huff_codes[1][257], m_huff_code_sizes[1][257];
    size_t m_len_codes[512], m_dist_codes[512], m_num_decompress_remaining;
} tinfl_decompressor;

// Return status.
typedef enum
{
    TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS = -4,
    TINFL_STATUS_BAD_PARAM = -3,
    TINFL_STATUS_ADLER32_MISMATCH = -2,
    TINFL_STATUS_FAILED = -1,
    TINFL_STATUS_DONE = 0,
    TINFL_STATUS_NEEDS_MORE_INPUT = 1,
    TINFL_STATUS_HAS_MORE_OUTPUT = 2
} tinfl_status;

// alternate parsing flags
#define TINFL_FLAG_PARSE_ZLIB_HEADER 1
#define TINFL_FLAG_HAS_MORE_INPUT 2
#define TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF 4
#define TINFL_FLAG_COMPUTE_ADLER32 8

// Internal/private bits follow.
#define TINFL_MAX_HUFF_TABLES 3
#define TINFL_MAX_HUFF_SYMBOLS_0 288
#define TINFL_MAX_HUFF_SYMBOLS_1 32
#define TINFL_MAX_HUFF_SYMBOLS_2 19
#define TINFL_FAST_LOOKUP_BITS 10
#define TINFL_FAST_LOOKUP_SIZE (1 << TINFL_FAST_LOOKUP_BITS)
//
// internal inflate function
//
int tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const mz_uint32 decomp_flags)
{
#define TINFL_GET_BYTE(state_index, c)                                                                                                                           \
    do                                                                                                                                                           \
    {                                                                                                                                                            \
        if (pIn_buf_cur >= pIn_buf_stop)                                                                                                                         \
        {                                                                                                                                                        \
            r->m_state = state_index;                                                                                                                            \
            return TINFL_STATUS_NEEDS_MORE_INPUT;                                                                                                                \
        }                                                                                                                                                        \
        c = *pIn_buf_cur++;                                                                                                                                      \
    } while (0)

#define TINFL_NEED_BITS(state_index, n)                \
    do                                                 \
    {                                                  \
        if (num_bits < (n))                            \
        {                                              \
            TINFL_GET_BYTE(state_index, c);            \
            bit_buf |= (((mz_uint32)c) << num_bits);    \
            num_bits += 8;                             \
        }                                              \
    } while (0)

#define TINFL_PEEK_BITS(state_index, n) \
    do { TINFL_NEED_BITS(state_index, n); } while(0)

#define TINFL_SKIP_BITS(state_index, n)             \
    do                                              \
    {                                               \
        if (num_bits < (n))                         \
        {                                           \
            TINFL_NEED_BITS(state_index, n);        \
        }                                           \
        bit_buf >>= (n);                            \
        num_bits -= (n);                            \
    } while (0)

#define TINFL_PUT_BYTE(c)                                                                                                                                  \
    do                                                                                                                                                     \
    {                                                                                                                                                      \
        *pOut_buf_cur++ = (mz_uint8)(c);                                                                                                                   \
    } while (0)

#define TINFL_DECOMPRESS_STATE_COMMON                                                                                                                        \
    size_t in_buf_size = *pIn_buf_size, out_buf_size = *pOut_buf_size;                                                                                        \
    const mz_uint8 *pIn_buf_cur = pIn_buf_next, *const pIn_buf_stop = pIn_buf_next + in_buf_size;                                                             \
    mz_uint8 *pOut_buf_cur = pOut_buf_next, *const pOut_buf_stop = pOut_buf_next + out_buf_size;                                                              \
    mz_uint32 bit_buf = r->m_bit_buf;                                                                                                                        \
    int num_bits = r->m_num_bits, dist, counter;                                                                                                             \
    int num_extra, t;                                                                                                                                        \
    mz_uint32 crc;                                                                                                                                           \
    mz_uint c;

#define TINFL_STORE_STATE_AND_RETURN(status)                                                                                                                 \
    do                                                                                                                                                       \
    {                                                                                                                                                        \
        *pIn_buf_size = pIn_buf_cur - pIn_buf_next;                                                                                                          \
        *pOut_buf_size = pOut_buf_cur - pOut_buf_next;                                                                                                       \
        r->m_num_bits = num_bits;                                                                                                                            \
        r->m_bit_buf = bit_buf;                                                                                                                              \
        return status;                                                                                                                                       \
    } while (0)


    static const mz_uint16 s_length_base[31] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
    static const mz_uint8 s_length_extra[31] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0};
    static const mz_uint16 s_dist_base[32] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0, 0};
    static const mz_uint8 s_dist_extra[32] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 0, 0};
    static const mz_uint8 s_length_dezigzag[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    static const int s_min_table_sizes[3] = {257, 1, 4};
    
    TINFL_DECOMPRESS_STATE_COMMON
    
    // The following macro determines if we can safely loop and copy data directly from the input buffer to the output buffer,
    // which is faster than calling TINFL_PUT_BYTE() for each byte.
#define TINFL_CAN_FAST_COPY(num_bytes) ((num_bytes <= 64) && (pOut_buf_cur_end >= pOut_buf_cur + (num_bytes)) && (pIn_buf_cur_end >= pIn_buf_cur + (num_bytes)))
    
#if TINFL_USE_64BIT_BITBUF
#define TINFL_HUFF_DECODE(state_index, pHuff)                                                                                                                                \
    do                                                                                                                                                                      \
    {                                                                                                                                                                       \
        int l;                                                                                                                                                              \
        TINFL_PEEK_BITS(state_index, 15);                                                                                                                                   \
        if ((l = (pHuff)[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) < 0)                                                                                                       \
            l = tinfl_huff_get_symbol((r->m_table_sizes[0] + r->m_table_sizes[1]), (pHuff), &bit_buf); \
        TINFL_SKIP_BITS(state_index, l & 15);                                                                                                                               \
        t = l >> 4;                                                                                                                                                         \
    } while (0)
#else
#define TINFL_HUFF_DECODE(state_index, pHuff)                                                                                                                                \
    do                                                                                                                                                                      \
    {                                                                                                                                                                       \
        int l;                                                                                                                                                              \
        TINFL_PEEK_BITS(state_index, 15);                                                                                                                                   \
        if ((l = (pHuff)[bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) < 0)                                                                                                       \
            l = tinfl_huff_get_symbol((r->m_table_sizes[0] + r->m_table_sizes[1]), (pHuff), &bit_buf);                                                                        \
        TINFL_SKIP_BITS(state_index, l & 15);                                                                                                                               \
        t = l >> 4;                                                                                                                                                         \
    } while (0)
#endif
    
#define TINFL_MEMCPY(d, s, l) memcpy(d, s, l)
#define TINFL_MEMSET(d, v, l) memset(d, v, l)

#if TINFL_USE_64BIT_BITBUF
    static mz_uint32 tinfl_bit_reverse32(mz_uint32 v) { v = ((v & 0x55555555) << 1) | ((v >> 1) & 0x55555555); v = ((v & 0x33333333) << 2) | ((v >> 2) & 0x33333333); v = ((v & 0x0F0F0F0F) << 4) | ((v >> 4) & 0x0F0F0F0F); return (v << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | (v >> 24); }
#else
    static mz_uint32 tinfl_bit_reverse32(mz_uint32 v) { v = ((v & 0x55555555) << 1) | ((v >> 1) & 0x55555555); v = ((v & 0x33333333) << 2) | ((v >> 2) & 0x33333333); v = ((v & 0x0F0F0F0F) << 4) | ((v >> 4) & 0x0F0F0F0F); return (v << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | (v >> 24); }
#endif
    
    for (;;)
    {
        switch (r->m_state)
        {
            case 0:
                if ((decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER) == 0)
                {
                    r->m_zhdr0 = r->m_zhdr1 = 0;
                    r->m_state = 2;
                    break;
                }
                TINFL_GET_BYTE(1, r->m_zhdr0);
                TINFL_GET_BYTE(2, r->m_zhdr1);
                c = (r->m_zhdr0 * 256 + r->m_zhdr1);
                if ((r->m_zhdr0 & 15) != 8 || (r->m_zhdr0 >> 4) > 7 || (c % 31 != 0))
                {
                    TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_FAILED);
                }
            case 2:
                TINFL_NEED_BITS(3, 3);
                r->m_final = (bit_buf & 1);
                r->m_type = (bit_buf >> 1) & 3;
                TINFL_SKIP_BITS(3, 3);
                if (r->m_type == 0)
                {
                    TINFL_SKIP_BITS(4, num_bits & 7);
                    r->m_state = 4;
                    break;
                }
                else if (r->m_type == 3)
                {
                    TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_FAILED);
                }
                else if (r->m_type == 1)
                {
                    TINFL_MEMSET(r->m_huff_code_sizes[0], 0, sizeof(r->m_huff_code_sizes[0]));
                    TINFL_MEMSET(r->m_huff_code_sizes[1], 0, sizeof(r->m_huff_code_sizes[1]));
                    TINFL_MEMSET(r->m_huff_codes[0], 0, sizeof(r->m_huff_codes[0]));
                    TINFL_MEMSET(r->m_huff_codes[1], 0, sizeof(r->m_huff_codes[1]));
                    for (t = 0; t < 288; t++)
                        r->m_huff_code_sizes[0][t] = (t <= 143) ? 8 : 9;
                    for (t = 0; t < 32; t++)
                        r->m_huff_code_sizes[1][t] = 5;
                    r->m_table_sizes[0] = 288;
                    r->m_table_sizes[1] = 32;
                    r->m_state = 12;
                    break;
                }
                r->m_state = 6;
                break;
            case 3:
                TINFL_NEED_BITS(4, 32);
                r->m_z_adler32 = tinfl_bit_reverse32(bit_buf);
                TINFL_SKIP_BITS(4, 32);
                r->m_state = 4;
            case 4:
                TINFL_NEED_BITS(5, 16);
                counter = (bit_buf & 0xFFFF);
                TINFL_SKIP_BITS(5, 16);
                r->m_state = 5;
            case 5:
                TINFL_NEED_BITS(6, 16);
                if ((bit_buf & 0xFFFF) != (counter ^ 0xFFFF))
                {
                    TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_FAILED);
                }
                TINFL_SKIP_BITS(6, 16);
                r->m_num_decompress_remaining = r->m_counter = counter;
                r->m_state = 23;
                break;
            case 6:
                TINFL_NEED_BITS(7, 5);
                r->m_table_sizes[0] = 257 + (bit_buf & 31);
                TINFL_SKIP_BITS(7, 5);
                r->m_state = 7;
            case 7:
                TINFL_NEED_BITS(8, 5);
                r->m_table_sizes[1] = 1 + (bit_buf & 31);
                TINFL_SKIP_BITS(8, 5);
                r->m_state = 8;
            case 8:
                TINFL_NEED_BITS(9, 4);
                r->m_table_sizes[2] = 4 + (bit_buf & 15);
                TINFL_SKIP_BITS(9, 4);
                counter = 0;
                TINFL_MEMSET(r->m_huff_code_sizes[2], 0, sizeof(r->m_huff_code_sizes[2]));
                r->m_state = 9;
            case 9:
                if (counter < r->m_table_sizes[2])
                {
                    TINFL_NEED_BITS(10, 3);
                    r->m_huff_code_sizes[2][s_length_dezigzag[counter++]] = (bit_buf & 7);
                    TINFL_SKIP_BITS(10, 3);
                    r->m_state = 9;
                    break;
                }
                r->m_state = 10;
            case 10:
                for (t = 0; t < 3; t++)
                    r->m_table_sizes[t] = s_min_table_sizes[t];
                
                TINFL_MEMSET(r->m_huff_codes[0], 0, sizeof(r->m_huff_codes[0]));
                TINFL_MEMSET(r->m_huff_codes[1], 0, sizeof(r->m_huff_codes[1]));
                TINFL_MEMSET(r->m_huff_count[0], 0, sizeof(r->m_huff_count[0]));
                TINFL_MEMSET(r->m_len_codes, 0, sizeof(r->m_len_codes));
                TINFL_MEMSET(r->m_dist_codes, 0, sizeof(r->m_dist_codes));
                
                counter = 0;
                r->m_state = 11;
            case 11:
                if (counter < r->m_table_sizes[0] + r->m_table_sizes[1])
                {
                    TINFL_HUFF_DECODE(11, r->m_dist_codes);
                    if (t < 16)
                    {
                        r->m_huff_code_sizes[0][counter++] = t;
                        break;
                    }
                    if (t == 16)
                        num_extra = 2;
                    else if (t == 17)
                        num_extra = 3;
                    else
                        num_extra = 7;
                    
                    if (counter + num_extra > r->m_table_sizes[0] + r->m_table_sizes[1])
                    {
                        TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_FAILED);
                    }
                    TINFL_NEED_BITS(11, num_extra);
                    t = (bit_buf & ((1 << num_extra) - 1));
                    TINFL_SKIP_BITS(11, num_extra);
                    
                    TINFL_MEMSET(r->m_huff_code_sizes[0] + counter, (t == 0) ? 0 : r->m_huff_code_sizes[0][counter - 1], num_extra);
                    counter += num_extra;
                    break;
                }
                r->m_state = 12;
            case 12:
                if (tinfl_build_huffman_table(r, 0) != 0 || tinfl_build_huffman_table(r, 1) != 0)
                {
                    TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_FAILED);
                }
                r->m_counter = 0;
                r->m_state = 23;
                break;
            case 23:
                if (pOut_buf_cur >= pOut_buf_stop)
                {
                    TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_HAS_MORE_OUTPUT);
                }
                if (r->m_num_decompress_remaining == 0)
                {
                    TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_DONE);
                }
                counter = r->m_counter;
                if (counter > 0)
                {
                    TINFL_MEMCPY(pOut_buf_cur, pOut_buf_start - dist, counter);
                    pOut_buf_cur += counter;
                    r->m_counter = 0;
                }
                
                for (;;)
                {
                    TINFL_HUFF_DECODE(23, r->m_len_codes);
                    if (t < 256)
                    {
                        TINFL_PUT_BYTE(t);
                    }
                    else if (t > 256)
                    {
                        counter = s_length_base[t - 257];
                        num_extra = s_length_extra[t - 257];
                        if (num_extra > 0)
                        {
                            TINFL_NEED_BITS(23, num_extra);
                            counter += (bit_buf & ((1 << num_extra) - 1));
                            TINFL_SKIP_BITS(23, num_extra);
                        }
                        
                        TINFL_HUFF_DECODE(23, r->m_dist_codes);
                        dist = s_dist_base[t];
                        num_extra = s_dist_extra[t];
                        if (num_extra > 0)
                        {
                            TINFL_NEED_BITS(23, num_extra);
                            dist += (bit_buf & ((1 << num_extra) - 1));
                            TINFL_SKIP_BITS(23, num_extra);
                        }
                        
                        // Copy bytes from history
                        const mz_uint8 *pSrc = pOut_buf_cur - dist;
                        if (pSrc < pOut_buf_start)
                        {
                            TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_FAILED);
                        }
                        
                        if ((r->m_num_decompress_remaining -= counter) < 0)
                        {
                            TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_FAILED);
                        }
                        
                        if (TINFL_CAN_FAST_COPY(counter))
                        {
                            TINFL_MEMCPY(pOut_buf_cur, pSrc, counter);
                            pOut_buf_cur += counter;
                        }
                        else
                        {
                            while (counter-- > 0)
                            {
                                *pOut_buf_cur++ = *pSrc++;
                            }
                        }
                    }
                    else // t == 256 (end of block)
                    {
                        if (r->m_final)
                        {
                            if (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER)
                            {
                                r->m_state = 24;
                                break;
                            }
                            TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_DONE);
                        }
                        r->m_state = 2;
                        break;
                    }
                }
                break;
            case 24:
                TINFL_NEED_BITS(25, 32);
                crc = tinfl_bit_reverse32(bit_buf);
                TINFL_SKIP_BITS(25, 32);
                if (crc != r->m_z_adler32)
                {
                    TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_ADLER32_MISMATCH);
                }
                r->m_state = 25;
            case 25:
                TINFL_STORE_STATE_AND_RETURN(TINFL_STATUS_DONE);
        }
    }
}

// Higher-level helper functions.
void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags)
{
    tinfl_decompressor decomp;
    void *pBuf;
    *pOut_len = 0;
    if ((pBuf = malloc(src_buf_len * 4)) == NULL) return NULL;
    tinfl_init(&decomp);
    if (tinfl_decompress(&decomp, (const mz_uint8 *)pSrc_buf, &src_buf_len, (mz_uint8 *)pBuf, (mz_uint8*)pBuf, pOut_len, (flags & ~TINFL_FLAG_HAS_MORE_INPUT)) != TINFL_STATUS_DONE)
    {
        free(pBuf);
        return NULL;
    }
    return pBuf;
}
size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags)
{
    tinfl_decompressor decomp;
    size_t out_len = out_buf_len;
    tinfl_init(&decomp);
    if (tinfl_decompress(&decomp, (const mz_uint8 *)pSrc_buf, &src_buf_len, (mz_uint8 *)pOut_buf, (mz_uint8*)pOut_buf, &out_len, (flags & ~TINFL_FLAG_HAS_MORE_INPUT) | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) != TINFL_STATUS_DONE)
        return 0;
    return out_len;
}

int tinfl_build_huffman_table(tinfl_decompressor *r, int table_index)
{
    int i, l, n, num_syms = r->m_table_sizes[table_index];
    mz_uint32 code, next_code[17], sizes[18];
    TINFL_MEMSET(sizes, 0, sizeof(sizes));
    for (i = 0; i < num_syms; i++)
        sizes[r->m_huff_code_sizes[table_index][i]]++;
    next_code[0] = code = 0;
    for (l = 1; l <= 16; l++)
    {
        next_code[l] = code = (code + sizes[l-1]) << 1;
    }
    for (i = 0; i < num_syms; i++)
    {
        if ((l = r->m_huff_code_sizes[table_index][i]) != 0)
        {
            r->m_huff_codes[table_index][i] = next_code[l]++;
        }
    }
    
    // Create a fast lookup table
    TINFL_MEMSET(r->m_len_codes, 0, sizeof(r->m_len_codes));
    for (i = 0; i < num_syms; i++)
    {
        if ((l = r->m_huff_code_sizes[table_index][i]) != 0)
        {
            code = tinfl_bit_reverse32(r->m_huff_codes[table_index][i]) >> (32 - l);
            for (n = 0; n < (1 << (TINFL_FAST_LOOKUP_BITS - l)); n++)
            {
                r->m_len_codes[code | (n << l)] = (i << 4) | l;
            }
        }
    }
    return 0;
}

int tinfl_huff_get_symbol(int num_syms, const size_t* pHuff_codes, mz_uint32* pBit_buf)
{
    int i, l;
    mz_uint32 code;
    for (l = 1; l <= 15; l++)
    {
        code = tinfl_bit_reverse32(*pBit_buf) >> (32 - l);
        for (i = 0; i < num_syms; i++)
            if (pHuff_codes[i] == code) return (i << 4) | l;
    }
    return -1;
}

void tinfl_init(tinfl_decompressor *r)
{
    r->m_state = 0;
}
int zlib_inflate(unsigned char *inbuf, int inlen, unsigned char *outbuf, int outlen)
{
    return (int)tinfl_decompress_mem_to_mem(outbuf, outlen, inbuf, inlen, TINFL_FLAG_PARSE_ZLIB_HEADER);
}
int zlib_get_version()
{
    return 120; // v1.2.0 of this code
} 