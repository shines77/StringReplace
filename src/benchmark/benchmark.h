
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
