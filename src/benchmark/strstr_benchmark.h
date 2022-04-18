
#ifndef STRSTR_BENCHMARK_H
#define STRSTR_BENCHMARK_H

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

#include "benchmark.h"
#include "win_iconv.h"

#define USE_READ_WRITE_STATISTICS   0

namespace strstr_bench {

inline int string_compare(const std::string & key1, const std::string & key2)
{
    std::size_t key_len1 = key1.size();
    std::size_t key_len2 = key2.size();
    std::size_t key_len = (key_len1 <= key_len2) ? key_len1 : key_len2;
    std::size_t i;
    for (i = 0; i < key_len; i++) {
        if (key1[i] != key2[i]) {
            return ((uint8_t)key1[i] > (uint8_t)key2[i]) ? 1 : -1;
        }
    }

    if (key_len1 > key_len2)
        return 1;
    else if (key_len1 < key_len2)
        return -1;
    else
        return 0;
}

int insert_kv_list(std::list<std::pair<std::string, int>> & dict_list,
                   const std::string & in_key, int value_type)
{
    int index = 0;
    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        const std::string & key = iter->first;
        int cmp = string_compare(in_key, key);
        if (cmp > 0) {
            dict_list.insert(iter, std::make_pair(in_key, value_type));
            return index;
        }
        else if (cmp == 0) {
            assert(false);
            return -1;
        }
        index++;
    }

    // Append to the tail
    dict_list.push_back(std::make_pair(in_key, value_type));
    return index;
}

void preprocessing_dict_file(const std::string & dict_kv,
                             std::list<std::pair<std::string, int>> & dict_list,
                             std::vector<std::pair<std::string, int>> & dict_table)
{
    std::size_t total_size = dict_kv.size();
    printf("strstr_bench::preprocessing_dict_file()\n\n");

    bool last_line_processed = false;
    std::uint32_t kv_index = 0;
    std::size_t last_pos = 0;
    do {
        std::size_t next_pos = dict_kv.find_first_of('\n', last_pos);
        if (next_pos == std::string::npos)
            next_pos = total_size;

        std::size_t sep_pos = find_kv_separator(dict_kv, last_pos, next_pos, '\t');
        if (sep_pos != std::string::npos) {
            std::string key, value;
            std::size_t key_len = sep_pos - last_pos;
            key.resize(key_len);
            dict_kv.copy(&key[0], key_len, last_pos);
#if 0
            std::size_t value_len = next_pos - (sep_pos + 1);
            value.resize(value_len);
            dict_kv.copy(&value[0], value_len, sep_pos + 1);

            std::string key_ansi;
            utf8_to_ansi(key, key_ansi);

            std::string value_ansi;
            utf8_to_ansi(value, value_ansi);
            printf("%4u, key: [ %s ], value: [ %s ].\n", kv_index + 1, key_ansi.c_str(), value_ansi.c_str());
#endif
            int value_type = ValueType::parseValueType(dict_kv, sep_pos + 1, next_pos);

            int index = insert_kv_list(dict_list, key, value_type);
        }

        kv_index++;

        // Next line
        last_pos = next_pos + 1;
        if (next_pos == total_size)
            break;
    } while (1);

    int li_index = 0;
    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        std::string key_ansi;
        utf8_to_ansi(iter->first, key_ansi);
        printf("%4u, key: [ %s ], value: %d.\n", li_index + 1, key_ansi.c_str(), iter->second);
        li_index++;
    }

    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        dict_table.push_back(std::make_pair(iter->first, iter->second));
    }

    printf("\n");
}

