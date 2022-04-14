
#ifdef _DEBUG
//#include <vld.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
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
#include <vector>
#include <algorithm>

#ifndef __cplusplus
#include <stdalign.h>   // C11 defines _Alignas().  This header defines alignas()
#endif

#define TEST_ALL_BENCHMARK          1
#define USE_ALIGNED_PATTAEN         1

#define SWITCH_BENCHMARK_TEST       0
#define ENABLE_AHOCORASICK_TEST     0

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

static const size_t kInputReadBufSize = 64 * 1024;
static const size_t kOutputWriteBufSize = 128 * 1024;

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
    std::size_t ansi_len = utf_8.size() + 1;
    ansi.resize(ansi_len);
    Utf8ToAnsi(utf_8.c_str(), &ansi[0], (int)ansi_len);
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
        u8"-*电影*-",
        u8"-*音乐*-",
        u8"-*音乐&电影*-",
        "Unknown"
    };

    //static const int kMaxType = 3;

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
        assert(type >= 0 && type < kMaxType);
        return ValueText[type];
    };
};

std::size_t read_dict_file(const std::string & dict_file, std::string & content)
{
    static const size_t kReadBufSize = 8 * 1024;

    content.clear();

    std::ifstream ifs;
    ifs.open(dict_file, std::ios::in | std::ios::binary);
    if (ifs.good()) {
        ifs.seekg(0, std::ios::end);
        std::fstream::pos_type totalSize = ifs.tellg();
        content.reserve(totalSize);
        ifs.seekg(0, std::ios::beg);

        while (!ifs.eof()) {
            char buffer[kReadBufSize];
            ifs.read(buffer, kReadBufSize);
            std::streamsize readBytes = ifs.gcount();
            if (readBytes != 0)
                content.append(buffer, readBytes);
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

void process_dict_file(const std::string & dict_kv,
                       std::list<std::pair<std::string, int>> & dict_list)
{
    std::size_t total_size = dict_kv.size();
    printf("process_dict_file()\n\n");

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
                ValueType vt;
                int value_type = vt.parseValueType(dict_kv, sep_pos + 1, next_pos);

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

    ValueType vt;
    int li_index = 0;
    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        std::string key_ansi;
        utf8_to_ansi(iter->first, key_ansi);
        printf("%4u, key: [ %s ], value: %d.\n", li_index + 1, key_ansi.c_str(), iter->second);
        li_index++;
    }

    printf("\n");
}

void StringReplace_Benchmark(const std::string & dict_file,
                             const std::string & input_file,
                             const std::string & output_file)
{
    std::string dict_kv;
    std::size_t dict_filesize = read_dict_file(dict_file, dict_kv);
    if (dict_filesize == 0) {
        std::cout << "dict_file [ " << dict_file << " ] read failed." << std::endl;
        return;
    }

    std::list<std::pair<std::string, int>> dict_list;
    process_dict_file(dict_kv, dict_list);
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
    const char * default_output_file = "result_output.txt";

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

    StringReplace_Benchmark(dict_file, input_file, output_file);

#if (defined(_DEBUG) && defined(_MSC_VER))
    ::system("pause");
#endif
    return 0;
}
