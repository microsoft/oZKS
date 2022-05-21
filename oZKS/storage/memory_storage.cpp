// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "memory_storage.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;

MemoryStorage::MemoryStorage(vector<uint8_t> &v) : v_(v)
{}

tuple<size_t, partial_label_type, partial_label_type> MemoryStorage::LoadCTNode(CTNode& node)
{
    tuple<size_t, partial_label_type, partial_label_type> result;

    return result;
}

size_t MemoryStorage::SaveCTNode(const CTNode& node)
{
    size_t result = 0;

    return result;
}

size_t MemoryStorage::LoadCompressedTrie(CompressedTrie& trie)
{
    size_t result = 0;

    return result;
}

size_t MemoryStorage::SaveCompressedTrie(const CompressedTrie& trie)
{
    size_t result = 0;

    return result;
}
