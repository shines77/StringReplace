
#ifndef DOUBLE_ARRAY_TRIE_H
#define DOUBLE_ARRAY_TRIE_H

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
#include "AhoCorasick_v1.h"
#include "AhoCorasick_v2.h"

namespace darts_bench {

//
// See: https://zhuanlan.zhihu.com/p/368184958 (KMP, Trie, DFA, AC-Auto, very clear)
//
// See: https://zhuanlan.zhihu.com/p/137614862
// See: https://www.zhihu.com/question/352900751
// See: https://nark.cc/p/?p=1453
//
template <typename CharT>
class Darts {
public:
    typedef Darts<CharT>                                    this_type;
    typedef typename v1::AcTrie<CharT>                      AcTireT;
    typedef CharT                                           o_char_type;
    typedef typename ::detail::char_trait<CharT>::NoSigned  char_type;
    typedef typename ::detail::char_trait<CharT>::Signed    schar_type;
    typedef typename ::detail::char_trait<CharT>::Unsigned  uchar_type;

    typedef std::size_t     size_type;
    typedef std::uint32_t   ident_t;

    static const ident_t kInvalidLink = 0;
    static const ident_t kRootLink = 1;

    static const size_type kMaxAscii = 256;

    static const std::uint32_t kPatternIdMask = 0x7FFFFFFFu;
    static const std::uint32_t kHasChildMask = 0x40000000u;
    static const std::uint32_t kIsFinalMask = 0x80000000u;
    static const std::uint32_t kIsFreeMask = 0x80000000u;

    #pragma pack(push, 1)

    struct State {
        ident_t                 base;
        ident_t                 check;
        ident_t                 fail_link;
        union {
            std::uint32_t       identifier;
            struct {
                std::uint32_t   pattern_id : 30;
                std::uint32_t   has_child  : 1;
                std::uint32_t   is_final   : 1;
            };
        };
    };

    struct MatchInfo {
        std::uint32_t last_pos;
        std::uint32_t pattern_id;
    };

    #pragma pack(pop)

    typedef State state_type;

private:
    std::vector<state_type> states_;
    AcTireT acTrie_;

public:
    Darts() {
        this->create_root();
    }

    Darts(size_type capacity) {
        if (capacity != 0) {
            this->states_.reserve(capacity);
        }
        this->create_root();
    }

    virtual ~Darts() {}

    ident_t max_state_id() const {
        return static_cast<ident_t>(this->states_.size());
    }

    ident_t last_state_id() const {
        return static_cast<ident_t>(this->states_.size() - 1);
    }

    bool is_valid_id(ident_t identifier) const {
        return ((identifier != kInvalidLink) && (identifier < this->max_state_id()));
    }

    size_type state_count() const {
        return this->states_.size();
    }

    const State & state(size_type index) {
        return this->states_[index];
    }

    ident_t root() const {
        return kRootLink;
    }

    void clear() {
        this->acTrie_.clear();

        this->states_.clear();
        this->states_.reserve(2);
        this->create_root();
    }

    bool insert(const uchar_type * in_pattern, size_type length, std::uint32_t id) {
        const uchar_type * pattern = (const uchar_type *)in_pattern;

        ident_t cur = this->root();
        assert(this->is_valid_id(cur));

        for (size_type i = 0; i < length; i++) {
            std::uint32_t label = (uchar_type)*pattern++;
            State & cur_state = this->states_[cur];
            ident_t base = cur_state.base;
            ident_t child = base + label;
            State & child_state = this->states_[child];
            if (likely(child_state.check != base)) {
                ident_t child = this->max_state_id();

                State child_state;
                child_state.base = 0;
                child_state.check = 0;
                child_state.fail_link = kInvalidLink;
                child_state.identifier = 0;
                this->states_.push_back(child_state);

                assert(child == this->last_state_id());
                cur = child;
                assert(this->is_valid_id(cur));
            }
            else {
                cur = child;
                assert(this->is_valid_id(cur));
            }
        }

        // Setting the leaf state
        assert(this->is_valid_id(cur));
        State & leaf_state = this->states_[cur];
        if (leaf_state.is_final == 0) {
            //leaf_state.pattern_id = id & kPatternIdMask;
            //leaf_state.is_final = 1;
            leaf_state.identifier = (id & kPatternIdMask) | kIsFinalMask;
            return true;
        } else {
            return false;
        }
    }