std::size_t replaceInputChunkText(std::vector<std::pair<std::string, int>> & dict_table,
                                  std::string & input_chunk, std::size_t input_chunk_size,
                                  std::vector<std::pair<int, int>> & short_keys,
                                  std::string & output_chunk, std::size_t output_offset)
{
    const char * chunk_first = input_chunk.c_str();
    const char * chunk_last = input_chunk.c_str() + input_chunk_size;
    const char * substr;
    int index = 0;

    if (input_chunk_size == 0)
        return 0;

    for (auto iter = dict_table.begin(); iter != dict_table.end(); ++iter) {
        const char * start = chunk_first;
        const std::string & key = iter->first;
        int valueType = iter->second;
        std::size_t valueLength = ValueType::length(valueType);
        if (key.size() >= (valueLength + 2) || key.size() == valueLength) {
            do {
#ifdef _MSC_VER
                substr = std::strstr(start, key.c_str());
#else
                substr = A_strstr(start, key.c_str());
#endif
                if (substr != nullptr) {
                    if ((substr + key.size()) > chunk_last || substr >= chunk_last)
                        break;
                    // Replace text use key, filling the remaining space.
                    uint8_t * p = (uint8_t *)substr;
                    uint8_t * value = (uint8_t *)ValueType::toString(valueType);
                    std::size_t i;
                    for (i = 0; i < valueLength; i++) {
                        *p++ = *value++;
                    }
                    // If there are two continuous 0xFF, it means empty chars.
                    std::size_t r = key.size() - i;
                    assert(r == 0 || r >= 2);
                    for (; i < key.size(); i++) {
                        *p++ = '\xFF';
                    }
                    start = substr + key.size();
                    if (start >= chunk_last)
                        break;
                }
                else break;
            } while (1);
        } else {
            if (true || key.size() >= 3) {
                do {
#ifdef _MSC_VER
                    substr = std::strstr(start, key.c_str());
#else
                    substr = A_strstr(start, key.c_str());
#endif
                    if (substr != nullptr) {
                        if ((substr + key.size()) > chunk_last || substr >= chunk_last)
                            break;

                        uint8_t * p = (uint8_t *)substr;
                        // Because 0xFF, 0xFE does not appear in the UTF-8 encoding forever.
                        // 0xFF, index:high, index:low, filling ...
                        *p++ = '\xFF';
                        *p++ = uint8_t(index / 128) + 128;
                        *p++ = uint8_t(index % 128) + 128;
                        std::size_t i;
                        for (i = 3; i < key.size(); i++) {
                            *p++ = '\x1';
                        }
                        start = substr + key.size();
                        if (start >= chunk_last)
                            break;
                    }
                    else break;
                } while (1);
            } else {
#if 0
                do {
#ifdef _MSC_VER
                    substr = std::strstr(start, key.c_str());
#else
                    substr = A_strstr(start, key.c_str());
#endif
                    if (substr != nullptr) {
                        if ((substr + key.size()) > chunk_last || substr >= chunk_last)
                            break;

                        uint8_t * p = (uint8_t *)substr;
                        // 0xFE, 0xFE, ...
                        for (std::size_t i = 0; i < key.size(); i++) {
                            *p++ = '\xFE';
                        }
                        short_keys.push_back(std::make_pair(int(start - chunk_first), index));
                        start = substr + key.size();
                        if (start >= chunk_last)
                            break;
                    }
                    else break;
                } while (1);
#endif
            }
        }
        index++;
    }

    // Real replacement
    uint8_t * input = (uint8_t *)input_chunk.c_str();
    uint8_t * input_end = (uint8_t *)input_chunk.c_str() + input_chunk_size;
    uint8_t * output = (uint8_t *)output_chunk.c_str() + output_offset;
    uint8_t * output_start = output;

    while (input < input_end) {
        uint8_t ch1;
        uint8_t ch0 = *input;
        if (ch0 < uint8_t('\xFE')) {
            *output++ = *input++;
        } else {
            if (ch0 == uint8_t('\xFF')) {
                ch1 = *(input + 1);
                if (ch1 != uint8_t('\xFF')) {
                    uint8_t ch2 = *(input + 2);
                    assert(ch1 >= 128);
                    assert(ch2 >= 128);
                    int index = (int)(ch1 - 128) * 128 + (ch2 - 128);
                    assert(index >= 0 && index < dict_table.size());

                    const std::string & key = dict_table[index].first;
                    std::string ansi_key;
                    utf8_to_ansi(key, ansi_key);

                    int valueType = dict_table[index].second;
                    uint8_t * value = (uint8_t *)ValueType::toString(valueType);
                    std::size_t length = ValueType::length(valueType);

                    for (std::size_t i = 0; i < length; i++) {
                        *output++ = *value++;
                    }

                    assert(key.size() >= 3);
                    input += key.size();
                } else {
                    // Two continuous 0xFF
                    input += 2;
                    // Skip all 0xFF chars
                    while (*input == uint8_t('\xFF')) {
                        input++;
                    }
                }
            } else {
#if 0
                // 0xFE
                assert(ch0 == uint8_t('\xFE'));
                input++;

                // Skip all 0xFE chars
                while (*input == uint8_t('\xFE')) {
                    input++;
                }

                // TODO: XXXXX
                //
                assert(false);
#endif
            }
        }
    }

    assert(output >= output_start);
    return std::size_t(output - output_start);
}

inline void writeOutputChunk(std::ofstream & ofs,
                             const std::string & output_chunk,
                             std::size_t writeBlockSize)
{
    ofs.write(output_chunk.c_str(), writeBlockSize);
}

