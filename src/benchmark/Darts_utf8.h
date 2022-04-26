
#ifndef DOUBLE_ARRAY_TRIE_UTF8_H
#define DOUBLE_ARRAY_TRIE_UTF8_H

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
#include <unordered_map>
#include <functional>
#include <utility>
#include <algorithm>
#include <type_traits>

#include "benchmark.h"
#include "win_iconv.h"
#include "utf8_utils.h"
#include "AcTrie_utf8.h"

namespace utf8 {

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
    typedef CharT                                           o_char_type;
    typedef typename ::detail::char_trait<CharT>::NoSigned  char_type;
    typedef typename ::detail::char_trait<CharT>::Signed    schar_type;
    typedef typename ::detail::char_trait<CharT>::Unsigned  uchar_type;

    typedef typename utf8::AcTrie<CharT>                    AcTireT;
    typedef typename AcTireT::State                         AcState;
    typedef typename AcTireT::size_type                     size_type;
    typedef typename AcTireT::ident_t                       ident_t;

    typedef std::function<std::size_t (std::uint32_t)>      on_hit_callback;

    static const ident_t kInvalidIdent = 0;
    static const ident_t kRootIdent = 1;
    static const ident_t kFirstFreeIdent = 2;

    static const std::uint32_t kMaxAscii = 65536;   // 0x0000FFFFu
    static const std::uint32_t kOverFlowLable = kMaxAscii;
    static const std::uint32_t kMaxLabel = 0x0010FFFFu;

    static const std::uint32_t kPatternIdMask = 0x7FFFFFFFu;
    static const std::uint32_t kHasChildMask = 0x40000000u;
    static const std::uint32_t kIsFinalMask = 0x80000000u;
    static const std::uint32_t kIsFreeMask = 0x80000000u;

    static const std::uint32_t kSignMask = 0x80000000u;

    #pragma pack(push, 1)

    struct State {
        union {
            std::uint64_t       is_free;

            struct {
              ident_t           base;
              ident_t           check;
            };
        };
        union {
            std::uint64_t       extend;

            struct {
              ident_t           fail_link;
              union {
                std::uint32_t   identifier;
                struct {
                  std::uint32_t pattern_id : 30;
                  std::uint32_t has_child  : 1;
                  std::uint32_t is_final   : 1;
                };
              };
            };
        };
#if 1
        State() noexcept : is_free(0), extend(0) {
        }
#else
        State() noexcept : base(0), check(0), fail_link(kInvalidIdent), identifier(0) {
        }
#endif
    };

    struct MatchInfo {
        std::uint32_t end;
        std::uint32_t pattern_id;
    };

    struct MatchInfoEx {
        std::uint32_t begin;
        std::uint32_t end;
        std::uint32_t pattern_id;
        std::uint32_t reserve;
    };

    #pragma pack(pop)

    typedef State state_type;

private:
    std::vector<state_type> states_;
    std::unordered_map<std::uint64_t, std::uint32_t> overflow_labels_;

    ident_t first_free_id_;
    ident_t last_free_id_;
    AcTireT acTrie_;

public:
    Darts() : first_free_id_(kFirstFreeIdent), last_free_id_(kFirstFreeIdent) {
        this->create_root();
    }

