
#ifndef AHO_CORASICK_AUTO_H
#define AHO_CORASICK_AUTO_H

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
#include <set>
#include <map>
#include <utility>
#include <algorithm>
#include <type_traits>

#include "benchmark.h"
#include "win_iconv.h"

namespace darts_bench {

//
// See: https://zhuanlan.zhihu.com/p/137614862
// See: https://www.zhihu.com/question/352900751
// See: https://nark.cc/p/?p=1453
//
template <typename CharTy>
class AcTrie {
public:
    typedef AcTrie<CharTy>                              this_type;
    typedef CharTy                                      char_type;
    typedef typename std::make_signed<CharTy>::type     schar_type;
    typedef typename std::make_unsigned<CharTy>::type   uchar_type;

    typedef std::size_t         size_type;
    typedef std::uint32_t       identifier_t;

    static const identifier_t kInvalidLink = 0;
    static const identifier_t kRootLink = 1;

    static const size_type kMaxAscii = 256;

    #pragma pack(push, 1)

    struct State {
        identifier_t                        fail_link;
        std::uint32_t                       is_final;
        std::map<uchar_type, identifier_t>  next_link;
    };

    #pragma pack(pop)

    typedef State   state_type;

private:    
    std::vector<state_type> states_;

public:
    AcTrie() {
        this->createRoot();
    }

    AcTrie(size_type capacity) {
        if (capacity != 0) {
            this->states_.reserve(capacity);
        }
        this->createRoot();
    }

    virtual ~AcTrie() {}

    identifier_t max_state_id() const {
        return static_cast<identifier_t>(this->states_.size());
    }

    identifier_t last_state_id() const {
        return static_cast<identifier_t>(this->states_.size() - 1);
    }

    bool is_valid_id(identifier_t identifier) const {
        return ((identifier != kInvalidLink) && (identifier < this->max_state_id()));
    }

    size_type state_count() const {
        return this->states_.size();
    }

    const State & state(size_type index) {
        return this->states_[index];
    }

    identifier_t root() const {
        return kRootLink;
    }

    void appendPattern(const char_type * pattern, size_type length) {
        identifier_t root = this->root();
        identifier_t cur = root;
        assert(this->is_valid_id(cur));

        for (size_type i = 0; i < length; i++) {
            uchar_type label = (uchar_type)*pattern++;
            State & cur_state = this->states_[cur];
            auto iter = cur_state.next_link.find(label);
            if (likely(iter == cur_state.next_link.end())) {
                identifier_t next = this->max_state_id();
                cur_state.next_link.insert(std::make_pair(label, next));

                State next_state;
                next_state.fail_link = kInvalidLink;
                next_state.is_final = 0;
                this->states_.push_back(next_state);

                assert(next == this->last_state_id());
                cur = next;
                assert(this->is_valid_id(cur));
            }
            else {
                assert(label == iter->first);
                cur = iter->second;
                assert(this->is_valid_id(cur));
            }
        }

        // Setting the leaf state
        assert(this->is_valid_id(cur));
        State & leaf_state = this->states_[cur];
        leaf_state.is_final = 1;
    }

    void build() {
        std::vector<identifier_t> queue;

        identifier_t root = this->root();
        queue.push_back(root);

        size_type head = 0;
        while (likely(head < queue.size())) {
            identifier_t cur = queue[head++];
            State & cur_state = this->states_[cur];
            for (auto iter = cur_state.next_link.begin();
                iter != cur_state.next_link.end(); ++iter) {
                uchar_type label = iter->first;
                identifier_t next = iter->second;
                State & next_state = this->states_[next];
                assert(this->is_valid_id(next));
                if (likely(cur == root)) {
                    next_state.fail_link = root;
                    queue.push_back(next);
                }
                else {
                    identifier_t node = cur_state.fail_link;
                    do {
                        if (likely(node != kInvalidLink)) {
                            State & node_state = this->states_[node];
                            auto node_iter = node_state.next_link.find(label);
                            if (likely(node_iter == node_state.next_link.end())) {
                                // node = node->fail;
                                node = node_state.fail_link;
                            }
                            else {
                                // next->fail = node->next[i];
                                next_state.fail_link = node_iter->second;
                                break;
                            }
                        }
                        else {
                            // next->fail = root;
                            next_state.fail_link = root;
                            break;
                        }
                    } while (1);
                    queue.push_back(next);
                }
            }
        }
    }

private:
    void createRoot() {
        assert(this->states_.size() == 0);

        // Append dummy state for invalid link, Identifier = 0
        State dummy;
        dummy.fail_link = kInvalidLink;
        dummy.is_final = 0;
        this->states_.push_back(std::move(dummy));

        // Append Root state, Identifier = 1
        State root;
        root.fail_link = kInvalidLink;
        root.is_final = 0;
        this->states_.push_back(std::move(root));
    }
};

} // namespace darts_bench

#endif // AHO_CORASICK_AUTO_H
