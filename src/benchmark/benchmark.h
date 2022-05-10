
#ifndef BENCHMARK_H
#define BENCHMARK_H

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
#include <utility>
#include <algorithm>
#include <type_traits>

#include "StringMatch.h"
#include "support/StopWatch.h"

#ifndef __LITTLE_ENDIAN
#define __LITTLE_ENDIAN     0
#endif

#ifndef __BIG_ENDIAN
#define __BIG_ENDIAN        1
#endif

#ifndef __ENDIAN__
#define __ENDIAN__  __LITTLE_ENDIAN
#endif

#if (__ENDIAN__ == __LITTLE_ENDIAN)

#define MAKE_UINT16(A, B) \
    ((uint16_t)(((B) & 0xFF) << 8u) | (uint16_t)((A) & 0xFF))

#define MAKE_UINT32(A, B, C, D) \
    (((uint32_t)MAKE_UINT16(C, D) << 16u) | (uint32_t)MAKE_UINT16(A, B))

#define MAKE_UINT64(A, B, C, D, E, F, G, H) \
    (((uint64_t)MAKE_UINT32(E, F, G, H) << 32u) | (uint64_t)MAKE_UINT32(A, B, C, D))

#else

#define MAKE_UINT16(A, B) \
    ((uint16_t)(((A) & 0xFF) << 8u) | (uint16_t)((B) & 0xFF))

#define MAKE_UINT32(A, B, C, D) \
    (((uint32_t)MAKE_UINT16(A, B) << 16u) | (uint32_t)MAKE_UINT16(C, D))

#define MAKE_UINT64(A, B, C, D, E, F, G, H) \
    (((uint64_t)MAKE_UINT32(A, B, C, D) << 32u) | (uint64_t)MAKE_UINT32(E, F, G, H))

#endif

static const char * gValueText[4] = {
#if defined(_MSC_VER)
    u8"-*电影*-",
    u8"-*音乐*-",
    u8"-*音乐&电影*-",
#elif defined(__clang__)
    "\x2D\x2A\xE7\x94\xB5\xE5\xBD\xB1\x2A\x2D",
    "\x2D\x2A\xE9\x9F\xB3\xE4\xB9\x90\x2A\x2D",
    "\x2D\x2A\xE9\x9F\xB3\xE4\xB9\x90\x26\xE7\x94\xB5\xE5\xBD\xB1\x2A\x2D",
#else
    "-*电影*-",
    "-*音乐*-",
    "-*音乐&电影*-",
#endif
    "Unknown"
};

static const std::size_t gValueLength[4] = {
    std::strlen(gValueText[0]),
    std::strlen(gValueText[1]),
    std::strlen(gValueText[2]),
    std::strlen(gValueText[3])
};

struct ValueType {
    enum {
        Movie,
        Music,
        MovieAndMusic,
        Unknown,
        kMaxType = Unknown
    };

    ValueType() {
#if 0
        for (int valueType = 0; valueType <= kMaxType; valueType++) {
            ValueLength[valueType] = std::strlen(gValueText[valueType]);
        }
#endif
    }

    static const char * toString(int type) {
        assert(type >= 0 && type <= kMaxType);
        return gValueText[type];
    };

    static std::size_t length(int type) {
        assert(type >= 0 && type <= kMaxType);
        return gValueLength[type];
    }

    static int parseValueType(const std::string & dict_kv, std::size_t first, std::size_t last) {
        assert(last > first);
        const char * value_start = &dict_kv[0] + first;
        const char * value_end = &dict_kv[0] + last;
        for (int valueType = 0; valueType < kMaxType; valueType++) {
            const char * value = value_start;
            const char * valueText = gValueText[valueType];
            bool matched = true;
            while (value < value_end) {
                if (*value++ != *valueText++) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                return valueType;
            }
        }
        return ValueType::Unknown;
    }

