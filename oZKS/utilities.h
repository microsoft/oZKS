// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

// OZKS
#include "oZKS/defines.h"
#include "oZKS/serialization_helpers.h"

// GSL
#include "gsl/span"

namespace ozks {
    namespace utils {
        /**
        Size of a hash
        */
        const std::size_t hash_size = 64;

        /**
        Convert the given partial label to a hexadecimal string
        */
        std::string to_string(const partial_label_type &label);

        /**
        Convert the given byte vector to an hexadecimal string
        */
        std::string to_string(gsl::span<const std::byte> bytes);

        /**
        Convert the given byte vector to a bool vector
        */
        std::vector<bool> bytes_to_bools(gsl::span<const std::byte> bytes);

        /**
        Convert the given byte data to a bool vector.
        The paremeter 'size' is the actual number of bits to convert.
        */
        std::vector<bool> bytes_to_bools(const std::byte *bytes, std::size_t size);

        /**
        Convert the given bool vector to a vector of bytes.
        High-order bits will be padded with zeros if the length of the bool vector is not
        byte-aligned.
        */
        void bools_to_bytes(const std::vector<bool> &bools, std::vector<std::byte> &bytes);

        /**
        Convert the given bool vector to a vector of bytes.
        High-order bits will be padded with zeros if the length of the bool vector is not
        byte-aligned.
        */
        std::vector<std::byte> bools_to_bytes(const std::vector<bool> &bools);

        /**
        Get the high-order bits that are common between the given partial labels.
        */
        partial_label_type get_common_prefix(
            const partial_label_type &label1, const partial_label_type &label2);

        /**
        Initialize a vector of bytes using a list of byte-convertible integers.
        */
        template <typename... Ts>
        std::vector<std::byte> make_bytes(Ts &&... args) noexcept
        {
            return { std::byte(std::forward<Ts>(args))... };
        }

        /**
        Copies bytes from source to destination, throwing an exception if either source or
        destination is nullptr. The memory ranges are assumed to be non-overlapping.
        */
        void copy_bytes(const void *src, std::size_t count, void *dst);

        /**
        Compute the hash of a leaf node in a Compressed Trie.
        The hash is computed by using BLAKE2 on the concatenation of:
        - partial label
        - payload
        - epoch
        */
        void compute_leaf_hash(
            const partial_label_type &label,
            const payload_type &payload,
            std::size_t epoch,
            hash_type &hash);

        /**
        Compute the hash of a non-leaf node in a Compressed Trie.
        The hash is computed by using BLAKE2 on the concatenation of:
        - partial label of left node
        - hash of left node
        - partial label of right node
        - hash of right node
        */
        void compute_node_hash(
            const partial_label_type &partial_left_label,
            const hash_type &left_hash,
            const partial_label_type &partial_right_label,
            const hash_type &right_hash,
            hash_type &hash);

        /**
        Compute the hash of a byte buffer and add some randomness to it.
        Returns the hash and the randomness used to compute it.
        */
        void compute_randomness_hash(
            gsl::span<const std::byte> buffer, hash_type &hash, randomness_type &randomness);

        /**
        Compute a domain-separated BLAKE2 hash of the given input. In other words, this function
        returns BLAKE2(domain_str || in). The benefit is that we can create multiple independent
        hash functions from BLAKE2: a separate one for each distinct use.
        */
        hash_type compute_hash(gsl::span<const std::byte> in, const std::string &domain_str);

        /**
        Hasher for a byte vector.
        Needed to be able to use a byte vector as the key to a map.
        */
        struct byte_vector_hash {
            std::size_t operator()(std::vector<std::byte> const &v) const
            {
                hash_type hash = compute_hash(v, "vector_hash");
                std::size_t result;
                copy_bytes(&hash, sizeof(std::size_t), &result);
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
    } // namespace utils
} // namespace ozks
