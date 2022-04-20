
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
// See: https://zhuanlan.zhihu.com/p/368184958 (KMP, Trie, DFA, AC-Auto, very clear)
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
    static const std::uint32_t kPatternIdMask = 0x7FFFFFFFu;
    static const std::uint32_t kIsFinalMask = 0x80000000u;

    #pragma pack(push, 1)

    struct State {
        typedef std::map<std::uint32_t, identifier_t> map_type;

        identifier_t                            fail_link;
        union {
            struct {
                std::uint32_t                   pattern_id : 31;
                std::uint32_t                   is_final   : 1;
            };
            std::uint32_t                       identifier;
        };
        std::map<std::uint32_t, identifier_t>   next_link;
    };

    struct MatchInfo {
        std::uint32_t last;
        std::uint32_t pattern_id;
    };

    struct MatchInfo2 {
        std::uint32_t first;
        std::uint32_t last;
        std::uint32_t pattern_id;
        std::uint32_t reserve;
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

    void appendPattern(const char_type * pattern, size_type length, std::uint32_t id) {
        identifier_t root = this->root();
        identifier_t cur = root;
        assert(this->is_valid_id(cur));

        for (size_type i = 0; i < length; i++) {
            std::uint32_t label = (uchar_type)*pattern++;
            State & cur_state = this->states_[cur];
            auto iter = cur_state.next_link.find(label);
            if (likely(iter == cur_state.next_link.end())) {
                identifier_t next = this->max_state_id();
                cur_state.next_link.insert(std::make_pair(label, next));

                State next_state;
                next_state.fail_link = kInvalidLink;
                next_state.identifier = 0;
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
        //leaf_state.pattern_id = id & kPatternIdMask;
        //leaf_state.is_final = 1;
        leaf_state.identifier = (id & kPatternIdMask) | kIsFinalMask;
    }

    void build() {
        std::vector<identifier_t> queue;
        queue.reserve(this->states_.size());

        identifier_t root = this->root();
        queue.push_back(root);

        size_type head = 0;
        while (likely(head < queue.size())) {
            identifier_t cur = queue[head++];
            State & cur_state = this->states_[cur];
            for (auto iter = cur_state.next_link.begin();
                iter != cur_state.next_link.end(); ++iter) {
                std::uint32_t label = iter->first;
                identifier_t next = iter->second;
                State & next_state = this->states_[next];
                assert(this->is_valid_id(next));
                if (likely(cur != root)) {
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
                }
                else {
                    next_state.fail_link = root;
                }
                queue.push_back(next);
            }
        }
    }

    template <typename T>
    inline
    bool search_suffix(identifier_t root, const T * first, const T * last, MatchInfo & matchInfo) {
        bool matched = false;
        uchar_type * text_first = (uchar_type *)first;
        uchar_type * text_last = (uchar_type *)last;
        uchar_type * text = text_first;
        assert(text_first <= text_last);

        identifier_t cur = root;
        while (text < text_last) {
            std::uint32_t lable = (std::uint32_t)*text;
            assert(this->is_valid_id(cur));
            State & cur_state = this->states_[cur];
            auto iter = cur_state.next_link.find(lable);
            if (iter != cur_state.next_link.end()) {
                cur = iter->second;
                text++;
            } else {
                if ((cur != root) && (cur_state.is_final != 0)) {
                    matchInfo.last       = (std::uint32_t)(text - text_first);
                    matchInfo.pattern_id = cur_state.pattern_id;
                    return true;
                }
                break;
            }
        }

        return matched;
    }

    //
    // See: https://zhuanlan.zhihu.com/p/80325757 (The picture of the article is good, and the code is concise and clear.)
    // See: https://juejin.cn/post/6844903635130777614 (The DFA diagram drawing is nice.)
    // See: https://zhuanlan.zhihu.com/p/191644920 (The code is a little similar to Article 1.)
    //
    template <typename T>
    bool search(const T * first, const T * last, MatchInfo & matchInfo) {
        bool matched = false;
        uchar_type * text_first = (uchar_type *)first;
        uchar_type * text_last = (uchar_type *)last;
        uchar_type * text = text_first;
        assert(text_first <= text_last);

        typedef typename State::map_type::const_iterator const_iterator;

        identifier_t cur = this->root();

        while (text < text_last) {
            const_iterator iter;
            bool hasChildren = false;
            std::uint32_t lable = (std::uint32_t)*text;
            do {
                assert(this->is_valid_id(cur));
                State & cur_state = this->states_[cur];
                iter = cur_state.next_link.find(lable);
                hasChildren = (iter != cur_state.next_link.end());
                if (!hasChildren && (cur != this->root())) {
                    cur = cur_state.fail_link;
                } else {
                    break;
                }
            } while (1);

            if (!hasChildren) {
                cur = this->root();
                text++;
            } else {
                cur = iter->second;

                identifier_t tmp = cur;
                do {
                    State & tmp_state = this->states_[tmp];
                    if (tmp_state.is_final != 0) {
                        // Matched
                        matchInfo.last       = (std::uint32_t)(text + 1 - text_first);
                        matchInfo.pattern_id = tmp_state.pattern_id;
                        if (tmp != cur) {
                            // If current full prefix is matched, judge the continous suffixs has some chars is matched?
                            // If it's have any chars is matched, it would be the longest matched suffix.
                            MatchInfo matchInfo1;
                            bool matched1 = this->search_suffix(cur, text + 1, text_last, matchInfo1);
                            if (matched1) {
                                matchInfo.last      += matchInfo1.last;
                                matchInfo.pattern_id = matchInfo1.pattern_id;
                                return true;
                            }
                        }
                        if (tmp_state.next_link.size() != 0) {
                            // If a sub suffix exists, match the continous longest suffixs.
                            MatchInfo matchInfo2;
                            bool matched2 = this->search_suffix(tmp, text + 1, text_last, matchInfo2);
                            if (matched2) {
                                matchInfo.last      += matchInfo2.last;
                                matchInfo.pattern_id = matchInfo2.pattern_id;
                            }
                        }
                        return true;
                    }
                    tmp = tmp_state.fail_link;
                } while (tmp != this->root());
                text++;
            }
        }

        return matched;
    }

    template <typename T>
    bool search2(const T * first, const T * last, MatchInfo2 & matchInfo) {
        bool matched = false;
        uchar_type * text_first = (uchar_type *)first;
        uchar_type * text_last = (uchar_type *)last;
        uchar_type * text = text_first;
        uchar_type * match_first = text_first;
        assert(text_first <= text_last);

        identifier_t cur = this->root();
        while (text < text_last) {
            assert(this->is_valid_id(cur));
            State & cur_state = this->states_[cur];
            std::uint32_t lable = (std::uint32_t)*text;
            auto const & iter = cur_state.next_link.find(lable);
            if (iter == cur_state.next_link.end()) {
                if (cur_state.is_final == 0) {
                    identifier_t fail_link = cur_state.fail_link;
                    if (fail_link == kInvalidLink) {
                        // cur = root
                        cur = this->root();
                        text++;
                        match_first = text;
                        //break;
                    } else {
                        // cur = cur.fail
                        cur = fail_link;
                        State & cur_fail = this->states_[cur];
                        if (cur_fail.is_final == 0) {
                            if (cur == this->root()) {
                                text++;
                                match_first = text;
                            }
                        } else {
                            matchInfo.first = (std::uint32_t)(match_first - text_first);
                            matchInfo.last = (std::uint32_t)(text - 1 - text_first);
                            matchInfo.pattern_id = cur_fail.pattern_id;
                            matchInfo.reserve = 0;
                            return true;
                        }
                    }
                } else {
                    matchInfo.first = (std::uint32_t)(match_first - text_first);
                    matchInfo.last = (std::uint32_t)(text - 1 - text_first);
                    matchInfo.pattern_id = cur_state.pattern_id;
                    matchInfo.reserve = 0;
                    return true;
                }
            } else {
                // cur = cur.next[i]
                cur = iter->second;
                text++;
            }
        }

        return matched;
    }

private:
    void createRoot() {
        assert(this->states_.size() == 0);

        // Append dummy state for invalid link, Identifier = 0
        State dummy;
        dummy.fail_link = kInvalidLink;
        dummy.identifier = 0;
        this->states_.push_back(std::move(dummy));

        // Append Root state, Identifier = 1
        State root;
        root.fail_link = kInvalidLink;
        root.identifier = 0;
        this->states_.push_back(std::move(root));
    }
};

} // namespace darts_bench

#endif // AHO_CORASICK_AUTO_H
