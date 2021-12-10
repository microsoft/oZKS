// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

// OZKS
#include "oZKS/fourq/random.h"
#include "oZKS/hash/sha512.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;

string utils::to_string(const partial_label_type &label)
{
    stringstream ss;
    size_t bit_idx = 0;

    while (bit_idx < label.size()) {
        ss << (label[bit_idx++] ? "1" : "0");
    }

    return ss.str();
}

string utils::to_string(gsl::span<const byte> bytes)
{
    stringstream ss;
    ss << hex;

    for_each(bytes.begin(), bytes.end(), [&](const byte &bt) {
        int byte_val = static_cast<int>(bt);
        ss << setw(2) << setfill('0') << byte_val;
    });

    return ss.str();
}

vector<bool> utils::bytes_to_bools(gsl::span<const byte> bytes)
{
    size_t vec_size = bytes.size() * 8;
    vector<bool> result(vec_size);

    size_t bit_idx = 0;
    for_each(bytes.begin(), bytes.end(), [&](const byte &bt) {
        for (int bit = 7; bit >= 0; bit--) {
            byte mask = byte{ 1 } << bit;
            result[bit_idx++] = (bt & mask) != byte{ 0 };
        }
    });

    return result;
}

vector<bool> utils::bytes_to_bools(const byte *bytes, size_t size)
{
    vector<bool> result(size);
    size_t bit_mod = size % 8;
    size_t bit_idx = (bit_mod == 0) ? 7 : (size % 8) - 1;
    size_t byte_idx = 0;

    for (size_t idx = 0; idx < size; idx++) {
        byte mask = byte{ 1 } << bit_idx;
        result[idx] = (bytes[byte_idx] & mask) != byte{ 0 };

        if (bit_idx == 0) {
            bit_idx = 7;
            byte_idx++;
        } else {
            bit_idx--;
        }
    }

    return result;
}

void utils::bools_to_bytes(const vector<bool> &bools, vector<byte> &bytes)
{
    bytes.resize(0);
    size_t bit_idx = (bools.size() % 8);
    byte curr_byte = byte{ 0 };

    if (bit_idx == 0) {
        bit_idx = 7;
    } else {
        bit_idx--;
    }

    for_each(bools.begin(), bools.end(), [&](const bool &bl) {
        byte mask = bl ? byte{ 1 } : byte{ 0 };
        mask <<= bit_idx;
        curr_byte |= mask;

        if (bit_idx == 0) {
            bytes.push_back(curr_byte);
            bit_idx = 7;
            curr_byte = byte{ 0 };
        } else {
            bit_idx--;
        }
    });

    if (bools.size() > 0 && bytes.size() == 0) {
        // Add whatever we have
        bytes.push_back(curr_byte);
    }
}

vector<byte> utils::bools_to_bytes(const vector<bool> &bools)
{
    vector<byte> result;
    bools_to_bytes(bools, result);
    return result;
}

partial_label_type utils::get_common_prefix(
    const partial_label_type &label1, const partial_label_type &label2)
{
    partial_label_type result;

    size_t bit_idx = 0;
    while (bit_idx < label1.size() && bit_idx < label2.size() &&
           label1[bit_idx] == label2[bit_idx]) {
        result.push_back(label1[bit_idx]);
        bit_idx++;
    }

    return result;
}

void utils::compute_leaf_hash(
    const partial_label_type &label, const payload_type &payload, size_t epoch, hash_type &hash)
{
    vector<byte> lbl;
    utils::bools_to_bytes(label, lbl);
    vector<byte> buffer(lbl.size() + payload.size() + sizeof(size_t));
    utils::copy_bytes(lbl.data(), lbl.size(), buffer.data());
    utils::copy_bytes(payload.data(), payload.size(), buffer.data() + lbl.size());
    utils::copy_bytes(&epoch, sizeof(size_t), buffer.data() + lbl.size() + payload.size());

    hash = compute_hash(buffer, "leaf_hash");
}

