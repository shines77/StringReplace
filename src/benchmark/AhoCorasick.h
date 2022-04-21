
#ifndef AHO_CORASICK_AUTO_V1_H
#define AHO_CORASICK_AUTO_V1_H

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

namespace v1 {

//
// See: https://zhuanlan.zhihu.com/p/368184958 (KMP, Trie, DFA, AC-Auto, very clear)
//
// See: https://zhuanlan.zhihu.com/p/137614862
// See: https://www.zhihu.com/question/352900751
// See: https://nark.cc/p/?p=1453
//
template <typename CharT>
class AcTrie {
public:
    typedef AcTrie<CharT>                                   this_type;
    typedef CharT                                           o_char_type;
    typedef typename detail::char_trait<CharT>::NoSigned    char_type;
    typedef typename detail::char_trait<CharT>::Signed      schar_type;
    typedef typename detail::char_trait<CharT>::Unsigned    uchar_type;

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
        std::map<std::uint32_t, identifier_t>   children;
    };

    struct MatchInfo {
        std::uint32_t last_pos;
        std::uint32_t pattern_id;
    };

    #pragma pack(pop)

    typedef State   state_type;

private:    
    std::vector<state_type> states_;

public:
    AcTrie() {
        this->create_root();
    }

    AcTrie(size_type capacity) {
        if (capacity != 0) {
            this->states_.reserve(capacity);
        }
        this->create_root();
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

    bool insert(const uchar_type * in_pattern, size_type length, std::uint32_t id) {
        const uchar_type * pattern = (const uchar_type *)in_pattern;

        identifier_t cur = this->root();
        assert(this->is_valid_id(cur));

        for (size_type i = 0; i < length; i++) {
            std::uint32_t label = (uchar_type)*pattern++;
            State & cur_state = this->states_[cur];
            auto iter = cur_state.children.find(label);
            if (likely(iter == cur_state.children.end())) {
                identifier_t next = this->max_state_id();
                cur_state.children.insert(std::make_pair(label, next));

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

    void build() {
        std::vector<identifier_t> queue;
        queue.reserve(this->states_.size());

        identifier_t root = this->root();
        queue.push_back(root);

        size_type head = 0;
        while (likely(head < queue.size())) {
            identifier_t cur = queue[head++];
            State & cur_state = this->states_[cur];
            for (auto iter = cur_state.children.begin();
                iter != cur_state.children.end(); ++iter) {
                std::uint32_t label = iter->first;
                identifier_t child = iter->second;
                State & child_state = this->states_[child];
                assert(this->is_valid_id(child));
                if (likely(cur != root)) {
                    identifier_t node = cur_state.fail_link;
                    do {
                        if (likely(node != kInvalidLink)) {
                            State & node_state = this->states_[node];
                            auto node_iter = node_state.children.find(label);
                            if (likely(node_iter == node_state.children.end())) {
                                // node = node->fail;
                                node = node_state.fail_link;
                            }
                            else {
                                // child->fail = node->next[i];
                                child_state.fail_link = node_iter->second;
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

    inline
    bool search_suffix(identifier_t root, const uchar_type * first,
                       const uchar_type * last, MatchInfo & matchInfo) {
        bool matched = false;
        uchar_type * text_first = (uchar_type *)first;
        uchar_type * text_last = (uchar_type *)last;
        uchar_type * text = text_first;
        assert(text_first <= text_last);

        identifier_t cur = root;
        while (text < text_last) {
            std::uint32_t lable = (uchar_type)*text;
            assert(this->is_valid_id(cur));
            State & cur_state = this->states_[cur];
            auto iter = cur_state.children.find(lable);
            if (iter != cur_state.children.end()) {
                cur = iter->second;
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
    bool search_suffix(identifier_t root, const char_type * first,
                       const char_type * last, MatchInfo & matchInfo) {
        return this->search_suffix(root, (const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

    inline
    bool search_suffix(identifier_t root, const schar_type * first,
                       const schar_type * last, MatchInfo & matchInfo) {
        return this->search_suffix(root, (const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

    //
    // See: https://zhuanlan.zhihu.com/p/80325757 (The picture of the article is good, and the code is concise and clear.)
    // See: https://juejin.cn/post/6844903635130777614 (The DFA diagram drawing is nice.)
    // See: https://zhuanlan.zhihu.com/p/191644920 (The code is a little similar to Article 1.)
    //
    bool search(const uchar_type * first, const uchar_type * last, MatchInfo & matchInfo) {
        uchar_type * text_first = (uchar_type *)first;
        uchar_type * text_last = (uchar_type *)last;
        uchar_type * text = text_first;
        assert(text_first <= text_last);

        typedef typename State::map_type::const_iterator const_iterator;

        identifier_t cur = this->root();
        bool matched = false;

        while (text < text_last) {
            const_iterator iter;
            bool hasChildren = false;
            std::uint32_t lable = (uchar_type)*text;
            do {
                assert(this->is_valid_id(cur));
                State & cur_state = this->states_[cur];
                iter = cur_state.children.find(lable);
                hasChildren = (iter != cur_state.children.end());
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

                identifier_t node = cur;
                do {
                    State & node_state = this->states_[node];
                    if (node_state.is_final != 0) {
                        // Matched
                        matchInfo.last_pos   = (std::uint32_t)(text + 1 - text_first);
                        matchInfo.pattern_id = node_state.pattern_id;
                        if (node != cur) {
                            // If current full prefix is matched, judge the continous suffixs has some chars is matched?
                            // If it's have any chars is matched, it would be the longest matched suffix.
                            MatchInfo matchInfo1;
                            bool matched1 = this->search_suffix(cur, text + 1, text_last, matchInfo1);
                            if (matched1) {
                                matchInfo.last_pos  += matchInfo1.last_pos;
                                matchInfo.pattern_id = matchInfo1.pattern_id;
                                return true;
                            }
                        }
                        if (node_state.children.size() != 0) {
                            // If a sub suffix exists, match the continous longest suffixs.
                            MatchInfo matchInfo2;
                            bool matched2 = this->search_suffix(node, text + 1, text_last, matchInfo2);
                            if (matched2) {
                                matchInfo.last_pos  += matchInfo2.last_pos;
                                matchInfo.pattern_id = matchInfo2.pattern_id;
                            }
                        }
                        return true;
                    }
                    node = node_state.fail_link;
                } while (node != this->root());
                text++;
            }
        }

        return matched;
    }

    bool search(const char_type * first, const char_type * last, MatchInfo & matchInfo) {
        return this->search((const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

    bool search(const schar_type * first, const schar_type * last, MatchInfo & matchInfo) {
        return this->search((const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

private:
    void create_root() {
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

} // namespace v1

#endif // AHO_CORASICK_AUTO_V1_H
