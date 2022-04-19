
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

#include "StringMatch.h"
#include "support/StopWatch.h"

static const char * gValueText[4] = {
#ifdef _MSC_VER
    u8"-*电影*-",
    u8"-*音乐*-",
    u8"-*音乐&电影*-",
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

    static const char * toString(int type) {
        assert(type >= 0 && type <= kMaxType);
        return gValueText[type];
    };

    static std::size_t length(int type) {
        assert(type >= 0 && type <= kMaxType);
        return gValueLength[type];
    }
};

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
