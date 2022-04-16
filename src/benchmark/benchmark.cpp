
#ifdef _DEBUG
//#include <vld.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <string.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdbool>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>

#ifndef __cplusplus
#include <stdalign.h>   // C11 defines _Alignas().  This header defines alignas()
#endif

#define USE_READ_WRITE_STATISTICS   0

#include "StringMatch.h"
#include "support/StopWatch.h"

#include "algorithm/StrStr.h"
#include "algorithm/MemMem.h"
#include "algorithm/MyStrStr.h"
#include "algorithm/GlibcStrStr.h"
#include "algorithm/GlibcStrStrOld.h"
#include "algorithm/SSEStrStr.h"
#include "algorithm/SSEStrStr2.h"
#include "algorithm/SSEStrStrA.h"
#include "algorithm/SSEStrStrA_v0.h"
#include "algorithm/SSEStrStrA_v2.h"
#include "algorithm/MyMemMem.h"
#include "algorithm/MyMemMemBw.h"
#include "algorithm/FastStrStr.h"
#include "algorithm/StdSearch.h"
#include "algorithm/StdBoyerMoore.h"
#include "algorithm/Kmp.h"
#include "algorithm/KmpStd.h"
#include "algorithm/BoyerMoore.h"
#include "algorithm/Sunday.h"
#include "algorithm/Horspool.h"
#include "algorithm/QuickSearch.h"
#include "algorithm/BMTuned.h"
#include "algorithm/ShiftAnd.h"
#include "algorithm/ShiftOr.h"
#include "algorithm/WordHash.h"
#include "algorithm/Volnitsky.h"
#include "algorithm/Rabin-Karp.h"
#include "algorithm/AhoCorasick.h"

#include "win_iconv.h"

using namespace StringMatch;

void cpu_warmup(int delayTime)
{
#if defined(NDEBUG)
    double startTime, stopTime;
    double delayTimeLimit = (double)delayTime / 1000.0;
    volatile int sum = 0;

    printf("CPU warm-up begin ...\n");
    fflush(stdout);
    startTime = test::StopWatch::timestamp();
    double elapsedTime;
    do {
        for (int i = 0; i < 500; ++i) {
            sum += i;
            for (int j = 5000; j >= 0; --j) {
                sum -= j;
            }
        }
        stopTime = test::StopWatch::timestamp();
        elapsedTime = stopTime - startTime;
    } while (elapsedTime < delayTimeLimit);

    printf("sum = %u, time: %0.3f ms\n", sum, elapsedTime * 1000.0);
    printf("CPU warm-up end   ... \n\n");
    fflush(stdout);
#endif // !_DEBUG
}

void utf8_to_ansi(const std::string & utf_8, std::string & ansi)
{
#ifdef _MSC_VER
    std::size_t ansi_capacity = utf_8.size() + 1;
    ansi.resize(ansi_capacity);
    if (utf_8 == "minutes") {
        ansi_capacity = ansi_capacity;
    }
    int ansi_len = Utf8ToAnsi(utf_8.c_str(), &ansi[0], (int)ansi_capacity);
    if (ansi_len != -1) {
        assert(ansi_len <= ansi_capacity);
        ansi.resize(ansi_len - 1);
    }
#else
    ansi = utf_8;
#endif
}

struct ValueType {
    enum {
        Movie,
        Music,
        MovieAndMusic,
        Unknown,
        kMaxType = Unknown
    };

    const char * ValueText[4] = {
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

    //static const int kMaxType = 3;
    std::size_t ValueLength[4];

    ValueType() {
        for (int valueType = 0; valueType <= kMaxType; valueType++) {
            ValueLength[valueType] = std::strlen(ValueText[valueType]);
        }
    }

    int parseValueType(const std::string & dict_kv, std::size_t first, std::size_t last) {
        assert(last > first);
        const char * value_start = &dict_kv[0] + first;
        const char * value_end = &dict_kv[0] + last;
        for (int valueType = 0; valueType < kMaxType; valueType++) {
            const char * value = value_start;
            const char * valueText = &ValueText[valueType][0];
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

    const char * toString(int type) {
        assert(type >= 0 && type <= kMaxType);
        return ValueText[type];
    };

    std::size_t length(int type) {
        assert(type >= 0 && type <= kMaxType);
        return ValueLength[type];
    }
};

static ValueType sValueType;

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
                              char sep)
{
    const char * kv = (const char *)&dict_kv[start_pos];
    for (std::size_t pos = start_pos; pos < end_pos; pos++) {
        if (*kv != sep)
            kv++;
        else
            return pos;       
    }
    return std::string::npos;
}

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
    printf("preprocessing_dict_file()\n\n");

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
                int value_type = sValueType.parseValueType(dict_kv, sep_pos + 1, next_pos);

                int index = insert_kv_list(dict_list, key, value_type);
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

    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        dict_table.push_back(std::make_pair(iter->first, iter->second));
    }