    template <typename T>
    static inline
    void writeType(T * output, int valueType) {
#if defined(WIN64) || defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) \
 || defined(_M_IA64) || defined(__amd64__) || defined(__x86_64__) \
 || defined(__aarch64__) || defined(_M_ARM64)
        uint16_t * output16;
        uint64_t * output64 = (uint64_t *)output;
        if (valueType == ValueType::Movie) {
            // (10 Bytes) 2D 2A E7 94 B5 E5 BD B1 2A 2D
            *output64 = MAKE_UINT64(0x2D, 0x2A, 0xE7, 0x94, 0xB5, 0xE5, 0xBD, 0xB1);
            output16 = (uint16_t *)(output + 8);
            *output16 = MAKE_UINT16(0x2A, 0x2D);
        } else if (valueType == ValueType::Music) {
            // (10 Bytes) 2D 2A E9 9F B3 E4 B9 90 2A 2D
            *output64 = MAKE_UINT64(0x2D, 0x2A, 0xE9, 0x9F, 0xB3, 0xE4, 0xB9, 0x90);
            output16 = (uint16_t *)(output + 8);
            *output16 = MAKE_UINT16(0x2A, 0x2D);
        } else { // ValueType::MovieAndMusic
            // (17 Bytes) 2D 2A E9 9F B3 E4 B9 90 26 E7 94 B5 E5 BD B1 2A 2D
            *(output64 + 0) = MAKE_UINT64(0x2D, 0x2A, 0xE9, 0x9F, 0xB3, 0xE4, 0xB9, 0x90);
            *(output64 + 1) = MAKE_UINT64(0x26, 0xE7, 0x94, 0xB5, 0xE5, 0xBD, 0xB1, 0x2A);
            *(output + 16) = 0x2D;
        }
#else // !__amd64__
        uint16_t * output16;
        uint32_t * output32 = (uint32_t *)output;
        if (valueType == ValueType::Movie) {
            // (10 Bytes) 2D 2A E7 94 B5 E5 BD B1 2A 2D
            *(output32 + 0) = MAKE_UINT32(0x2D, 0x2A, 0xE7, 0x94);
            *(output32 + 1) = MAKE_UINT32(0xB5, 0xE5, 0xBD, 0xB1);
            output16 = (uint16_t *)(output + 8);
            *output16 = MAKE_UINT16(0x2A, 0x2D);
        } else if (valueType == ValueType::Music) {
            // (10 Bytes) 2D 2A E9 9F B3 E4 B9 90 2A 2D
            *(output32 + 0) = MAKE_UINT32(0x2D, 0x2A, 0xE9, 0x9F);
            *(output32 + 1) = MAKE_UINT32(0xB3, 0xE4, 0xB9, 0x90);
            output16 = (uint16_t *)(output + 8);
            *output16 = MAKE_UINT16(0x2A, 0x2D);
        } else { // ValueType::MovieAndMusic
            // (17 Bytes) 2D 2A E9 9F B3 E4 B9 90 26 E7 94 B5 E5 BD B1 2A 2D
            *(output32 + 0) = MAKE_UINT32(0x2D, 0x2A, 0xE9, 0x9F);
            *(output32 + 1) = MAKE_UINT32(0xB3, 0xE4, 0xB9, 0x90);
            *(output32 + 2) = MAKE_UINT32(0x26, 0xE7, 0x94, 0xB5);
            *(output32 + 3) = MAKE_UINT32(0xE5, 0xBD, 0xB1, 0x2A);
            *(output + 16) = 0x2D;
        }
#endif // __amd64__
    }
};

