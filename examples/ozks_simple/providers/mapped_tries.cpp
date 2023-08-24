// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <unordered_map>

// OZKS
#include "oZKS/defines.h"
#include "oZKS/compressed_trie.h"
#include "mapped_tries.h"

using namespace std;
using namespace ozks;
using namespace ozks_simple;
using namespace ozks_simple::providers;

namespace {
    unordered_map<trie_id_type, shared_ptr<CompressedTrie>> tries_;
    size_t thread_count_ = 0;
    TrieType trie_type_ = TrieType::Stored;
    shared_ptr<storage::Storage> storage_;
}

void ozks_simple::providers::set_config(
    shared_ptr<storage::Storage> storage,
    TrieType trie_type,
    size_t thread_count)
{
    storage_ = storage;
    trie_type_ = trie_type;
    thread_count_ = thread_count;
}

shared_ptr<CompressedTrie> ozks_simple::providers::get_compressed_trie(trie_id_type trie_id)
{
    shared_ptr<CompressedTrie> result;

    auto it = tries_.find(trie_id);
    if (it == tries_.end()) {
        // Need to add it
        switch (trie_type_) {
        case TrieType::Stored:
            result = make_shared<CompressedTrie>(storage_, TrieType::Stored);
            break;
        case TrieType::Linked:
            result = make_shared<CompressedTrie>(storage_, TrieType::Linked, thread_count_);
            break;
        case TrieType::LinkedNoStorage:
            result = make_shared<CompressedTrie>(nullptr, TrieType::Linked, thread_count_);
            break;
        default:
            throw logic_error("Invalid Trie Type");
        }

        result->init(trie_id);
        result->save_to_storage();
        tries_[trie_id] = result;
    } else {
        result = (*it).second;
    }

    return result;
}
