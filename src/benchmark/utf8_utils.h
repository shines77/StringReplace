
#ifndef UTF8_UTILS_H
#define UTF8_UTILS_H

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include <cstdint>
#include <cstddef>
#include <cstdbool>

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

    Utf-8 encoding online: https://tool.oschina.net/encode?type=4

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
std::uint32_t utf8_decode(const char * utf8_input, std::size_t & skip)
{
    std::uint32_t unicode;
    const std::uint8_t * utf8 = (const std::uint8_t *)utf8_input;
    std::uint32_t type = (std::uint32_t)(*utf8 & 0xF0);
    if (type == 0x000000E0u) {
        // 16 bits, 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
        unicode = ((std::uint32_t)(*(utf8 + 0) & 0x0F) << 12u) | ((std::uint32_t)(*(utf8 + 1) & 0x3F) << 6u) |
                   (std::uint32_t)(*(utf8 + 2) & 0x3F);
        skip = 3;
    } else if (type < 0x00000080u) {
        // 8bits,   1 byte:  0xxxxxxx
        unicode = (std::uint32_t)*utf8;
        skip = 1;
    } else if (type < 0x000000E0u) {
        // 11 bits, 2 bytes: 110xxxxx 10xxxxxx
        unicode = ((std::uint32_t)(*(utf8 + 0) & 0x1F) << 6u) | (std::uint32_t)(*(utf8 + 1) & 0x3F);
        skip = 2;
    } else {
        // 21 bits, 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        unicode = ((std::uint32_t)(*(utf8 + 0) & 0x07) << 18u) | ((std::uint32_t)(*(utf8 + 1) & 0x3F) << 12u) |
                  ((std::uint32_t)(*(utf8 + 2) & 0x3F) << 6u ) |  (std::uint32_t)(*(utf8 + 3) & 0x3F);
        skip = 4;
    }
    return unicode;
}

static inline
std::size_t utf8_encode(std::uint32_t unicode, char * utf8_output)
{
    uint8_t * utf8 = (uint8_t *)utf8_output;
    if (unicode >= 0x00000800u) {
        if (unicode <= 0x0000FFFFu) {
            // 0x00000800 - 0x0000FFFF
            // 16 bits, 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
            // Using 0x0000FFFFu instead of 0x00003F00u may be optimized
            *(utf8 + 0) = (uint8_t)(((unicode & 0x0000FFFFu) >> 12u) | 0xE0);
            *(utf8 + 1) = (uint8_t)(((unicode & 0x00000FC0u) >> 6u ) | 0x80);
            *(utf8 + 2) = (uint8_t)(((unicode & 0x0000003Fu) >> 0u ) | 0x80);
            return std::size_t(3);
        } else {
            // 0x00010000 - 0x001FFFFF (in fact 0x0010FFFF)
            // 21 bits, 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            *(utf8 + 0) = (uint8_t)(((unicode & 0x001C0000u) >> 18u) | 0xF0);
            *(utf8 + 1) = (uint8_t)(((unicode & 0x00003F00u) >> 12u) | 0x80);
            *(utf8 + 2) = (uint8_t)(((unicode & 0x00000FC0u) >> 6u ) | 0x80);
            *(utf8 + 3) = (uint8_t)(((unicode & 0x0000003Fu) >> 0u ) | 0x80);
            return std::size_t(4);
        }
    } else if (unicode < 0x00000080u) {
        // 0x00000000 - 0x0000007F
        // 8bits,   1 byte:  0xxxxxxx
        *(utf8 + 0) = (uint8_t)(unicode & 0x0000007Fu);
        return std::size_t(1);
    } else {
        // 0x00000080 - 0x000007FF
        // 11 bits, 2 bytes: 110xxxxx 10xxxxxx
        *(utf8 + 0) = (uint8_t)(((unicode & 0x000007C0u) >> 6u ) | 0xC0);
        *(utf8 + 1) = (uint8_t)(((unicode & 0x0000003Fu) >> 0u ) | 0x80);
        return std::size_t(2);
    }
}

} // namespace utf8

#endif // UTF8_UTILS_H