    printf("\n");
}

#if 0

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

std::size_t replaceInputChunkText(std::vector<std::pair<std::string, int>> & dict_table,
                                  std::string & input_chunk, std::size_t input_chunk_size,
                                  std::vector<std::pair<int, int>> & short_keys,
                                  std::string & output_chunk, std::size_t output_offset)
{
    const char * chunk_first = input_chunk.c_str();
    const char * substr;
    int index = 0;
    for (auto iter = dict_table.begin(); iter != dict_table.end(); ++iter) {
        const char * start = chunk_first;
        const std::string & key = iter->first;
        int valueType = iter->second;
        std::size_t valueLength = sValueType.length(valueType);
        if (key.size() >= (valueLength + 2) || key.size() == valueLength) {
            do {
#ifdef _MSC_VER
                substr = std::strstr(start, key.c_str());
#else
                substr = A_strstr(start, key.c_str());
#endif
                if (substr != nullptr) {
                    // Replace text use key, filling the remaining space.
                    uint8_t * p = (uint8_t *)substr;
                    uint8_t * value = (uint8_t *)sValueType.toString(valueType);
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
                        uint8_t * p = (uint8_t *)substr;
                        // 0xFE, 0xFE, ...
                        for (std::size_t i = 0; i < key.size(); i++) {
                            *p++ = '\xFE';
                        }
                        short_keys.push_back(std::make_pair(int(start - chunk_first), index));
                        start = substr + key.size();
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
                    uint8_t * value = (uint8_t *)sValueType.toString(valueType);
                    std::size_t length = sValueType.length(valueType);

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

int StringReplace_Benchmark(const std::string & dict_file,
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
                std::size_t lastNewLinePos = input_chunk.find_last_of('\n', kReadChunkSize - 1);
                if (lastNewLinePos != std::string::npos) {
                    //char saveChar = input_chunk[lastNewLinePos];
                    input_chunk[lastNewLinePos] = '\0';
                    std::size_t output_offset = writeBufSize;
                    std::size_t outputBytes = replaceInputChunkText(
                                                    dict_table, input_chunk, lastNewLinePos,
                                                    short_keys, output_chunk, output_offset);
                    input_chunk[lastNewLinePos] = '\n';
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
                    assert((input_offset + totalReadBytes) >= (lastNewLinePos + 1));
                    std::size_t tailingBytes = kReadChunkSize - (lastNewLinePos + 1);
                    if (tailingBytes > 0) {
                        // Move the tailing bytes to head
                        std::copy_n(&input_chunk[lastNewLinePos + 1], tailingBytes, &input_chunk[0]);
                    }
                    input_offset = tailingBytes;
                }
                inputTotalSize -= totalReadBytes;
            }
            else break;
        } while (1);

        if (writeBufSize != 0) {
            writeOutputChunk(ofs, output_chunk, writeBufSize);
#if USE_READ_WRITE_STATISTICS
            globalWriteBytes += writeBufSize;
            globalWriteCount++;
#endif
        }

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

void print_arch_type()
{
#if defined(WIN64) || defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64) \
 || defined(__amd64__) || defined(__x86_64__) || defined(__aarch64__)
    printf("Arch type: __amd64__\n\n");
#elif defined(_M_IA64)
    printf("Arch type: __itanium64__\n\n");
#elif defined(_M_ARM64)
    printf("Arch type: __arm64__\n\n");
#elif defined(_M_ARM)
    printf("Arch type: __arm__\n\n");
#else
    printf("Arch type: __x86__\n\n");
#endif
}

int main(int argc, char * argv[])
{
    print_arch_type();
    ::srand((unsigned int)::time(nullptr));
#ifndef _DEBUG
    //cpu_warmup(1000);
#endif

    const char * default_dict_file = "dict.txt";
    const char * default_input_file = "video_title.txt";
    const char * default_output_file = "output_result.txt";

    const char * dict_file = nullptr;
    const char * input_file = nullptr;
    const char * output_file = nullptr;

    if (argc > 3) {
        dict_file = argv[1];
        input_file = argv[2];
        output_file = argv[3];
    } else if (argc > 2) {
        dict_file = argv[1];
        input_file = argv[2];
        output_file = default_output_file;
    } else if (argc > 1) {
        dict_file = argv[1];
        input_file = default_input_file;
        output_file = default_output_file;
    } else {
        dict_file = default_dict_file;
        input_file = default_input_file;
        output_file = default_output_file;
    }

    test::StopWatch sw;

    sw.start();
    StringReplace_Benchmark(dict_file, input_file, output_file);
    sw.stop();

    double elapsedTime = sw.getMillisec();

    printf("Elapsed time: %0.2f ms\n\n", elapsedTime);

#if (defined(_DEBUG) && defined(_MSC_VER))
    ::system("pause");
#endif
    return 0;
}