    bool insert(const char_type * pattern, size_type length, std::uint32_t id) {
        return this->insert((const uchar_type *)pattern, length, id);
    }

    bool insert(const schar_type * pattern, size_type length, std::uint32_t id) {
        return this->insert((const uchar_type *)pattern, length, id);
    }

    bool insert(const std::string & pattern, std::uint32_t id) {
        return this->insert(pattern.c_str(), pattern.size(), id);
    }

    void insert_all(const std::vector<std::string> & patterns) {
        this->clear();
        std::uint32_t index = 0;
        for (auto iter = patterns.begin(); iter != patterns.end(); ++iter) {
            const std::string & pattern = *iter;
            this->insert(pattern.c_str(), pattern.size(), index);
            index++;
        }
    }

    void build() {
        std::vector<ident_t> queue;
        queue.reserve(this->states_.size());

        ident_t root = this->root();
        queue.push_back(root);

        size_type head = 0;
        while (likely(head < queue.size())) {
            ident_t cur = queue[head++];
            State & cur_state = this->states_[cur];
            ident_t base = cur_state.base;
            for (ident_t label = 0; label < kMaxAscii; label++) {
                ident_t child = base + label;
                assert(this->is_valid_id(child));
                State & child_state = this->states_[child];
                if (likely(child_state.check == base)) {
                    if (likely(cur != root)) {
                        ident_t node = cur_state.fail_link;
                        do {
                            if (likely(node != kInvalidLink)) {
                                State & node_state = this->states_[node];
                                ident_t node_base = node_state.base;
                                ident_t node_child = node_base + label;
                                assert(this->is_valid_id(node_child));
                                State & node_child_state = this->states_[node_child];
                                if (likely(node_child_state.check != base)) {
                                    // node = node->fail;
                                    node = node_state.fail_link;
                                }
                                else {
                                    // child->fail = node->children[i];
                                    child_state.fail_link = node_child;
                                    break;
                                }
                            }
                            else {
                                // child->fail = root;
                                child_state.fail_link = root;
                                break;
                            }
                        } while (1);
                    }
                    else {
                        child_state.fail_link = root;
                    }
                    queue.push_back(child);
                }
            }
        }
    }

    inline
    bool match_tail(ident_t root, const uchar_type * first,
                    const uchar_type * last, MatchInfo & matchInfo) {
        uchar_type * text_first = (uchar_type *)first;
        uchar_type * text_last = (uchar_type *)last;
        uchar_type * text = text_first;
        assert(text_first <= text_last);

        bool matched = false;

        ident_t cur = root;
        while (text < text_last) {
            std::uint32_t label = (uchar_type)*text;
            assert(this->is_valid_id(cur));
            State & cur_state = this->states_[cur];
            ident_t base = cur_state.base;
            ident_t child = base + label;
            State & child_state = this->states_[child];
            if (likely(child_state.check == base)) {
                cur = child;
                text++;
            } else {
                if ((cur != root) && (cur_state.is_final != 0)) {
                    matchInfo.last_pos   = (std::uint32_t)(text - text_first);
                    matchInfo.pattern_id = cur_state.pattern_id;
                    return true;
                }
                break;
            }
        }

        return matched;
    }