    Darts(size_type capacity) : first_free_id_(kFirstFreeIdent), last_free_id_(kFirstFreeIdent) {
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

#ifdef _DEBUG

    bool is_valid_id(ident_t identifier) const {
        return ((identifier != kInvalidIdent) && (identifier < this->max_state_id()));
    }

    bool is_valid_base(ident_t identifier) const {
        return ((identifier > kRootIdent) && (identifier < this->max_state_id()) && ((identifier & kSignMask) == 0));
    }

    bool is_valid_child(ident_t identifier) const {
        return ((identifier > kRootIdent) && (identifier < this->max_state_id()));
    }

    bool is_free_child(ident_t identifier) const {
        return ((identifier > kRootIdent) && (identifier < this->max_state_id()) && this->is_free_state(identifier));
    }

    bool is_allocated_child(ident_t identifier) const {
        return ((identifier > kRootIdent) && (identifier < this->max_state_id()) && !this->is_free_state(identifier));
    }

#else

    bool is_valid_id(ident_t identifier) const {
        return (identifier != kInvalidIdent);
    }

    bool is_valid_base(ident_t identifier) const {
        return ((identifier > kRootIdent) && ((identifier & kSignMask) == 0));
    }

    bool is_valid_child(ident_t identifier) const {
        return (identifier > kRootIdent);
    }

    bool is_free_child(ident_t identifier) const {
        return ((identifier > kRootIdent) && this->is_free_state(identifier));
    }

    bool is_allocated_child(ident_t identifier) const {
        return ((identifier > kRootIdent) && !this->is_free_state(identifier));
    }

#endif // _DEBUG

    size_type size() const {
        return this->states_.size();
    }

    State & states(size_type index) {
        return this->states_[index];
    }

    const State & states(size_type index) const {
        return this->states_[index];
    }

    inline bool is_free_state(ident_t index) const {
        const State & state = this->states_[index];
        return (state.is_free == 0);
    }

    ident_t first_free_id() const {
        return this->first_free_id_;
    }

    void set_first_free_id(ident_t next) {
        this->first_free_id_ = next;
    }

    ident_t last_free_id() const {
        return this->last_free_id_;
    }

    void set_last_free_id(ident_t last) {
        this->last_free_id_ = last;
    }

    ident_t root() const {
        return kRootIdent;
    }

    bool has_overflow_labels() const {
        return this->acTrie_.has_overflow_labels();
    }

    void clear() {
        this->clear_ac_trie();
        this->clear_trie();
    }

    void clear_ac_trie() {
        this->acTrie_.clear();
    }

    void clear_trie(size_type capacity = 2) {
        capacity = (capacity < 2) ? 2: capacity;

        this->overflow_labels_.clear();
        this->states_.clear();
        this->states_.reserve(capacity);
        this->create_root();
    }

    bool insert(const uchar_type * pattern, size_type length, std::uint32_t id) {
        return this->acTrie_.insert(pattern, length, id);
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

    inline ident_t find_first_free_state() {
        ident_t first = this->first_free_id();
        ident_t next = this->find_next_free_state(first);
        if (next >= kFirstFreeIdent) {
            this->set_first_free_id(next);
        }
        return next;
    }

    inline ident_t find_next_free_state(ident_t first) {
        for (ident_t cur = first; cur < this->max_state_id(); cur++) {
            if (this->is_free_state(cur)) {
                return cur;
            }
        }
        return kInvalidIdent;
    }

    bool build() {
        if (!this->has_overflow_labels())
            return this->build_no_overflow();
        else
            return this->build_overflow();
    }

    bool build_no_overflow() {
        std::vector<ident_t> ac_queue;
        std::vector<ident_t> queue;
        ac_queue.reserve(this->acTrie_.size());
        queue.reserve(this->acTrie_.size());

        size_type state_capacity = (size_type)((double)this->acTrie_.size() * 1.1);
        if (state_capacity < (kFirstFreeIdent + kMaxAscii))
            state_capacity = (kFirstFreeIdent + kMaxAscii) + 1024;
        this->clear_trie(state_capacity);
        this->states_.resize(this->acTrie_.size());

        ident_t root_ac = this->acTrie_.root();
        ac_queue.push_back(root_ac);

        ident_t root = this->root();
        queue.push_back(root);

        bool has_overflow_labels = false;

        size_type head = 0;
        size_type first_children = 0;
        while (likely(head < ac_queue.size())) {
            ident_t cur_ac = ac_queue[head];
            AcState & cur_ac_state = this->acTrie_.states(cur_ac);

            ident_t cur = queue[head++];
            State * cur_state = &this->states_[cur];

            std::size_t nums_child = cur_ac_state.children.size();
            cur_state->identifier = cur_ac_state.identifier;
            //cur_state->pattern_id = cur_ac_state.pattern_id;
            //cur_state->is_final = cur_ac_state.is_final;
            cur_state->has_child = (nums_child != 0) ? 1 : 0;

            if (nums_child > 0) {
                if (head == 1) {
                    first_children = nums_child;
                }
                // Find [min, max] label
                std::uint32_t min_label = kMaxAscii - 1;
                std::uint32_t max_label = 0;
                for (auto iter = cur_ac_state.children.begin();
                    iter != cur_ac_state.children.end(); ++iter) {
                    std::uint32_t label = iter->first;
                    if (label < min_label) {
                        min_label = label;
                    }
                    if (label > max_label) {
                        max_label = label;
                    }
                }

                // Search base value
                bool base_found = false;
                ident_t first_free = this->find_first_free_state();
                ident_t base = first_free - min_label;
                if (!this->is_valid_base(base)) {
                    first_free = kFirstFreeIdent + min_label;
                    first_free = this->find_next_free_state(first_free);
                }
                do {
                    if (first_free != kInvalidIdent) {
                        bool search_next_base = false;
                        base = first_free - min_label;
                        for (auto iter = cur_ac_state.children.begin();
                            iter != cur_ac_state.children.end(); ++iter) {
                            std::uint32_t label = iter->first;
                            assert(label < kOverFlowLable);
                            ident_t child = base + label;
                            if (child < this->states_.size()) {
                                if (!this->is_free_state(child)) {
                                    first_free = this->find_next_free_state(first_free + 1);
                                    if ((first_children != size_type(-1)) && (head > first_children + 1)) {
                                        this->set_first_free_id(first_free);
                                        first_children = size_type(-1);
                                    }
                                    search_next_base = true;
                                    break;
                                }
                            } else {
                                std::uint32_t label_size = max_label - min_label + 1;
                                size_type newCapacity = base + kMaxAscii;
                                this->states_.resize(newCapacity);
                                cur_state = &this->states_[cur];
                                break;
                            }
                        }
                        base_found = !search_next_base;
                    } else {
                        base = (ident_t)this->states_.size() - min_label;
                        std::uint32_t label_size = max_label - min_label + 1;
                        size_type newCapacity = this->states_.size() + kMaxAscii;
                        this->states_.resize(newCapacity);
                        cur_state = &this->states_[cur];
                        base_found = true;
                        break;
                    }
                } while (!base_found);

                assert(base_found);
                cur_state->base = base;

                // Travel all children
                for (auto iter = cur_ac_state.children.begin();
                    iter != cur_ac_state.children.end(); ++iter) {
                    std::uint32_t label = iter->first;

                    ident_t child_ac = iter->second;
                    AcState & child_ac_state = this->acTrie_.states(child_ac);
                    assert(this->acTrie_.is_valid_id(child_ac));

                    if (likely(cur_ac != root_ac)) {
                        ident_t node_ac = cur_ac_state.fail_link;
                        do {
                            if (likely(node_ac != kInvalidIdent)) {
                                AcState & node_ac_state = this->acTrie_.states(node_ac);
                                auto node_iter = node_ac_state.children.find(label);
                                if (likely(node_iter == node_ac_state.children.end())) {
                                    // node = node->fail;
                                    node_ac = node_ac_state.fail_link;
                                }
                                else {
                                    // child->fail = node->children[i];
                                    child_ac_state.fail_link = node_iter->second;
                                    assert(child_ac != node_iter->second);
                                    break;
                                }
                            }
                            else {
                                // child->fail = root;
                                child_ac_state.fail_link = root_ac;
                                break;
                            }
                        } while (1);
                    }
                    else {
                        child_ac_state.fail_link = root_ac;
                    }

                    ident_t child = base + label;
                    assert(this->is_valid_child(child));
                    assert(this->is_free_state(child));

                    State & child_state = this->states_[child];
                    child_state.check = cur;
                    child_state.identifier = child_ac_state.identifier;
                    //child_state.is_final = child_ac_state.is_final;
                    //child_state.pattern_id = child_ac_state.pattern_id;
                    child_state.has_child = (child_ac_state.children.size() != 0) ? 1 : 0;

                    if (likely(cur != root)) {
                        ident_t node = cur_state->fail_link;
                        do {
                            if (likely(node != kInvalidIdent)) {
                                assert(this->is_valid_id(node));
                                State & node_state = this->states_[node];
                                ident_t node_child = node_state.base + label;
                                if (likely(node_child != kInvalidIdent) && this->is_valid_child(node_child)) {
                                    State & node_child_state = this->states_[node_child];
                                    if (likely(node_child_state.check != node || this->is_free_child(node_child))) {
                                        // node = node->fail;
                                        node = node_state.fail_link;
                                    }
                                    else {
                                        // child->fail = node->children[i];
                                        assert(child != node_child);
                                        child_state.fail_link = node_child;
                                        break;
                                    }
                                } else {
                                    // child->fail = root;
                                    child_state.fail_link = root;
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

                    ac_queue.push_back(child_ac);
                    queue.push_back(child);
                }
            } else {
                assert(cur_state->base == 0);
            }
        }

        return has_overflow_labels;
    }

    bool build_overflow() {
        std::vector<ident_t> ac_queue;
        std::vector<ident_t> queue;
        ac_queue.reserve(this->acTrie_.size());
        queue.reserve(this->acTrie_.size());

        size_type state_capacity = (size_type)((double)this->acTrie_.size() * 1.1);
        if (state_capacity < (kFirstFreeIdent + kMaxAscii))
            state_capacity = (kFirstFreeIdent + kMaxAscii) + 1024;
        this->clear_trie(state_capacity);
        this->states_.resize(this->acTrie_.size());

        ident_t root_ac = this->acTrie_.root();
        ac_queue.push_back(root_ac);

        ident_t root = this->root();
        queue.push_back(root);

        bool has_overflow_labels = false;

        size_type head = 0;
        size_type first_children = 0;
        while (likely(head < ac_queue.size())) {
            ident_t cur_ac = ac_queue[head];
            AcState & cur_ac_state = this->acTrie_.states(cur_ac);

            ident_t cur = queue[head++];
            State * cur_state = &this->states_[cur];

            std::size_t nums_child = cur_ac_state.children.size();
            cur_state->identifier = cur_ac_state.identifier;
            //cur_state->pattern_id = cur_ac_state.pattern_id;
            //cur_state->is_final = cur_ac_state.is_final;
            cur_state->has_child = (nums_child != 0) ? 1 : 0;

            if (nums_child > 0) {
                if (head == 1) {
                    first_children = nums_child;
                }
                // Find [min, max] label
                std::uint32_t min_label = kMaxAscii - 1;
                std::uint32_t max_label = 0;
                for (auto iter = cur_ac_state.children.begin();
                    iter != cur_ac_state.children.end(); ++iter) {
                    std::uint32_t label = iter->first;
                    if (label < min_label) {
                        min_label = label;
                    }
                    if (label > max_label) {
                        max_label = label;
                    }
                }

                // Search base value
                bool base_found = false;
                ident_t first_free = this->find_first_free_state();
                ident_t base = first_free - min_label;
                if (!this->is_valid_base(base)) {
                    first_free = kFirstFreeIdent + min_label;
                    first_free = this->find_next_free_state(first_free);
                }
                do {
                    if (first_free != kInvalidIdent) {
                        bool search_next_base = false;
                        base = first_free - min_label;
                        for (auto iter = cur_ac_state.children.begin();
                            iter != cur_ac_state.children.end(); ++iter) {
                            std::uint32_t label = iter->first;
                            if (label >= kOverFlowLable)
                                continue;
                            ident_t child = base + label;
                            if (child < this->states_.size()) {
                                if (!this->is_free_state(child)) {
                                    first_free = this->find_next_free_state(first_free + 1);
                                    if ((first_children != size_type(-1)) && (head > first_children + 1)) {
                                        this->set_first_free_id(first_free);
                                        first_children = size_type(-1);
                                    }
                                    search_next_base = true;
                                    break;
                                }
                            } else {
                                std::uint32_t label_size = max_label - min_label + 1;
                                size_type newCapacity = base + kMaxAscii;
                                this->states_.resize(newCapacity);
                                cur_state = &this->states_[cur];
                                break;
                            }
                        }
                        base_found = !search_next_base;
                    } else {
                        base = (ident_t)this->states_.size() - min_label;
                        std::uint32_t label_size = max_label - min_label + 1;
                        size_type newCapacity = this->states_.size() + kMaxAscii;
                        this->states_.resize(newCapacity);
                        cur_state = &this->states_[cur];
                        base_found = true;
                        break;
                    }
                } while (!base_found);

                assert(base_found);
                cur_state->base = base;

                // Travel all children
                for (auto iter = cur_ac_state.children.begin();
                    iter != cur_ac_state.children.end(); ++iter) {
                    std::uint32_t label = iter->first;

                    ident_t child_ac = iter->second;
                    AcState & child_ac_state = this->acTrie_.states(child_ac);
                    assert(this->acTrie_.is_valid_id(child_ac));

                    if (likely(cur_ac != root_ac)) {
                        ident_t node_ac = cur_ac_state.fail_link;
                        do {
                            if (likely(node_ac != kInvalidIdent)) {
                                AcState & node_ac_state = this->acTrie_.states(node_ac);
                                auto node_iter = node_ac_state.children.find(label);
                                if (likely(node_iter == node_ac_state.children.end())) {
                                    // node = node->fail;
                                    node_ac = node_ac_state.fail_link;
                                }
                                else {
                                    // child->fail = node->children[i];
                                    child_ac_state.fail_link = node_iter->second;
                                    assert(child_ac != node_iter->second);
                                    break;
                                }
                            }
                            else {
                                // child->fail = root;
                                child_ac_state.fail_link = root_ac;
                                break;
                            }
                        } while (1);
                    }
                    else {
                        child_ac_state.fail_link = root_ac;
                    }

                    ident_t child;
                    if (label < kOverFlowLable) {
                        child = base + label;
                        assert(this->is_valid_child(child));
                        assert(this->is_free_state(child));
                    } else {
                        first_free = this->find_first_free_state();
                        child = first_free;
                        std::uint64_t ident_and_label = ((std::uint64_t)cur << 32u) | label;
                        assert(this->overflow_labels_.count(ident_and_label) == 0);
                        this->overflow_labels_.insert(std::make_pair(ident_and_label, child));
                        has_overflow_labels = true;
                    }

                    State & child_state = this->states_[child];
                    child_state.check = cur;
                    child_state.identifier = child_ac_state.identifier;
                    //child_state.is_final = child_ac_state.is_final;
                    //child_state.pattern_id = child_ac_state.pattern_id;
                    child_state.has_child = (child_ac_state.children.size() != 0) ? 1 : 0;

                    if (likely(cur != root)) {
                        ident_t node = cur_state->fail_link;
                        do {
                            if (likely(node != kInvalidIdent)) {
                                assert(this->is_valid_id(node));
                                State & node_state = this->states_[node];
                                ident_t node_child = node_state.base + label;
                                if (likely(node_child != kInvalidIdent) && this->is_valid_child(node_child)) {
                                    State & node_child_state = this->states_[node_child];
                                    if (likely(node_child_state.check != node || this->is_free_child(node_child))) {
                                        // node = node->fail;
                                        node = node_state.fail_link;
                                    }
                                    else {
                                        // child->fail = node->children[i];
                                        assert(child != node_child);
                                        child_state.fail_link = node_child;
                                        break;
                                    }
                                } else {
                                    // child->fail = root;
                                    child_state.fail_link = root;
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

                    ac_queue.push_back(child_ac);
                    queue.push_back(child);
                }
            } else {
                assert(cur_state->base == 0);
            }
        }

        return has_overflow_labels;
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
            std::size_t skip;
            std::uint32_t label = utf8_decode((const char *)text, skip);
            assert(this->is_valid_child(cur));
            State & cur_state = this->states_[cur];
            ident_t base = cur_state.base;
            ident_t child = base + label;
            assert(this->is_valid_child(child));
            State & child_state = this->states_[child];
            //if (likely((child_state.check == cur) && this->is_allocated_child(child))) {
            if (likely(child_state.check == cur)) {
                cur = child;
                text += skip;
            } else {
                if ((cur != root) && (cur_state.is_final != 0)) {
                    matchInfo.end        = (std::uint32_t)(text - text_first);
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
            std::size_t skip;
            std::uint32_t label = utf8_decode((const char *)text, skip);
            text += skip;
            if (likely(cur == root)) {
                assert(this->is_valid_id(cur));
                State & cur_state = this->states_[cur];
                ident_t base = cur_state.base;
                ident_t child = base + label;
                assert(this->is_valid_child(child));
                State & child_state = this->states_[child];
                //if (likely((child_state.check == cur) && this->is_allocated_child(child))) {
                if (likely(child_state.check == cur)) {
                    cur = child;
                } else {
                    goto MatchNextLabel;
                }
            } else {
                do {
                    assert(this->is_valid_id(cur));
                    assert((cur == root) || ((cur != root) && !this->is_free_state(cur)));
                    State & cur_state = this->states_[cur];
                    ident_t base = cur_state.base;
                    ident_t child = base + label;
                    assert(this->is_valid_child(child));
                    State & child_state = this->states_[child];
                    //if (likely((child_state.check != cur) || this->is_free_child(child))) {
                    if (likely(child_state.check != cur)) {
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
                assert(this->is_valid_id(node));
                assert(!this->is_free_state(node));
                if (unlikely(node_state.is_final != 0)) {
                    // Matched
                    matchInfo.end        = (std::uint32_t)(text - text_first);
                    matchInfo.pattern_id = node_state.pattern_id;
                    if (node != cur) {
                        // If current full prefix is matched, judge the continous suffixs has some chars is matched?
                        // If it's have any chars is matched, it would be the longest matched suffix.
                        MatchInfo matchInfo1;
                        bool matched1 = this->match_tail(cur, text, text_last, matchInfo1);
                        if (matched1) {
                            matchInfo.end       += matchInfo1.end;
                            matchInfo.pattern_id = matchInfo1.pattern_id;
                            return true;
                        }
                    }
                    if (node_state.has_child != 0) {
                        // If a sub suffix exists, match the continous longest suffixs.
                        MatchInfo matchInfo2;
                        bool matched2 = this->match_tail(node, text, text_last, matchInfo2);
                        if (matched2) {
                            matchInfo.end       += matchInfo2.end;
                            matchInfo.pattern_id = matchInfo2.pattern_id;
                        }
                    }
                    return true;
                }
                node = node_state.fail_link;
            } while (node != root);

MatchNextLabel:
            (void)0;
        }

        return matched;
    }

    bool match_one(const char_type * first, const char_type * last, MatchInfo & matchInfo) {
        return this->match_one((const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

    bool match_one(const schar_type * first, const schar_type * last, MatchInfo & matchInfo) {
        return this->match_one((const uchar_type *)first, (const uchar_type *)last, matchInfo);
    }

    void match_one(const uchar_type * first, const uchar_type * last,
                   std::vector<MatchInfoEx> & matchList,
                   const on_hit_callback & onHit_callback) {
        uchar_type * text_first = (uchar_type *)first;
        uchar_type * text_last = (uchar_type *)last;
        uchar_type * text = text_first;
        assert(text_first <= text_last);

        ident_t root = this->root();
        ident_t cur = root;

        matchList.clear();
        assert(onHit_callback);

        while (text < text_last) {
            ident_t node;
            std::size_t skip;
            std::uint32_t label = utf8_decode((const char *)text, skip);
            text += skip;
            if (likely(cur == root)) {
                assert(this->is_valid_id(cur));
                State & cur_state = this->states_[cur];
                ident_t base = cur_state.base;
                ident_t child = base + label;
                assert(this->is_valid_child(child));
                State & child_state = this->states_[child];
                if (likely(child_state.check == cur)) {
                    cur = child;
                } else {
                    goto MatchNextLabel;
                }
            } else {
                do {
                    assert(this->is_valid_id(cur));
                    assert((cur == root) || ((cur != root) && !this->is_free_state(cur)));
                    State & cur_state = this->states_[cur];
                    ident_t base = cur_state.base;
                    ident_t child = base + label;
                    assert(this->is_valid_child(child));
                    State & child_state = this->states_[child];
                    if (likely(child_state.check != cur)) {
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
                assert(this->is_valid_id(node));
                assert(!this->is_free_state(node));
                if (unlikely(node_state.is_final != 0)) {
                    // Matched
                    MatchInfoEx matchInfo;
                    matchInfo.end        = (std::uint32_t)(text - text_first);
                    matchInfo.pattern_id = node_state.pattern_id;
                    if (node != cur) {
                        // If current full prefix is matched, judge the continous suffixs has some chars is matched?
                        // If it's have any chars is matched, it would be the longest matched suffix.
                        MatchInfo matchInfo1;
                        bool matched1 = this->match_tail(cur, text, text_last, matchInfo1);
                        if (matched1) {
                            matchInfo.end       += matchInfo1.end;
                            matchInfo.pattern_id = matchInfo1.pattern_id;
                            size_type length = onHit_callback(matchInfo1.pattern_id);
                            matchInfo.begin = matchInfo.end - (std::uint32_t)length;
                            matchList.push_back(matchInfo);
                            cur = root;
                            text += matchInfo1.end;
                            break;
                        }
                    }
                    if (node_state.has_child != 0) {
                        // If a sub suffix exists, match the continous longest suffixs.
                        MatchInfo matchInfo2;
                        bool matched2 = this->match_tail(node, text, text_last, matchInfo2);
                        if (matched2) {
                            matchInfo.end       += matchInfo2.end;
                            matchInfo.pattern_id = matchInfo2.pattern_id;
                            text += matchInfo2.end;
                        }
                    }
                    size_type length = onHit_callback(matchInfo.pattern_id);
                    matchInfo.begin = matchInfo.end - (std::uint32_t)length;
                    matchList.push_back(matchInfo);
                    cur = root;
                    break;
                }
                node = node_state.fail_link;
            } while (node != root);

MatchNextLabel:
            (void)0;
        }
    }

    void match_one(const char_type * first, const char_type * last,
                   std::vector<MatchInfoEx> & matchList,
                   const on_hit_callback & onHit_callback) {
        return this->match_one((const uchar_type *)first, (const uchar_type *)last, matchList, onHit_callback);
    }

    void match_one(const schar_type * first, const schar_type * last,
                   std::vector<MatchInfoEx> & matchList,
                   const on_hit_callback & onHit_callback) {
        return this->match_one((const uchar_type *)first, (const uchar_type *)last, matchList, onHit_callback);
    }

private:
    void create_root() {
        assert(this->states_.size() == 0);

        // Append dummy state for invalid link, Identifier = 0
        State dummy;
        dummy.base = 0;
        dummy.check = 0;
        dummy.fail_link = kInvalidIdent;
        dummy.identifier = 0;
        this->states_.push_back(std::move(dummy));

        // Append root state, Identifier = 1
        State root;
        root.base = 0;
        root.check = 0;
        root.fail_link = kInvalidIdent;
        root.identifier = 0;
        this->states_.push_back(std::move(root));
    }
};

} // namespace utf8

#endif // DOUBLE_ARRAY_TRIE_UTF8_H
