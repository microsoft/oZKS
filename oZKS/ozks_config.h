// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <iostream>
#include <vector>

// GSL
#include "gsl/span"

// OZKS
#include "oZKS/defines.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/storage/storage.h"

namespace ozks {
    class OZKSConfig {
    public:
        /**
        Construct an instance of OZKSConfig
        */
        OZKSConfig(
            PayloadCommitmentType commitment_type,
            LabelType label_type,
            TrieType trie_type,
            std::shared_ptr<storage::Storage> storage,
            const gsl::span<const std::byte> vrf_seed = gsl::span<std::byte>(nullptr, nullptr),
            std::size_t vrf_cache_size = 0,
            std::size_t thread_count = 0);

        /**
        Construct an instance of OZKSConfig
        */
        OZKSConfig();

        /**
        Returns the type of payload commitment to use
        */
        PayloadCommitmentType payload_commitment() const
        {
            return commitment_type_;
        }

        /**
        Returns the type of labels to use
        */
        LabelType label_type() const
        {
            return label_type_;
        }

        /**
        Returns the type of the trie
        */
        TrieType trie_type() const
        {
            return trie_type_;
        }

        /**
        Storage to use
        */
        std::shared_ptr<storage::Storage> storage() const
        {
            return storage_;
        }

        /**
        Get a byte span that will be used as a seed for generating VRF keys.
        This parameter will only be used if the label type to use is VRF.
        If the span is empty, a random seed will be used instead.
        */
        gsl::span<const std::byte> vrf_seed() const
        {
            return gsl::span<const std::byte>(vrf_seed_.data(), vrf_seed_.size());
        }

        /**
        Get the maximum number of threads to use for insertion.
        0 means using the number of processors in the machine.
        */
        std::size_t thread_count() const
        {
            return thread_count_;
        }

        /**
        Get the size of the VRF cache.
        */
        std::size_t vrf_cache_size() const
        {
            return vrf_cache_size_;
        }

        /**
        Save the current OZKSConfig instance to a byte vector
        */
        template <typename T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Load an oZKS instance from a byte vector
        */
        template <typename T>
        static std::size_t Load(
            OZKSConfig &config,
            const std::vector<T> &vec,
            std::shared_ptr<storage::Storage> storage,
            std::size_t position = 0);

    private:
        PayloadCommitmentType commitment_type_;
        LabelType label_type_;
        TrieType trie_type_;
        std::shared_ptr<storage::Storage> storage_;
        std::vector<std::byte> vrf_seed_;
        std::size_t vrf_cache_size_;
        std::size_t thread_count_;

        /**
        Save the current OZKSConfig object to the given serialization writer
        */
        std::size_t save(SerializationWriter &writer) const;

        /**
        Load an OZKSConfig object from the given serialization reader
        */
        static std::size_t Load(
            OZKSConfig &config,
            SerializationReader &reader,
            std::shared_ptr<storage::Storage> storage);
    };
} // namespace ozks