    inline
    bool match_tail(ident_t root, const char_type * first,
                    const char_type * last, MatchInfo & matchInfo) {
        return this->match_tail(root, (const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

    inline
    bool match_tail(ident_t root, const schar_type * first,
                    const schar_type * last, MatchInfo & matchInfo) {
        return this->match_tail(root, (const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

    //
    // See: https://zhuanlan.zhihu.com/p/80325757 (The picture of the article is good, and the code is concise and clear.)
    // See: https://juejin.cn/post/6844903635130777614 (The DFA diagram drawing is nice.)
    // See: https://zhuanlan.zhihu.com/p/191644920 (The code is a little similar to Article 1.)
    //
    bool match_one(const uchar_type * first, const uchar_type * last, MatchInfo & matchInfo) {
        uchar_type * text_first = (uchar_type *)first;
        uchar_type * text_last = (uchar_type *)last;
        uchar_type * text = text_first;
        assert(text_first <= text_last);

        ident_t root = this->root();
        ident_t cur = root;
        bool matched = false;

        while (text < text_last) {
            ident_t node;
            std::uint32_t label = (uchar_type)*text;
            if (likely(cur == root)) {
                assert(this->is_valid_id(cur));
                State & cur_state = this->states_[cur];
                ident_t base = cur_state.base;
                ident_t child = base + label;
                State & child_state = this->states_[child];
                if (likely(child_state.check == base)) {
                    cur = child;
                } else {
                    goto MatchNextLabel;
                }
            } else {
                do {
                    assert(this->is_valid_id(cur));
                    State & cur_state = this->states_[cur];
                    ident_t base = cur_state.base;
                    ident_t child = base + label;
                    State & child_state = this->states_[child];
                    if (likely(child_state.check != base)) {
                        if (likely(cur != root)) {
                            cur = cur_state.fail_link;
                        } else {
                            goto MatchNextLabel;
                        }
                    } else {
                        cur = child;
                        break;
                    }
                } while (1);
            }

            node = cur;
            assert(node != root);

            do {
                State & node_state = this->states_[node];
                if (unlikely(node_state.is_final != 0)) {
                    // Matched
                    matchInfo.last_pos   = (std::uint32_t)(text + 1 - text_first);
                    matchInfo.pattern_id = node_state.pattern_id;
                    if (node != cur) {
                        // If current full prefix is matched, judge the continous suffixs has some chars is matched?
                        // If it's have any chars is matched, it would be the longest matched suffix.
                        MatchInfo matchInfo1;
                        bool matched1 = this->match_tail(cur, text + 1, text_last, matchInfo1);
                        if (matched1) {
                            matchInfo.last_pos  += matchInfo1.last_pos;
                            matchInfo.pattern_id = matchInfo1.pattern_id;
                            return true;
                        }
                    }
                    if (node_state.has_child != 0) {
                        // If a sub suffix exists, match the continous longest suffixs.
                        MatchInfo matchInfo2;
                        bool matched2 = this->match_tail(node, text + 1, text_last, matchInfo2);
                        if (matched2) {
                            matchInfo.last_pos  += matchInfo2.last_pos;
                            matchInfo.pattern_id = matchInfo2.pattern_id;
                        }
                    }
                    return true;
                }
                node = node_state.fail_link;
            } while (node != root);

MatchNextLabel:
            text++;
        }

        return matched;
    }

    bool match_one(const char_type * first, const char_type * last, MatchInfo & matchInfo) {
        return this->match_one((const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

    bool match_one(const schar_type * first, const schar_type * last, MatchInfo & matchInfo) {
        return this->match_one((const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

private:
    void create_root() {
        assert(this->states_.size() == 0);

        // Append dummy state for invalid link, Identifier = 0
        State dummy;
        dummy.base = 0;
        dummy.check = 0;
        dummy.fail_link = kInvalidLink;
        dummy.identifier = 0;
        this->states_.push_back(std::move(dummy));

        // Append root state, Identifier = 1
        State root;
        root.base = 0;
        root.check = 0;
        root.fail_link = kInvalidLink;
        root.identifier = 0;
        this->states_.push_back(std::move(root));
    }
};

} // namespace darts_bench

#endif // DOUBLE_ARRAY_TRIE_H
