
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

namespace darts_bench {

void AcTire_test()
{
    AcTrie<char> ac_trie;
    ac_trie.appendPattern("abcd", 4);
    ac_trie.appendPattern("abef", 4);
    ac_trie.appendPattern("ghjsdasf", 8);
    ac_trie.appendPattern("Hello", 5);
    ac_trie.appendPattern("Hello World", 11);
    ac_trie.appendPattern("test", 4);

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

    AcTrie<char> ac_trie;
    for (auto iter = dict_list.begin(); iter != dict_list.end(); ++iter) {
        const std::string & key = iter->first;
        ac_trie.appendPattern(key.c_str(), key.size());
    };

    ac_trie.build();

    printf("ac_trie.max_state_id() = %u\n", (uint32_t)ac_trie.max_state_id());
    printf("ac_trie.state(30).is_final = %u\n\n", ac_trie.state(30).is_final);

    return 0;
}

} // namespace darts_bench

#endif // DARTS_BENCHMARK_H
