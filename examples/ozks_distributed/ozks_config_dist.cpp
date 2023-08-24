// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// oZKS
#include "ozks_config_dist.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks_distributed;

OZKSConfigDist::OZKSConfigDist(
    PayloadCommitmentType commitment_type,
    LabelType label_type,
    TrieType trie_type,
    shared_ptr<Storage> storage,
    shared_ptr<Storage> storage_querier,
    const gsl::span<const byte> vrf_seed,
    size_t vrf_cache_size,
    size_t thread_count)
    : OZKSConfig(
          commitment_type, label_type, trie_type, storage, vrf_seed, vrf_cache_size, thread_count),
      storage_querier_(storage_querier)
{}

OZKSConfigDist::OZKSConfigDist()
{}
