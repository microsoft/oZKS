// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <thread>

// OZKS
#include "oZKS/core_types_generated.h"
#include "oZKS/path_element_generated.h"
#include "oZKS/utilities.h"

// OpenSSL
#include <openssl/rand.h>
#include <openssl/sha.h>

using namespace std;
using namespace ozks;

namespace {
    template <std::size_t sz>
    void hash_bytes(const std::byte *data, std::size_t size, std::byte *hash_out);

    template <>
    void hash_bytes<SHA256_DIGEST_LENGTH>(
        const std::byte *data, std::size_t size, std::byte *hash_out)
    {
        SHA256(
            reinterpret_cast<const unsigned char *>(data),
            size,
            reinterpret_cast<unsigned char *>(hash_out));
    }

    template <>
    void hash_bytes<SHA512_DIGEST_LENGTH>(
        const std::byte *data, std::size_t size, std::byte *hash_out)
    {
        SHA512(
            reinterpret_cast<const unsigned char *>(data),
            size,
            reinterpret_cast<unsigned char *>(hash_out));
    }

    hash_type clear_lsb(hash_type hash)
    {
        hash[0] &= std::byte{ 0xFE };
        return hash;
    }
} // namespace

void utils::random_bytes(gsl::span<byte> random_array)
{
    if (!RAND_bytes(
            reinterpret_cast<unsigned char *>(random_array.data()),
            static_cast<int>(random_array.size()))) {
        throw runtime_error("Failed to get random bytes");
    }
}