namespace detail {

template <typename T>
struct no_signed_char_trait {
    typedef T type;
};

template <>
struct no_signed_char_trait<unsigned char> {
    typedef char type;
};

template <>
struct no_signed_char_trait<signed char> {
    typedef char type;
};

template <>
struct no_signed_char_trait<unsigned short> {
    typedef short type;
};

template <>
struct no_signed_char_trait<signed short> {
    typedef short type;
};

template <>
struct no_signed_char_trait<wchar_t> {
    typedef wchar_t type;
};

template <>
struct no_signed_char_trait<char16_t> {
    typedef char16_t type;
};

template <>
struct no_signed_char_trait<char32_t> {
    typedef char32_t type;
};

template <>
struct no_signed_char_trait<unsigned int> {
    typedef int type;
};

template <>
struct no_signed_char_trait<signed int> {
    typedef int type;
};

template <>
struct no_signed_char_trait<unsigned long> {
    typedef long type;
};

template <>
struct no_signed_char_trait<signed long> {
    typedef long type;
};

template <>
struct no_signed_char_trait<unsigned long long> {
    typedef long long type;
};

template <>
struct no_signed_char_trait<signed long long> {
    typedef long long type;
};

template <typename T>
struct char_trait {
	static_assert(
		((std::is_integral<T>::value || std::is_enum<T>::value) &&
			!std::is_same<T, bool>::value),
        "detail::char_trait<T> require that T shall be a (possibly "
		"cv-qualified) integral type or enumeration but not a bool type.");

    typedef typename std::remove_reference<
                typename std::remove_pointer<
                    typename std::remove_extent<
                        typename std::remove_cv<T>::type
                    >::type
                >::type
            >::type CharT;

    typedef typename std::make_unsigned<CharT>::type    Unsigned;
    typedef typename std::make_signed<CharT>::type      Signed;
    typedef typename no_signed_char_trait<CharT>::type  NoSigned;
};

} // namespace detail

namespace StrUtils {

///////////////////////////////////////////////////////////////////////////////
//
// Find
//
///////////////////////////////////////////////////////////////////////////////

template <typename T>
static inline
T * find(const T * first, const T * last, T token)
{
    while (first < last) {
        if (*first != token)
            ++first;
        else
            return (T *)first;
    }
    return nullptr;
}

template <typename T>
static inline
std::size_t find(const T * first, const T * last, T token, std::size_t offset)
{
    const T * cur = first + offset;
    while (cur < last) {
        if (*cur != token)
            ++cur;
        else
            return std::size_t(cur - first);
    }
    return std::string::npos;
}

static inline
char * findp(const std::string & text, char token,
             std::size_t offset = 0,
             std::size_t limit = (std::size_t)-1)
{
    assert(offset < text.size());
    assert(limit <= text.size());
    const char * first = text.c_str() + offset;
    const char * last = text.c_str() + ((limit == (std::size_t)-1) ? text.size() : limit);
    return find(first, last, token);
}

static inline
std::size_t find(const std::string & text, char token,
                 std::size_t offset = 0,
                 std::size_t limit = (std::size_t)-1)
{
    assert(offset < text.size());
    assert(limit <= text.size());
    const char * first = text.c_str();
    const char * last = text.c_str() + ((limit == (std::size_t)-1) ? text.size() : limit);
    return find(first, last, token, offset);
}

///////////////////////////////////////////////////////////////////////////////
//
// Reverse Find
//
///////////////////////////////////////////////////////////////////////////////

template <typename T>
static inline
T * rfind(const T * first, const T * last, T token)
{
    --last;
    while (last >= first) {
        if (*last != token)
            --last;
        else
            return (T *)last;
    }
    return nullptr;
}

template <typename T>
static inline
std::size_t rfind(const T * first, const T * last, T token, std::size_t offset)
{
    const T * cur = last - offset;
    --cur;
    while (cur >= first) {
        if (*cur != token)
            --cur;
        else
            return std::size_t(cur - first);
    }
    return std::string::npos;
}

static inline
char * rfindp(const std::string & text, char token,
              std::size_t offset = 0,
              std::size_t limit = 0)
{
    assert(offset <= text.size());
    assert(limit <= text.size());
    const char * first = text.c_str() + limit;
    const char * last = text.c_str() + offset;
    return rfind(first, last, token);
}

static inline
std::size_t rfind(const std::string & text, char token,
                  std::size_t offset = 0,
                  std::size_t limit = 0)
{
    assert(offset <= text.size());
    assert(limit <= text.size());
    const char * first = text.c_str() + limit;
    const char * last = text.c_str() + offset;

    return rfind(first, last, token, 0);
}

}; // StrUtils