void utils::compute_node_hash(
    const partial_label_type &partial_left_label,
    const hash_type &left_hash,
    const partial_label_type &partial_right_label,
    const hash_type &right_hash,
    hash_type &hash)
{
    // Hash is a concatenation of left and right hashes
    label_type left_label = bools_to_bytes(partial_left_label);
    label_type right_label = bools_to_bytes(partial_right_label);

    vector<byte> buffer(
        left_label.size() + left_hash.size() + right_label.size() + right_hash.size());
    utils::copy_bytes(left_label.data(), left_label.size(), buffer.data());
    utils::copy_bytes(left_hash.data(), left_hash.size(), buffer.data() + left_label.size());
    utils::copy_bytes(
        right_label.data(),
        right_label.size(),
        buffer.data() + left_label.size() + left_hash.size());
    utils::copy_bytes(
        right_hash.data(),
        right_hash.size(),
        buffer.data() + left_label.size() + left_hash.size() + right_label.size());

    hash = compute_hash(buffer, "node_hash");
}

void utils::compute_randomness_hash(
    gsl::span<const byte> buffer, hash_type &hash, randomness_type &randomness)
{
    randomness.resize(64); // 64 bytes
    if (!random_bytes(
            reinterpret_cast<unsigned char *>(randomness.data()),
            static_cast<unsigned int>(randomness.size()))) {
        throw runtime_error("Failed to get random bytes");
    }

    vector<byte> hash_buffer(randomness.size() + buffer.size());
    utils::copy_bytes(buffer.data(), buffer.size(), hash_buffer.data());
    utils::copy_bytes(randomness.data(), randomness.size(), hash_buffer.data() + buffer.size());

    hash = compute_hash(hash_buffer, "randomness_hash");
}

hash_type utils::compute_hash(gsl::span<const byte> in, const string &domain_str)
{
    // Create the actual input buffer by prepending the given input with domain_str
    vector<byte> hash_in(in.size() + domain_str.size());
    utils::copy_bytes(domain_str.c_str(), domain_str.size(), hash_in.data());
    utils::copy_bytes(in.data(), in.size(), hash_in.data() + domain_str.size());

    hash_type hash;
    crypto_sha512(
        reinterpret_cast<unsigned char *>(hash_in.data()),
        hash_in.size(),
        reinterpret_cast<unsigned char *>(hash.data()));

    return hash;
}

void utils::copy_bytes(const void *src, size_t count, void *dst)
{
    if (!count) {
        return;
    }
    if (!src) {
        throw invalid_argument("cannot copy data: source is null");
    }
    if (!dst) {
        throw invalid_argument("cannot copy data: destination is null");
    }
    copy_n(
        reinterpret_cast<const unsigned char *>(src),
        count,
        reinterpret_cast<unsigned char *>(dst));
}

void utils::read_from_serialization_reader(
    SerializationReader &reader, uint32_t byte_count, vector<uint8_t> &destination)
{
    // Initial number of bytes to read
    const size_t first_to_read = 1024;

    // How many bytes we read in this round
    size_t to_read = min(static_cast<size_t>(byte_count), first_to_read);

    while (byte_count) {
        size_t old_size = destination.size();

        // Save the old size and resize by adding to_read many bytes to vector
        destination.resize(old_size + to_read);

        // Write some data into the vector
        reader.read(destination.data() + old_size, to_read);

        // Decrement byte_count and increase to_read for next round
        byte_count -= static_cast<uint32_t>(to_read);

        // Set to_read for next round exactly to right size so we don't read too much
        to_read = min(2 * to_read, static_cast<size_t>(byte_count));
    }
}

vector<uint8_t> utils::read_from_serialization_reader(SerializationReader &reader)
{
    uint32_t size = 0;
    reader.read(reinterpret_cast<uint8_t *>(&size), sizeof(uint32_t));

    vector<uint8_t> result(sizeof(uint32_t));
    copy_bytes(&size, sizeof(uint32_t), result.data());

    read_from_serialization_reader(reader, size, result);

    return result;
}