string utils::to_string(const PartialLabel &label)
{
    stringstream ss;
    uint32_t bit_index = 0;

    while (bit_index < label.bit_count()) {
        ss << (label[bit_index++] ? "1" : "0");
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

hash_type utils::compute_key_hash(const key_type &key)
{
    return compute_hash(key, "key_hash");
}

hash_type utils::compute_leaf_hash(const PartialLabel &label, const hash_type &hash, size_t epoch)
{
    array<byte, PartialLabel::SaveSize + hash_size + sizeof(epoch)> buffer{};
    label.save(gsl::span<byte, PartialLabel::SaveSize>(buffer.data(), PartialLabel::SaveSize));
    utils::copy_bytes(hash.data(), hash_size, buffer.data() + PartialLabel::SaveSize);
    utils::copy_bytes(&epoch, sizeof(epoch), buffer.data() + PartialLabel::SaveSize + hash_size);

    return clear_lsb(compute_hash(buffer, "leaf_hash"));
}

hash_type utils::compute_node_hash(
    const PartialLabel &left_label,
    const hash_type &left_hash,
    const PartialLabel &right_label,
    const hash_type &right_hash)
{
    array<byte, 2 * (PartialLabel::SaveSize + hash_size)> buffer{};
    left_label.save(gsl::span<byte, PartialLabel::SaveSize>(buffer.data(), PartialLabel::SaveSize));
    utils::copy_bytes(left_hash.data(), hash_size, buffer.data() + PartialLabel::SaveSize);
    right_label.save(
        gsl::span<byte, PartialLabel::SaveSize>(
            buffer.data() + PartialLabel::SaveSize + hash_size, PartialLabel::SaveSize));
    utils::copy_bytes(
        right_hash.data(), hash_size, buffer.data() + 2 * PartialLabel::SaveSize + hash_size);

    return clear_lsb(compute_hash(buffer, "node_hash"));
}

hash_type utils::compute_randomness_hash(gsl::span<const byte> buffer, randomness_type &randomness)
{
    random_bytes(randomness);
    return compute_payload_commitment(buffer, PayloadCommitmentType::CommitedPayload, randomness);
}

hash_type utils::compute_payload_commitment(
    gsl::span<const byte> payload,
    PayloadCommitmentType payload_commitment,
    const randomness_type &randomness)
{
    switch (payload_commitment) {
    case PayloadCommitmentType::CommitedPayload: {
        vector<byte> buffer(payload.size() + randomness.size());
        copy_bytes(payload.data(), payload.size(), buffer.data());
        copy_bytes(randomness.data(), randomness.size(), buffer.data() + payload.size());
        return compute_hash(buffer, "randomness_hash");
    }
    case PayloadCommitmentType::UncommitedPayload:
        return compute_hash(payload, "commitment_hash");
    default:
        throw invalid_argument("Invalid payload commitment type");
    }
}

hash_type utils::compute_hash(gsl::span<const byte> in, const string &domain_str)
{
    hash_type hash{};
    compute_hash<hash_size>(in, domain_str, hash);
    return hash;
}

template <size_t sz>
void utils::compute_hash(
    gsl::span<const byte> in, const string &domain_str, gsl::span<byte, sz> out)
{
    // Create the actual input buffer by prepending the given input with domain_str
    vector<byte> hash_in(in.size() + domain_str.size());
    utils::copy_bytes(domain_str.c_str(), domain_str.size(), hash_in.data());
    utils::copy_bytes(in.data(), in.size(), hash_in.data() + domain_str.size());

    hash_bytes<sz>(hash_in.data(), hash_in.size(), out.data());
}

hash_type utils::compute_hash(gsl::span<const byte> in)
{
    hash_type hash{};
    compute_hash<hash_size>(in, hash);
    return hash;
}

template <size_t sz>
void utils::compute_hash(gsl::span<const byte> in, gsl::span<byte, sz> out)
{
    hash_bytes<sz>(in.data(), in.size(), out.data());
}

std::pair<hash_type, std::unique_ptr<vrf::Proof>> utils::get_node_label(
    const key_type &key,
    vrf::SecretKey &vrf_sk,
    VRFCache &cache,
    LabelType label_type)
{
    // In any case, first hash the key with utils::compute_key_hash
    hash_type key_hash = utils::compute_key_hash(key);
    std::unique_ptr<vrf::Proof> proof;

    if (label_type == LabelType::VRFLabels) {
        // Look up the VRF proof from the cache
        auto cache_value = cache.get(key_hash);
        if (nullptr != cache_value) {
            proof = std::move(cache_value);
        } else {
            // Not found? Need to compute the proof and add it to the cache
            auto vrf_proof = vrf_sk.get_vrf_proof(key_hash);
            proof = std::move(vrf_proof);
            cache.add(key_hash, *proof);
        }

        // key_hash needs to be updated to be the VRF value hash.
        // Note that we have not verified the validity of the proof here.
        // We assume it is valid, as it is loaded from the cache and the
        // cache is only populated with valid proofs.
        auto vrf_value = proof->get_vrf_value();
        utils::copy_bytes_to_hash(vrf_value, key_hash);
    }

    return { key_hash, std::move(proof) };
}

hash_type utils::get_node_label(
    const key_type &key, vrf::SecretKey &vrf_sk, LabelType label_type)
{
    // In any case, first hash the key with utils::compute_key_hash
    hash_type key_hash = utils::compute_key_hash(key);

    if (label_type == LabelType::VRFLabels) {
        // If this OZKS uses VRFs, set key_hash instead to the VRF value (hash)
        auto proof = vrf_sk.get_vrf_proof(key_hash);
        auto vrf_value = proof->get_vrf_value();
        utils::copy_bytes_to_hash(vrf_value, key_hash);
    }

    return key_hash;
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

void utils::copy_bytes_to_hash(gsl::span<const std::byte> src, hash_type &dst)
{
    if (src.size() != hash_size) {
        throw invalid_argument("cannot copy to hash: source size is incorrect");
    }
    copy_bytes(src.data(), hash_size, dst.data());
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

size_t utils::write_path_element(
    const PartialLabel &label, const hash_type &hash, SerializationWriter &writer)
{
    flatbuffers::FlatBufferBuilder f_builder;

    array<uint8_t, PartialLabel::SaveSize> label_data{};
    label.save(gsl::span<byte, PartialLabel::SaveSize>(
        reinterpret_cast<byte *>(label_data.data()), label_data.size()));
    fbs::PathElementData pe_data(
        flatbuffers::span<const uint8_t, hash_size>{ reinterpret_cast<const uint8_t *>(hash.data()),
                                                     hash_size },
        flatbuffers::span<const uint8_t, PartialLabel::SaveSize>{ label_data });

    fbs::PathElementBuilder pe_builder(f_builder);
    pe_builder.add_path_element(&pe_data);

    auto fbs_append_proof = pe_builder.Finish();
    f_builder.FinishSizePrefixed(fbs_append_proof);

    writer.write(f_builder.GetBufferPointer(), f_builder.GetSize());
    return f_builder.GetSize();
}

size_t utils::read_path_element(SerializationReader &reader, PartialLabel &label, hash_type &hash)
{
    vector<unsigned char> pe_data(read_from_serialization_reader(reader));
    auto pe_verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(pe_data.data()), pe_data.size());
    bool pe_safe = fbs::VerifySizePrefixedPathElementBuffer(pe_verifier);
    if (!pe_safe) {
        throw runtime_error("Failed to load PathElement: invalid PathElement buffer");
    }

    auto fbs_path_element = fbs::GetSizePrefixedPathElement(pe_data.data());
    copy_bytes(fbs_path_element->path_element()->hash().data()->data(), hash_size, hash.data());
    label.load(gsl::span<const byte, PartialLabel::SaveSize>{
        reinterpret_cast<const byte *>(fbs_path_element->path_element()->label().data()->data()),
        fbs_path_element->path_element()->label().data()->size() });

    return pe_data.size();
}

pair<hash_type, randomness_type> utils::commit_payload(
    const payload_type &payload, PayloadCommitmentType payload_commitment)
{
    randomness_type randomness{};
    if (payload_commitment == PayloadCommitmentType::CommitedPayload) {
        random_bytes(randomness);
    }
    return { compute_payload_commitment(payload, payload_commitment, randomness), randomness };
}

size_t utils::get_insertion_thread_limit(shared_ptr<CTNode> node, size_t max_threads)
{
    size_t max_limit = thread::hardware_concurrency();
    if (max_threads != 0) {
        max_limit = max_threads;
    }

    // If node is null, we simply return the max_limit
    if (node == nullptr) {
        return max_limit;
    }

    size_t curr_limit = 1;
    vector<shared_ptr<CTNode>> level;

    auto check_node = [](shared_ptr<CTNode> n, vector<shared_ptr<CTNode>> &next_lvl) {
        PartialLabel left_child_lbl = n->label();
        left_child_lbl.add_bit(false);
        PartialLabel right_child_lbl = n->label();
        right_child_lbl.add_bit(true);

        shared_ptr<CTNode> left = n->left();
        shared_ptr<CTNode> right = n->right();

        if (nullptr == left || nullptr == right || left->label() != left_child_lbl ||
            right->label() != right_child_lbl) {
            return false;
        }

        next_lvl.push_back(left);
        next_lvl.push_back(right);

        return true;
    };

    // First level
    if (!check_node(node, level)) {
        return curr_limit;
    }

    curr_limit *= 2;

    // Next levels
    while (true) {
        vector<shared_ptr<CTNode>> next_level;

        for (size_t idx = 0; idx < level.size(); idx += 2) {
            if (!check_node(level[idx], next_level)) {
                return curr_limit;
            }

            if (!check_node(level[idx + 1], next_level)) {
                return curr_limit;
            }
        }

        curr_limit *= 2;
        if (curr_limit == max_limit) {
            return curr_limit;
        }
        if (curr_limit > max_limit) {
            return curr_limit /= 2;
        }

        level.swap(next_level);
    }
}

size_t utils::get_insertion_index(size_t bit_count, const PartialLabel &label)
{
    size_t result = 0;
    for (size_t idx = 0; idx < bit_count && idx < label.bit_count(); idx++) {
        result <<= 1;
        result |= static_cast<size_t>(label[idx]);
    }

    return result;
}

size_t utils::get_log2(size_t n)
{
    size_t r = 0;   // r will be log(v)
    while (n >>= 1) // unroll for more speed...
    {
        r++;
    }

    return r;
}

bool utils::verify_vrf_proof(
    const key_type &key,
    const vrf::Proof &proof,
    vrf::PublicKey &pk)
{
    hash_type hash = utils::compute_key_hash(key);
    auto verif_result = pk.verify_vrf_proof(hash, proof);
    return verif_result.first;
}

// Explicit instantiations
template void utils::compute_hash(
    gsl::span<const byte> in, const string &domain_str, gsl::span<byte, 32> out);
template void utils::compute_hash(
    gsl::span<const byte> in, const string &domain_str, gsl::span<byte, 64> out);
template void utils::compute_hash(gsl::span<const byte> in, gsl::span<byte, 32> out);
template void utils::compute_hash(gsl::span<const byte> in, gsl::span<byte, 64> out);