int StringReplace(const std::string & dict_file,
                  const std::string & input_file,
                  const std::string & output_file)
{
    std::string dict_kv;
    std::size_t dict_filesize = read_dict_file(dict_file, dict_kv);
    if (dict_filesize == 0) {
        std::cout << "dict_file [ " << dict_file << " ] read failed." << std::endl;
        return -1;
    }

    std::list<std::pair<std::string, int>> dict_list;
    std::vector<std::pair<std::string, int>> dict_table;
    preprocessing_dict_file(dict_kv, dict_list, dict_table);

    static const std::size_t kPageSize = 4 * 1024;
    static const std::size_t kReadChunkSize = 64 * 1024;
    static const std::size_t kWriteBlockSize = 128 * 1024;

#if USE_READ_WRITE_STATISTICS
    std::size_t globalReadBytes = 0;
    std::size_t globalReadCount = 0;

    std::size_t globalWriteBytes = 0;
    std::size_t globalWriteCount = 0;
#endif

    std::ifstream ifs;
    ifs.open(input_file, std::ios::in | std::ios::binary);
    if (ifs.good()) {
        ifs.seekg(0, std::ios::end);
        std::size_t inputTotalSize = (std::size_t)ifs.tellg();
        ifs.seekg(0, std::ios::beg);
        std::streampos read_pos = ifs.tellg();

        std::ofstream ofs;
        ofs.open(output_file, std::ios::out | std::ios::binary);
        if (!ofs.good()) {
            ifs.close();
            return -1;
        }
        ofs.seekp(0, std::ios::beg);

        std::string input_chunk;
        std::string output_chunk;
        std::vector<std::pair<int, int>> short_keys;

        input_chunk.resize(kReadChunkSize + kPageSize);
        output_chunk.resize(kWriteBlockSize + kReadChunkSize + kPageSize);

        std::size_t input_offset = 0;
        std::size_t writeBufSize = 0;

        do {
            short_keys.clear();

            std::size_t totalReadBytes = readInputChunk(ifs, input_chunk, input_offset,
                                                        kReadChunkSize - input_offset);
            if (totalReadBytes != 0) {
#if USE_READ_WRITE_STATISTICS
                globalReadBytes += totalReadBytes;
                globalReadCount++;
#endif
                std::size_t input_chunk_last;
                std::size_t actualInputChunkBytes = input_offset + totalReadBytes;
                std::size_t lastNewLinePos = input_chunk.find_last_of('\n', actualInputChunkBytes - 1);
                if (lastNewLinePos == std::string::npos)
                    input_chunk_last = actualInputChunkBytes;
                else
                    input_chunk_last = lastNewLinePos + 1;

                char saveChar = input_chunk[input_chunk_last];
                input_chunk[input_chunk_last] = '\0';
                std::size_t output_offset = writeBufSize;
                std::size_t outputBytes = replaceInputChunkText(
                                                dict_table, input_chunk, input_chunk_last,
                                                short_keys, output_chunk, output_offset);
                input_chunk[input_chunk_last] = saveChar;
                writeBufSize += outputBytes;
                if (writeBufSize >= kWriteBlockSize) {
                    writeOutputChunk(ofs, output_chunk, kWriteBlockSize);
                    std::size_t remainBytes = writeBufSize - kWriteBlockSize;
                    // Move the remaining bytes to head
                    std::copy_n(&output_chunk[kWriteBlockSize], remainBytes, &output_chunk[0]);
                    writeBufSize -= kWriteBlockSize;
#if USE_READ_WRITE_STATISTICS
                    globalWriteBytes += kWriteBlockSize;
                    globalWriteCount++;
#endif
                }
                assert(actualInputChunkBytes >= input_chunk_last);
                std::size_t tailingBytes = actualInputChunkBytes - input_chunk_last;
                if (tailingBytes > 0) {
                    // Move the tailing bytes to head
                    std::copy_n(&input_chunk[input_chunk_last], tailingBytes, &input_chunk[0]);
                }
                input_offset = tailingBytes;

                inputTotalSize -= totalReadBytes;
            } else {
                if (input_offset > 0) {
                    std::size_t input_chunk_last = input_offset;

                    char saveChar = input_chunk[input_chunk_last];
                    input_chunk[input_chunk_last] = '\0';
                    std::size_t output_offset = writeBufSize;
                    std::size_t outputBytes = replaceInputChunkText(
                                                    dict_table, input_chunk, input_chunk_last,
                                                    short_keys, output_chunk, output_offset);
                    input_chunk[input_chunk_last] = saveChar;
                    writeBufSize += outputBytes;
                }
                if (writeBufSize > 0) {
                    writeOutputChunk(ofs, output_chunk, writeBufSize);
#if USE_READ_WRITE_STATISTICS
                    globalWriteBytes += writeBufSize;
                    globalWriteCount++;
#endif
                }
                break;
            }
        } while (1);

        assert(inputTotalSize == 0);

        ofs.close();
        ifs.close();

#if USE_READ_WRITE_STATISTICS
        printf("globalReadCount = %" PRIu64 ", globalWriteCount = %" PRIu64 ".\n",
                globalReadCount, globalWriteCount);
        printf("globalReadBytes = %" PRIu64 ", globalWriteBytes = %" PRIu64 ".\n",
                globalReadBytes, globalWriteBytes);
        printf("\n");
#endif
        return 0;
    }

    return -1;
}

} // namespace strstr_bench

#undef USE_READ_WRITE_STATISTICS

#endif // STRSTR_BENCHMARK_H
