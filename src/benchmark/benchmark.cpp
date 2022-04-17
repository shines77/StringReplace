
#if defined(_MSC_VER) && defined(_DEBUG)
#include <vld.h>
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

#ifndef __cplusplus
#include <stdalign.h>   // C11 defines _Alignas().  This header defines alignas()
#endif

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
#include "strstr_benchmark.h"

using namespace StringMatch;

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

*******************************************************************************/

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

void benchmark(const std::string & dict_file,
               const std::string & input_file,
               const std::string & output_file)
{
    test::StopWatch sw;

    sw.start();
    strstr_bench::StringReplace_Benchmark(dict_file, input_file, output_file);
    sw.stop();

    double elapsedTime = sw.getMillisec();
    printf("Elapsed time: %0.2f ms\n\n", elapsedTime);
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

    benchmark(dict_file, input_file, output_file);

#if (defined(_DEBUG) && defined(_MSC_VER))
    ::system("pause");
#endif
    return 0;
}
