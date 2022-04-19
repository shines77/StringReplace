
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
#include <utility>
#include <algorithm>

#include "benchmark.h"
#include "win_iconv.h"
#include "AhoCorasick.h"

//
// See: https://www.cnblogs.com/zhangchaoyang/articles/4508266.html
// See: https://www.zhihu.com/question/352900751
//

#define USE_READ_WRITE_STATISTICS   0

namespace darts_bench {

void AcTire_test()
{
    AcTrie<char> ac_trie;
    ac_trie.appendPattern("abcd", 4, 0);
    ac_trie.appendPattern("abef", 4, 1);
    ac_trie.appendPattern("ghjsdasf", 8, 2);
    ac_trie.appendPattern("Hello", 5, 3);
    ac_trie.appendPattern("Hello World", 11, 4);
    ac_trie.appendPattern("test", 4, 5);

    ac_trie.build();

    printf("ac_trie.state(30).is_final = %u\n\n", ac_trie.state(30).is_final);
}

void preprocessing_dict_file(const std::string & dict_kv,
                             std::vector<std::pair<std::string, int>> & dict_list)
{
    std::size_t total_size = dict_kv.size();
    printf("darts_bench::preprocessing_dict_file()\n\n");

    std::uint32_t kv_index = 0;
    std::size_t last_pos = 0;
    do {
        std::size_t next_pos = dict_kv.find_first_of('\n', last_pos);
        if (next_pos != std::string::npos) {
Find_KV:
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
            }
            last_pos = next_pos + 1;
            kv_index++;
        } else {
            if (next_pos < total_size) {
                next_pos = total_size;
                goto Find_KV;
            }
            else break;
        }
    } while (1);

    int li_index = 0;
    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        std::string key_ansi;
        utf8_to_ansi(iter->first, key_ansi);
        printf("%4u, key: [ %s ], value: %d.\n", li_index + 1, key_ansi.c_str(), iter->second);
        li_index++;
    }

    printf("\n");
}

std::size_t replaceInputChunkText(AcTrie<char> & acTrie,
                                  std::vector<std::pair<std::string, int>> & dict_list,
                                  std::string & input_chunk, std::size_t input_chunk_size,
                                  std::string & output_chunk, std::size_t output_offset)
{
    uint8_t * input_end = (uint8_t *)input_chunk.c_str() + input_chunk_size;
    uint8_t * output = (uint8_t *)output_chunk.c_str() + output_offset;
    uint8_t * output_start = output;

    bool last_line_processed = false;
    std::size_t line_no = 0;
    std::size_t offset = 0;
    uint8_t * line_first = (uint8_t *)input_chunk.c_str() + offset;
    uint8_t * line_last;

    while (line_first < input_end) {
        std::size_t newline = input_chunk.find_first_of('\n', offset);
        if (newline == std::string::npos)
            newline = input_chunk_size;

        line_first = (uint8_t *)input_chunk.c_str() + offset;
        line_last = (uint8_t *)input_chunk.c_str() + newline;
        AcTrie<char>::MatchInfo matchInfo;
        while (line_first < line_last) {
            bool matched = acTrie.search(line_first, line_last, matchInfo);
            if (!matched) {
                while (line_first < line_last) {
                    *output++ = *line_first++;
                }
                break;
            } else {
                std::size_t match_last = matchInfo.last;
                std::size_t pattern_id = matchInfo.pattern_id;
                assert(pattern_id < dict_list.size());

                const std::pair<std::string, int> & dict_info = dict_list[pattern_id];
                const std::string & key = dict_info.first;
                assert(match_last >= key.size());
                std::size_t match_first = match_last - key.size();

                uint8_t * line_mid = line_first + match_first;
                while (line_first < line_mid) {
                    *output++ = *line_first++;
                }

                int valueType = dict_info.second;
                uint8_t * value = (uint8_t *)ValueType::toString(valueType);
                std::size_t valueLength = ValueType::length(valueType);

                uint8_t * value_end = value + valueLength;
                while (value < value_end) {
                    *output++ = *value++;
                }

                line_first += key.size();
            }
        }

        line_no++;
#ifdef _DEBUG
        if (line_no == 430) {
            line_no = line_no;
        }
#endif
        // Next line
        offset = newline + 1;
        if (newline != input_chunk_size)
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

    std::vector<std::pair<std::string, int>> dict_list;
    preprocessing_dict_file(dict_kv, dict_list);

    static const std::size_t kPageSize = 4 * 1024;
    static const std::size_t kReadChunkSize = 64 * 1024;
    static const std::size_t kWriteBlockSize = 128 * 1024;

    test::StopWatch sw;

    sw.start();

    AcTrie<char> ac_trie;
    std::uint32_t index = 0;
    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        const std::string & key = iter->first;
        ac_trie.appendPattern(key.c_str(), key.size(), index);
        index++;
    };

    ac_trie.build();

    sw.stop();

    double elapsedTime = sw.getMillisec();

    printf("ac_trie.max_state_id() = %u\n", (uint32_t)ac_trie.max_state_id());
    printf("ac_trie.state(30).is_final = %u\n\n", ac_trie.state(30).is_final);

    printf("ac_trie build elapsed time: %0.2f ms\n\n", elapsedTime);

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

        input_chunk.resize(kReadChunkSize + kPageSize);
        output_chunk.resize(kWriteBlockSize + kReadChunkSize + kPageSize);

        std::size_t input_offset = 0;
        std::size_t writeBufSize = 0;

        do {
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

                //char saveChar = input_chunk[input_chunk_last];
                //input_chunk[input_chunk_last] = '\0';
                std::size_t output_offset = writeBufSize;
                std::size_t outputBytes = replaceInputChunkText(
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
                    std::size_t outputBytes = replaceInputChunkText(
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

} // namespace darts_bench

#undef USE_READ_WRITE_STATISTICS

#endif // DARTS_BENCHMARK_H
