
#ifndef DARTS_BENCHMARK_H
#define DARTS_BENCHMARK_H

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
#include <functional>
#include <utility>
#include <algorithm>

#include "benchmark.h"
#include "win_iconv.h"
#include "Darts.h"
#include "Darts_utf8.h"
#include "DAT_utf8.h"

//
// See: https://www.cnblogs.com/zhangchaoyang/articles/4508266.html
// See: https://www.zhihu.com/question/352900751
//

#define USE_READ_WRITE_STATISTICS   0

namespace darts_bench {

static const bool kDisplayOutput = false;

void preprocessing_dict_file(const std::string & dict_kv,
                             std::vector<std::pair<std::string, int>> & dict_list,
                             std::vector<int> & length_list)
{
    std::size_t total_size = dict_kv.size();
    printf("darts_bench::preprocessing_dict_file()\n\n");

    dict_list.clear();
    length_list.clear();

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

            dict_list.push_back(std::make_pair(key, value_type));
            length_list.push_back((std::uint32_t)key.size());

            kv_index++;
        }

        // Next line
        last_pos = next_pos + 1;
        if (next_pos == total_size)
            break;
    } while (1);

    if (kDisplayOutput) {
        int li_index = 0;
        for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
            std::string key_ansi;
            utf8_to_ansi(iter->first, key_ansi);
            printf("%4u, key: [ %s ], value: %d.\n", li_index + 1, key_ansi.c_str(), iter->second);
            li_index++;
        }
    }

    if (kDisplayOutput) {
        printf("\n");
    }
}

static
std::size_t getPatternLength(std::uint32_t pattern_id,
                             const std::vector<std::pair<std::string, int>> & dict_list)
{
    const std::pair<std::string, int> & pattern = dict_list[pattern_id];
    const std::string & key = pattern.first;
    return key.size();
}

static
int getPatternType(std::uint32_t pattern_id,
                   const std::vector<std::pair<std::string, int>> & dict_list)
{
    const std::pair<std::string, int> & pattern = dict_list[pattern_id];
    int valueType = pattern.second;
    return valueType;
}

template <typename AcTrieT>
std::size_t replaceInputChunkText(AcTrieT & acTrie,
                                  const std::vector<std::pair<std::string, int>> & dict_list,
                                  const std::string & input_chunk, std::size_t input_chunk_size,
                                  std::string & output_chunk, std::size_t output_offset)
{
    uint8_t * input_end = (uint8_t *)input_chunk.c_str() + input_chunk_size;
    uint8_t * output = (uint8_t *)output_chunk.c_str() + output_offset;
    uint8_t * output_start = output;

    std::size_t line_no = 0;
    uint8_t * line_first = (uint8_t *)input_chunk.c_str();
    uint8_t * line_last;

    typedef typename AcTrieT::MatchInfo MatchInfo;

    while (line_first < input_end) {
#if 0
        line_last = StrUtils::find(line_first, input_end, uint8_t('\n'));
#else
        size_t length = (size_t)(input_end - line_first) * sizeof(uint8_t);
        line_last = (uint8_t *)std::memchr(line_first, '\n', length);
#endif
        if (line_last == nullptr)
            line_last = input_end;

        MatchInfo matchInfo;
        while (line_first < line_last) {
            bool matched = acTrie.match_one(line_first, line_last, matchInfo);
            if (!matched) {
                while (line_first < line_last) {
                    *output++ = *line_first++;
                }
                break;
            } else {
                std::uint32_t match_end = matchInfo.end;
                std::uint32_t pattern_id = matchInfo.pattern_id;
                assert(pattern_id < (std::uint32_t)dict_list.size());

                const std::pair<std::string, int> & dict_info = dict_list[pattern_id];
                const std::string & key = dict_info.first;
                std::string ansi_key;
                utf8_to_ansi(key, ansi_key);

                assert(match_end >= (std::uint32_t)key.size());
                std::uint32_t match_begin = match_end - (std::uint32_t)key.size();

                uint8_t * line_mid = line_first + match_begin;
                while (line_first < line_mid) {
                    *output++ = *line_first++;
                }

#if defined(_MSC_VER)
                int valueType = dict_info.second;
                std::size_t valueLength = ValueType::length(valueType);
                ValueType::writeType(output, valueType);
                output += valueLength;
#else
                int valueType = dict_info.second;
                uint8_t * value = (uint8_t *)ValueType::toString(valueType);
                std::size_t valueLength = ValueType::length(valueType);

                uint8_t * value_end = value + valueLength;
                while (value < value_end) {
                    *output++ = *value++;
                }
#endif
                line_first += key.size();
            }
        }

        line_no++;

        // Next line
        line_first = line_last + 1;
        if (line_last != input_end)
            *output++ = '\n';
        else
            break;
    }

    assert(output >= output_start);
    return std::size_t(output - output_start);
}

