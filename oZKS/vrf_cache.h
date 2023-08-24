// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <atomic>
#include <memory>
#include <optional>

// OZKS
#include "oZKS/defines.h"
#include "oZKS/vrf.h"

// GSL
#include "gsl/span"

// Poco
#include "Poco/LRUCache.h"

namespace ozks {
    class VRFCache {
    public:
        VRFCache(std::size_t vrf_cache_size);

        VRFCache &operator=(const VRFCache &other);

        VRFCache(const VRFCache &other) : VRFCache(other.size())
        {}

        void add(const hash_type &key_hash, const VRFProof &vrf_proof);

        std::optional<VRFProof> get(const hash_type &key_hash);

        // Return the maximum number of elements the cache can hold
        std::size_t max_size() const noexcept
        {
            return cache_size_;
        }

        // Return the number of currently cached elements
        std::size_t size() const
        {
            return cache_ ? cache_->size() : 0;
        }

        void clear()
        {
            clear_contents();
            clear_stats();
        }

        void clear_contents()
        {
            if (cache_) {
                cache_->clear();
            }
        }

        void clear_stats() noexcept
        {
            cache_hits_ = 0;
            cache_misses_ = 0;
        }

        std::uint64_t cache_hits() const noexcept
        {
            return cache_hits_;
        }

        std::uint64_t cache_misses() const noexcept
        {
            return cache_misses_;
        }

    private:
        std::unique_ptr<Poco::LRUCache<hash_type, VRFProof>> cache_{ nullptr };

        std::size_t cache_size_;

        std::atomic_uint64_t cache_hits_ = 0;

        std::atomic_uint64_t cache_misses_ = 0;
    };
} // namespace ozks
