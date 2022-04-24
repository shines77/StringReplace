
#ifndef UTF8_ENCODING_H
#define UTF8_ENCODING_H

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdbool>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <list>
#include <vector>
#include <set>
#include <map>
#include <utility>
#include <algorithm>
#include <type_traits>

namespace utf8 {

/*******************************************************************************

    UTF-8 encoding

    Bits     Unicode 32                     UTF-8
            (hexadecimal)                  (binary)

     7   0000 0000 - 0000 007F   1 byte:   0xxxxxxx
    11   0000 0080 - 0000 07FF   2 bytes:  110xxxxx 10xxxxxx
    16   0000 0800 - 0000 FFFF   3 bytes:  1110xxxx 10xxxxxx 10xxxxxx
    21   0001 0000 - 001F FFFF   4 bytes:  11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    26   0020 0000 - 03FF FFFF   5 bytes:  111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx
    31   0400 0000 - 7FFF FFFF   6 bytes:  1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx

    RFC3629£ºUTF-8, a transformation format of ISO 10646

    https://www.ietf.org/rfc/rfc3629.txt

    if ((str[i] & 0xc0) != 0x80)
        length++;

    Utf-8 encoding Online: https://tool.oschina.net/encode?type=4

*******************************************************************************/

static inline
std::size_t utf8_decode_len(const char * utf8)
{
    std::uint32_t type = (std::uint32_t)((std::uint8_t)*utf8 & 0xF0);
    if (type == 0x000000E0u) {
        return std::size_t(3);
    } else if (type < 0x00000080u) {
        return std::size_t(1);
    } else if (type < 0x000000E0u) {
        return std::size_t(2);
    } else {
        return std::size_t(4);
    }
}

static inline
std::size_t unicode_encode_len(std::uint32_t unicode)
{
    if (unicode >= 0x00000800u) {
        if (unicode <= 0x0000FFFFu)
            return std::size_t(3);
        else
            return std::size_t(4);
    } else if (unicode < 0x00000080u) {
        return std::size_t(1);
    } else {
        return std::size_t(2);
    }
}

static inline
std::uint32_t utf8_decode(const char * utf8, std::size_t & skip)
{
    std::uint32_t unicode;
    const std::uint8_t * text = (const std::uint8_t *)utf8;
    std::uint32_t type = (std::uint32_t)(*text & 0xF0);
    if (type == 0x000000E0u) {
        // 16 bits, 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
        unicode = ((*text & 0x0F) << 12u) | ((*(text + 1) & 0x3F) << 6u) | (*(text + 2) & 0x3F);
        skip = 3;
    } else if (type < 0x00000080u) {
        // 8bits,   1 byte:  0xxxxxxx
        unicode = *text;
        skip = 1;
    } else if (type < 0x000000E0u) {
        // 11 bits, 2 bytes: 110xxxxx 10xxxxxx
        unicode = ((*text & 0x1F) << 6u) | (*(text + 1) & 0x3F);
        skip = 2;
    } else {
        // 21 bits, 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        unicode = ((*text & 0x07) << 18u) | ((*(text + 1) & 0x3F) << 12u) | ((*(text + 2) & 0x3F) << 6u) | (*(text + 3) & 0x3F);
        skip = 4;
    }
    return unicode;
}

static inline
std::size_t utf8_encode(std::uint32_t unicode, char * text)
{
    uint8_t * output = (uint8_t *)text;
    if (unicode >= 0x00000800u) {
        if (unicode <= 0x0000FFFFu) {
            // 0x00000800 - 0x0000FFFF
            // 16 bits, 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
            // Using 0x0000FFFFu instead of 0x00003F00u may be optimized
            *(output + 0) = (uint8_t)(((unicode & 0x0000FFFFu) >> 12u) | 0xE0);
            *(output + 1) = (uint8_t)(((unicode & 0x00000FC0u) >> 6u ) | 0x80);
            *(output + 2) = (uint8_t)(((unicode & 0x0000003Fu) >> 0u ) | 0x80);
            return std::size_t(3);
        } else {
            // 0x00010000 - 0x001FFFFF (in fact 0x0010FFFF)
            // 21 bits, 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            *(output + 0) = (uint8_t)(((unicode & 0x001C0000u) >> 18u) | 0xF0);
            *(output + 1) = (uint8_t)(((unicode & 0x00003F00u) >> 12u) | 0x80);
            *(output + 2) = (uint8_t)(((unicode & 0x00000FC0u) >> 6u ) | 0x80);
            *(output + 3) = (uint8_t)(((unicode & 0x0000003Fu) >> 0u ) | 0x80);
            return std::size_t(4);
        }
    } else if (unicode < 0x00000080u) {
        // 0x00000000 - 0x0000007F
        // 8bits,   1 byte:  0xxxxxxx
        *(output + 0) = (uint8_t)(unicode & 0x0000007Fu);
        return std::size_t(1);
    } else {
        // 0x00000080 - 0x000007FF
        // 11 bits, 2 bytes: 110xxxxx 10xxxxxx
        *(output + 0) = (uint8_t)(((unicode & 0x000007C0u) >> 6u ) | 0xC0);
        *(output + 1) = (uint8_t)(((unicode & 0x0000003Fu) >> 0u ) | 0x80);
        return std::size_t(2);
    }
}

} // namespace utf8

#endif // UTF8_ENCODING_H