template <typename AcTrieT>
std::size_t replaceInputChunkTextEx(AcTrieT & acTrie,
                                    const std::vector<std::pair<std::string, int>> & dict_list,
                                    const std::vector<int> & length_list,
                                    const std::string & input_chunk, std::size_t input_chunk_size,
                                    std::string & output_chunk, std::size_t output_offset)
{
    uint8_t * input_end = (uint8_t *)input_chunk.c_str() + input_chunk_size;
    uint8_t * output = (uint8_t *)output_chunk.c_str() + output_offset;
    uint8_t * output_start = output;

    std::size_t line_no = 0;
    uint8_t * line_first = (uint8_t *)input_chunk.c_str();
    uint8_t * line_last;

    typedef typename AcTrieT::MatchInfoEx MatchInfoEx;

    std::vector<MatchInfoEx> match_list;
#if 0
    static typename AcTrieT::on_hit_callback onHit_callback =
        std::bind(&getPatternLength, std::placeholders::_1, dict_list);
#endif

    while (line_first < input_end) {
#if 0
        line_last = StrUtils::find(line_first, input_end, uint8_t('\n'));
#else
        size_t length = (size_t)(input_end - line_first) * sizeof(uint8_t);
        line_last = (uint8_t *)std::memchr(line_first, '\n', length);
#endif
        if (line_last == nullptr)
            line_last = input_end;

        acTrie.match_one(line_first, line_last, match_list, length_list);

        if (likely(match_list.size() == 0)) {
            while (line_first < line_last) {
                *output++ = *line_first++;
            }
        } else {
            uint8_t * line_start = line_first;
            for (auto iter = match_list.begin(); iter != match_list.end(); ++iter) {
                const MatchInfoEx & matchInfo = *iter;
                std::uint32_t match_begin = matchInfo.begin;
                std::uint32_t match_end   = matchInfo.end;
                std::uint32_t pattern_id  = matchInfo.pattern_id;
                assert(pattern_id < (std::uint32_t)dict_list.size());

                const std::pair<std::string, int> & dict_info = dict_list[pattern_id];
                const std::string & key = dict_info.first;
                std::string ansi_key;
                utf8_to_ansi(key, ansi_key);

                assert(std::uint32_t(match_end - match_begin) == (std::uint32_t)key.size());

                uint8_t * line_mid = line_start + match_begin;
                assert(line_first <= line_mid);
                while (line_first < line_mid) {
                    *output++ = *line_first++;
                }

#if defined(_MSC_VER)
                int valueType = dict_info.second;
                std::size_t valueLength = ValueType::length(valueType);
                ValueType::writeType(output, valueType);
                output += valueLength;
#else
                int valueType = dict_info.second;
                uint8_t * value = (uint8_t *)ValueType::toString(valueType);
                std::size_t valueLength = ValueType::length(valueType);

                uint8_t * value_end = value + valueLength;
                while (value < value_end) {
                    *output++ = *value++;
                }
#endif
                line_first += std::size_t(match_end - match_begin);
            }

            while (line_first < line_last) {
                *output++ = *line_first++;
            }
        }

        line_no++;

        // Next line
        line_first = line_last + 1;
        if (line_last != input_end)
            *output++ = '\n';
        else
            break;
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

template <typename AcTrieT>
int StringReplace(const std::string & name,
                  const std::string & dict_file,
                  const std::string & input_file,
                  const std::string & in_output_file)
{
    std::string dict_kv;
    std::size_t dict_filesize = read_dict_file(dict_file, dict_kv);
    if (dict_filesize == 0) {
        std::cout << "dict_file [ " << dict_file << " ] read failed." << std::endl;
        return -1;
    }

    std::string output_file = splicing_file_name(in_output_file, name);

    std::vector<std::pair<std::string, int>> dict_list;
    std::vector<int> length_list;

    preprocessing_dict_file(dict_kv, dict_list, length_list);

    static const std::size_t kPageSize = 4 * 1024;
    static const std::size_t kReadChunkSize = 64 * 1024;
    static const std::size_t kWriteBlockSize = 128 * 1024;

    test::StopWatch sw;

    sw.start();

    AcTrieT ac_trie;
    std::uint32_t index = 0;
    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        const std::string & key = iter->first;
        ac_trie.insert(key, index);
        index++;
    }

    ac_trie.build();

    sw.stop();

    double elapsedTime = sw.getMillisec();

    ac_trie.clear_ac_trie();
    printf("darts_trie.max_state_id() = %u\n", (uint32_t)ac_trie.max_state_id());
    printf("darts_trie build elapsed time: %0.2f ms\n\n", elapsedTime);

#if USE_READ_WRITE_STATISTICS
    std::size_t globalReadBytes = 0;
    std::size_t globalReadCount = 0;

    std::size_t globalWriteBytes = 0;
    std::size_t globalWriteCount = 0;
#endif

    bool has_overflow_labels = ac_trie.has_overflow_labels();

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

        input_chunk.resize(kReadChunkSize + kPageSize);
        output_chunk.resize(kWriteBlockSize + kReadChunkSize * 2 + kPageSize);

        std::size_t input_offset = 0;
        std::size_t writeBufSize = 0;

        do {
            std::size_t totalReadBytes = readInputChunk(ifs, input_chunk, input_offset,
                                                        kReadChunkSize);
            if (totalReadBytes > 0) {
#if USE_READ_WRITE_STATISTICS
                globalReadBytes += totalReadBytes;
                globalReadCount++;
#endif
                std::size_t input_chunk_last;
                std::size_t actualInputChunkBytes = input_offset + totalReadBytes;
#if defined(_MSC_VER)
                std::size_t lastNewLinePos = StrUtils::rfind(input_chunk, '\n', actualInputChunkBytes);
                if (lastNewLinePos == std::string::npos)
                    input_chunk_last = actualInputChunkBytes;
                else
                    input_chunk_last = lastNewLinePos + 1;
#else
                size_t length = (size_t)actualInputChunkBytes;
                const char * last_newline = (const char *)memrchr(input_chunk.c_str(), '\n', length);
                input_chunk_last = (last_newline == nullptr) ? actualInputChunkBytes
                                 : (last_newline - input_chunk.c_str() + 1);
#endif
                //char saveChar = input_chunk[input_chunk_last];
                //input_chunk[input_chunk_last] = '\0';
                std::size_t output_offset = writeBufSize;
                std::size_t outputBytes = replaceInputChunkText<AcTrieT>(
                                                ac_trie, dict_list,
                                                input_chunk, input_chunk_last,
                                                output_chunk, output_offset);
                //input_chunk[input_chunk_last] = saveChar;
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
                    std::size_t outputBytes = replaceInputChunkText<AcTrieT>(
                                                    ac_trie, dict_list,
                                                    input_chunk, input_chunk_last,
                                                    output_chunk, output_offset);
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

template <typename AcTrieT>
int StringReplaceEx(const std::string & name,
                    const std::string & dict_file,
                    const std::string & input_file,
                    const std::string & in_output_file)
{
    std::string dict_kv;
    std::size_t dict_filesize = read_dict_file(dict_file, dict_kv);
    if (dict_filesize == 0) {
        std::cout << "dict_file [ " << dict_file << " ] read failed." << std::endl;
        return -1;
    }

    std::string output_file = splicing_file_name(in_output_file, name);

    std::vector<std::pair<std::string, int>> dict_list;
    std::vector<int> length_list;

    preprocessing_dict_file(dict_kv, dict_list, length_list);

    static const std::size_t kPageSize = 4 * 1024;
    static const std::size_t kReadChunkSize = 64 * 1024;
    static const std::size_t kWriteBlockSize = 128 * 1024;

    test::StopWatch sw;

    sw.start();

    AcTrieT ac_trie;
    std::uint32_t index = 0;
    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        const std::string & key = iter->first;
        ac_trie.insert(key, index);
        index++;
    }

    ac_trie.build();

    sw.stop();

    double elapsedTime = sw.getMillisec();

    ac_trie.clear_ac_trie();
    printf("darts_trie.max_state_id() = %u\n", (uint32_t)ac_trie.max_state_id());
    printf("darts_trie build elapsed time: %0.2f ms\n\n", elapsedTime);

#if USE_READ_WRITE_STATISTICS
    std::size_t globalReadBytes = 0;
    std::size_t globalReadCount = 0;

    std::size_t globalWriteBytes = 0;
    std::size_t globalWriteCount = 0;
#endif

    bool has_overflow_labels = ac_trie.has_overflow_labels();

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

        input_chunk.resize(kReadChunkSize + kPageSize);
        output_chunk.resize(kWriteBlockSize + kReadChunkSize * 2 + kPageSize);

        std::size_t input_offset = 0;
        std::size_t writeBufSize = 0;

        do {
            std::size_t totalReadBytes = readInputChunk(ifs, input_chunk, input_offset,
                                                        kReadChunkSize);
            if (totalReadBytes > 0) {
#if USE_READ_WRITE_STATISTICS
                globalReadBytes += totalReadBytes;
                globalReadCount++;
#endif
                std::size_t input_chunk_last;
                std::size_t actualInputChunkBytes = input_offset + totalReadBytes;
#if defined(_MSC_VER)
                std::size_t lastNewLinePos = StrUtils::rfind(input_chunk, '\n', actualInputChunkBytes);
                if (lastNewLinePos == std::string::npos)
                    input_chunk_last = actualInputChunkBytes;
                else
                    input_chunk_last = lastNewLinePos + 1;
#else
                size_t length = (size_t)actualInputChunkBytes;
                const char * last_newline = (const char *)memrchr(input_chunk.c_str(), '\n', length);
                input_chunk_last = (last_newline == nullptr) ? actualInputChunkBytes
                                 : (last_newline - input_chunk.c_str() + 1);
#endif
                //char saveChar = input_chunk[input_chunk_last];
                //input_chunk[input_chunk_last] = '\0';
                std::size_t output_offset = writeBufSize;
                std::size_t outputBytes = replaceInputChunkTextEx<AcTrieT>(
                                                ac_trie, dict_list, length_list,
                                                input_chunk, input_chunk_last,
                                                output_chunk, output_offset);
                //input_chunk[input_chunk_last] = saveChar;
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
                    std::size_t outputBytes = replaceInputChunkTextEx<AcTrieT>(
                                                    ac_trie, dict_list, length_list,
                                                    input_chunk, input_chunk_last,
                                                    output_chunk, output_offset);
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

} // namespace darts_bench

#undef USE_READ_WRITE_STATISTICS

#endif // DARTS_BENCHMARK_H