void utf8_to_ansi(const std::string & utf_8, std::string & ansi)
{
#ifdef _MSC_VER
    std::size_t ansi_capacity = utf_8.size() + 1;
    ansi.resize(ansi_capacity);
    int ansi_len = Utf8ToAnsi(utf_8.c_str(), &ansi[0], (int)ansi_capacity);
    if (ansi_len != -1) {
        assert(ansi_len <= ansi_capacity);
        ansi.resize(ansi_len - 1);
    }
#else
    ansi = utf_8;
#endif
}

//
// splicing(AAAA.BBB.txt, CCCC) ==> AAAA.BBB_CCCC.txt
//
std::string splicing_file_name(const std::string & filename, const std::string & name)
{
    std::string ret_file;
    std::size_t dot_pos = filename.find_last_of('.', std::string::npos);
    if (dot_pos != std::string::npos) {
        std::size_t i;
        // Copy AAAA.BBB
        for (i = 0; i < dot_pos; i++)
            ret_file.push_back(filename[i]);
        // Copy _CCCC
        ret_file.push_back('_');
        for (i = 0; i < name.size(); i++)
            ret_file.push_back(name[i]);
        // Copy .txt
        for (i = dot_pos; i < filename.size(); i++)
            ret_file.push_back(filename[i]);
    } else {
        ret_file = filename + name;
    }
    return ret_file;
}

std::size_t read_dict_file(const std::string & dict_file, std::string & content)
{
    static const size_t kReadBufSize = 8 * 1024;

    content.clear();

    std::ifstream ifs;
    ifs.open(dict_file, std::ios::in | std::ios::binary);
    if (ifs.good()) {
        ifs.seekg(0, std::ios::end);
        std::size_t totalSize = (std::size_t)ifs.tellg();
        content.reserve(totalSize);
        ifs.seekg(0, std::ios::beg);

        while (!ifs.eof()) {
            char rdBuf[kReadBufSize];
            ifs.read(rdBuf, kReadBufSize);
            std::streamsize readBytes = ifs.gcount();
            if (readBytes > 0)
                content.append(rdBuf, readBytes);
            else
                break;
        }

        ifs.close();
    }

    return content.size();
}

inline
std::size_t find_kv_separator(const std::string & dict_kv,
                              std::size_t start_pos, std::size_t end_pos,
                              char separator)
{
    const char * kv = (const char *)&dict_kv[start_pos];
    for (std::size_t pos = start_pos; pos < end_pos; pos++) {
        if (*kv != separator)
            kv++;
        else
            return pos;
    }
    return std::string::npos;
}

#if 1

std::size_t readInputChunk(std::ifstream & ifs, std::string & input_chunk,
                           std::size_t offset, std::size_t needReadBytes)
{
    char * input_buf = &input_chunk[offset];
    std::size_t totalReadBytes = 0;

    while ((needReadBytes > 0) && !ifs.eof()) {
        ifs.read(input_buf, needReadBytes);
        std::streamsize readBytes = ifs.gcount();

        if (readBytes > 0) {
            input_buf += readBytes;
            totalReadBytes += readBytes;
            needReadBytes -= readBytes;
        }
        else break;
    }

    return totalReadBytes;
}

#else

std::size_t readInputChunk(std::ifstream & ifs, std::string & input_chunk,
                           std::size_t offset, std::size_t needReadBytes)
{
    static const std::size_t kReadBufSize = 8 * 1024;

    char rdBuf[kReadBufSize];
    char * input = &input_chunk[offset];
    std::size_t totalReadBytes = 0;

    while ((needReadBytes > 0) && !ifs.eof()) {
        std::size_t readBufSize = (needReadBytes >= kReadBufSize) ? kReadBufSize : needReadBytes;
        ifs.read(rdBuf, readBufSize);
        std::streamsize readBytes = ifs.gcount();

        if (readBytes > 0) {
            std::copy_n(&rdBuf[0], readBytes, input);
            input += readBytes;
            totalReadBytes += readBytes;
            needReadBytes -= readBytes;
        }
        else break;
    }

    return totalReadBytes;
}

#endif

#endif // BENCHMARK_H
