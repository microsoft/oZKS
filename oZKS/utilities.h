// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// OZKS
#include "oZKS/ct_node.h"
#include "oZKS/defines.h"
#include "oZKS/partial_label.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/vrf.h"
#include "oZKS/vrf_cache.h"

// GSL
#include "gsl/span"

namespace ozks {
    namespace utils {
        // Generate random bytes and output the result to random_array
        int random_bytes(std::byte *random_array, std::size_t nbytes);

        /**
        Convert the given partial label to a hexadecimal string
        */
        std::string to_string(const PartialLabel &label);

        /**
        Convert the given byte vector to an hexadecimal string
        */
        std::string to_string(gsl::span<const std::byte> bytes);

        /**
        Initialize a vector of bytes using a list of byte-convertible integers.
        */
        template <typename Out, typename... Ts>
        Out make_bytes(Ts &&... args) noexcept
        {
            return { static_cast<std::byte>(std::forward<Ts>(args))... };
        }

        /**
        Copies bytes from source to destination, throwing an exception if either source or
        destination is nullptr. The memory ranges are assumed to be non-overlapping.
        */
        void copy_bytes(const void *src, std::size_t count, void *dst);

        /**
        Compute an intermediate hash value for the label value in a Compressed Trie.
        This intermediate hash is used directly as the label if no VRF is used. Otherwise,
        it is given further as input to the VRF.
        */
        hash_type compute_key_hash(const key_type &key);

        /**
        Compute the hash of a leaf node in a Compressed Trie.
        The hash is computed by using BLAKE2 on the concatenation of:
        - partial label
        - payload
        - epoch
        */
        hash_type compute_leaf_hash(
            const PartialLabel &label, const hash_type &hash, std::size_t epoch);

        /**
        Compute the hash of a non-leaf node in a Compressed Trie.
        The hash is computed by using BLAKE2 on the concatenation of:
        - partial label of left node
        - hash of left node
        - partial label of right node
        - hash of right node
        */
        hash_type compute_node_hash(
            const PartialLabel &left_label,
            const hash_type &left_hash,
            const PartialLabel &right_label,
            const hash_type &right_hash);

        /**
        Compute the hash of a byte buffer and add some randomness to it.
        Returns the hash and the randomness used to compute it.
        */
        hash_type compute_randomness_hash(
            gsl::span<const std::byte> buffer, randomness_type &randomness);

        /**
        Compute a domain-separated hash of the given input. In other words, this function
        returns hash(domain_str||in).
        */
        hash_type compute_hash(gsl::span<const std::byte> in, const std::string &domain_str);

        /**
        Compute a domain-separated hash of the given input. In other words, this function
        returns hash(domain_str||in). The output of this function is of arbitrary length.
        */
        template <std::size_t sz>
        void compute_hash(
            gsl::span<const std::byte> in,
            const std::string &domain_str,
            gsl::span<std::byte, sz> out);

        /**
        Compute a hash of the given input.
        */
        hash_type compute_hash(gsl::span<const std::byte> in);

        /**
        Compute a hash of the given input. The output of this function is of arbitrary length.
        */
        template <std::size_t sz>
        void compute_hash(gsl::span<const std::byte> in, gsl::span<std::byte, sz> out);

        /**
        Compute the label for a given key.
        */
        hash_type get_node_label(
            const key_type &key,
            const VRFSecretKey &vrf_sk,
            VRFCache &cache,
            LabelType label_type,
            std::optional<VRFProof> &proof);

        /**
        This function computes the label for a given key. In case the VRF proof is not needed,
        this is faster than calling the overload that includes the VRF proof.
        */
        hash_type get_node_label(
            const key_type &key, const VRFSecretKey &vrf_sk, LabelType label_type);

        /**
        Hasher for a byte vector.
        Needed to be able to use a byte vector as the key to a map.
        */
        struct byte_vector_hash {
            std::size_t operator()(std::vector<std::byte> const &v) const
            {
                std::size_t result = 0;
                std::size_t idx = 0;
                do {
                    std::size_t val = 0;
                    utils::copy_bytes(
                        v.data() + idx, std::min(sizeof(std::size_t), v.size() - idx), &val);
                    result = std::hash<std::size_t>()(val) ^ (result << 1);
                    idx += sizeof(std::size_t);
                } while (idx < v.size());

                return result;
            }
        };

        /**
        This function reads a given number of bytes from a stream in small parts, writing the result
        to the end of a given vector. This can avoid issues where a large number of bytes is
        requested incorrectly to be read from a stream, causing a larger than necessary memory
        allocation.
        */
        void read_from_serialization_reader(
            SerializationReader &in,
            std::uint32_t byte_count,
            std::vector<unsigned char> &destination);

        /**
        This function reads a size-prefixed number of bytes from a stream and returns the result in
        a vector.
        */
        std::vector<uint8_t> read_from_serialization_reader(SerializationReader &reader);

        /**
        Write a path element
        */
        std::size_t write_path_element(
            const PartialLabel &label, const hash_type &hash, SerializationWriter &writer);

        /**
        Read a path element
        */
        std::size_t read_path_element(
            SerializationReader &reader, PartialLabel &label, hash_type &hash);

        /**
        Get a payload hash and the randomness used to compute it
        */
        std::pair<hash_type, randomness_type> commit_payload(
            const payload_type &payload, PayloadCommitmentType payload_commitment);

        /**
        Get the maximum number of threads that can be used for parallelized insertion
        */
        std::size_t get_insertion_thread_limit(
            std::shared_ptr<CTNode> node, std::size_t max_threads = 0);

        /**
        Get the insertion index for a given bit count
        */
        std::size_t get_insertion_index(std::size_t bit_count, const PartialLabel &label);

        /**
        Get logarithm base 2 of the given number
        */
        std::size_t get_log2(std::size_t n);
    } // namespace utils
} // namespace ozks
