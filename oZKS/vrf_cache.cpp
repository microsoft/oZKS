// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "oZKS/vrf_cache.h"

using namespace std;
using namespace ozks;

VRFCache::VRFCache(size_t vrf_cache_size) : cache_size_(vrf_cache_size)
{
    if (cache_size_) {
        cache_ = make_unique<Poco::LRUCache<hash_type, VRFProof>>(cache_size_);
    }
}

VRFCache &VRFCache::operator=(const VRFCache &other)
{
    // Just copy the cache size; not the contents
    VRFCache new_cache(other.cache_size_);
    swap(cache_, new_cache.cache_);
    swap(cache_size_, new_cache.cache_size_);

    return *this;
}

void VRFCache::add(const hash_type &key_hash, const VRFProof &vrf_proof)
{
    if (cache_) {
        cache_->add(key_hash, vrf_proof);
    }
}

optional<VRFProof> VRFCache::get(const hash_type &key_hash)
{
    optional<VRFProof> ret = nullopt;
    if (cache_) {
        auto cache_value = cache_->get(key_hash);
        if (!cache_value.isNull()) {
            ret = optional<VRFProof>{ *cache_value };
            cache_hits_++;
            return ret;
        }
    }

    // Even if there is no cache (size == 0) we count the miss
    cache_misses_++;
    return ret;
}
