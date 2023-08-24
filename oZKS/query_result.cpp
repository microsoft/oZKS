// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <stdexcept>

// oZKS
#include "oZKS/query_result.h"
#include "oZKS/query_result_generated.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;

bool QueryResult::verify_lookup_path(const commitment_type &commitment) const
{
    if (lookup_proof().size() == 0) {
        throw runtime_error("Append proof cannot be empty");
    }

    PartialLabel partial_label = lookup_proof()[0].first;
    hash_type hash = lookup_proof()[0].second;
    hash_type temp_hash;

    for (size_t idx = 1; idx < lookup_proof().size(); idx++) {
        auto &sibling = lookup_proof()[idx];
        PartialLabel common = PartialLabel::CommonPrefix(sibling.first, partial_label);

        // These are sibling nodes
        if (partial_label[common.bit_count()] == 0) {
            temp_hash =
                utils::compute_node_hash(partial_label, hash, sibling.first, sibling.second);
        } else {
            temp_hash =
                utils::compute_node_hash(sibling.first, sibling.second, partial_label, hash);
        }

        // Up the tree
        partial_label = common;
        hash = temp_hash;
    }

    // At the end, the resulting hash is either:
    // - The commitment
    // - the last child node hash we need to get the commitment
    commitment_type hash_commitment{};
    utils::copy_bytes(hash.data(), hash.size(), hash_commitment.data());
    if (hash_commitment == commitment) {
        return true;
    }

    if (partial_label.bit_count() == 0) {
        // This means the current hash should have matched the commitment, but it didn't
        return false;
    }

    if (partial_label[0] == 0) {
        temp_hash = utils::compute_node_hash(partial_label, hash, {}, {});
    } else {
        temp_hash = utils::compute_node_hash({}, {}, partial_label, hash);
    }

    utils::copy_bytes(temp_hash.data(), temp_hash.size(), hash_commitment.data());
    return (hash_commitment == commitment);
}

bool QueryResult::verify_vrf_proof(const VRFPublicKey &public_key) const
{
    hash_type key_hash = utils::compute_key_hash(key_);
    return public_key.verify_vrf_proof(key_hash, vrf_proof_);
}

bool QueryResult::verify(const Commitment &commitment) const
{
    bool verified = verify_lookup_path(commitment.root_commitment());
    if (use_vrf_) {
        verified = verified && verify_vrf_proof(commitment.public_key());
    }
    return verified;
}

size_t QueryResult::save(ostream &stream) const
{
    StreamSerializationWriter writer(&stream);
    return save(writer);
}

template <typename T>
size_t QueryResult::save(vector<T> &vector) const
{
    VectorSerializationWriter writer(&vector);
    return save(writer);
}

size_t QueryResult::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    auto key_data =
        fbs_builder.CreateVector(reinterpret_cast<const uint8_t *>(key().data()), key().size());
    auto payload_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(payload().data()), payload().size());
    auto randomness_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(randomness().data()), randomness().size());

    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> vrf_proof_data = 0;

    if (use_vrf_) {
        vector<byte> vrf_proof_bytes(
            utils::ECPoint::save_size + utils::ECPoint::order_size + utils::ECPoint::order_size);

        utils::copy_bytes(
            vrf_proof().gamma.data(), utils::ECPoint::save_size, vrf_proof_bytes.data());
        utils::copy_bytes(
            vrf_proof().c.data(),
            utils::ECPoint::order_size,
            vrf_proof_bytes.data() + utils::ECPoint::save_size);
        utils::copy_bytes(
            vrf_proof().s.data(),
            utils::ECPoint::order_size,
            vrf_proof_bytes.data() + utils::ECPoint::save_size + utils::ECPoint::order_size);

        vrf_proof_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(vrf_proof_bytes.data()), vrf_proof_bytes.size());
    }

    fbs::QueryResultBuilder qr_builder(fbs_builder);
    qr_builder.add_is_member(is_member());
    qr_builder.add_key(key_data);
    qr_builder.add_payload(payload_data);
    qr_builder.add_vrf_proof(vrf_proof_data);
    qr_builder.add_randomness(randomness_data);
    qr_builder.add_use_vrf(use_vrf_);
    qr_builder.add_lookup_proof_count(static_cast<uint32_t>(lookup_proof().size()));

    auto fbs_query_result = qr_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_query_result);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    // Write all lookup proof elements after the main structure
    size_t lp_size = 0;
    for (size_t idx = 0; idx < lookup_proof().size(); idx++) {
        const auto &lp = lookup_proof()[idx];
        lp_size += utils::write_path_element(lp.first, lp.second, writer);
    }

    return fbs_builder.GetSize() + lp_size;
}

pair<QueryResult, size_t> QueryResult::Load(const OZKSConfig &config, SerializationReader &reader)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedQueryResultBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load QueryResult: invalid buffer");
    }

    auto fbs_query_result = fbs::GetSizePrefixedQueryResult(in_data.data());

    QueryResult query_result(config);
    query_result.is_member_ = fbs_query_result->is_member();
    query_result.use_vrf_ = fbs_query_result->use_vrf();

    query_result.key_.resize(fbs_query_result->key()->size());
    utils::copy_bytes(
        fbs_query_result->key()->data(), fbs_query_result->key()->size(), query_result.key_.data());
    query_result.payload_.resize(fbs_query_result->payload()->size());
    utils::copy_bytes(
        fbs_query_result->payload()->data(),
        fbs_query_result->payload()->size(),
        query_result.payload_.data());

    if (fbs_query_result->randomness()->size() != query_result.randomness().size()) {
        throw runtime_error("Serialized randomness size does not match");
    }

    utils::copy_bytes(
        fbs_query_result->randomness()->data(),
        fbs_query_result->randomness()->size(),
        query_result.randomness_.data());

    query_result.lookup_proof_.resize(fbs_query_result->lookup_proof_count());

    if (query_result.use_vrf_) {
        utils::copy_bytes(
            fbs_query_result->vrf_proof()->data(),
            utils::ECPoint::save_size,
            query_result.vrf_proof_.gamma.data());
        utils::copy_bytes(
            fbs_query_result->vrf_proof()->data() + utils::ECPoint::save_size,
            utils::ECPoint::order_size,
            query_result.vrf_proof_.c.data());
        utils::copy_bytes(
            fbs_query_result->vrf_proof()->data() + utils::ECPoint::save_size +
                utils::ECPoint::order_size,
            utils::ECPoint::order_size,
            query_result.vrf_proof_.s.data());
    } else {
        memset(query_result.vrf_proof_.gamma.data(), 0, utils::ECPoint::save_size);
        memset(query_result.vrf_proof_.c.data(), 0, utils::ECPoint::order_size);
        memset(query_result.vrf_proof_.s.data(), 0, utils::ECPoint::order_size);
    }

    size_t lp_count = fbs_query_result->lookup_proof_count();
    size_t lp_size = 0;

    for (size_t idx = 0; idx < lp_count; idx++) {
        lp_size += utils::read_path_element(
            reader, query_result.lookup_proof_[idx].first, query_result.lookup_proof_[idx].second);
    }

    return { query_result, in_data.size() + lp_size };
}

pair<QueryResult, size_t> QueryResult::Load(const OZKSConfig &config, istream &stream)
{
    StreamSerializationReader reader(&stream);
    return Load(config, reader);
}

template <typename T>
pair<QueryResult, size_t> QueryResult::Load(
    const OZKSConfig &config, const vector<T> &vector, size_t position)
{
    VectorSerializationReader reader(&vector, position);
    return Load(config, reader);
}

// Explicit instantiations
template size_t QueryResult::save(vector<uint8_t> &vector) const;
template size_t QueryResult::save(vector<byte> &vector) const;
template pair<QueryResult, size_t> QueryResult::Load(
    const OZKSConfig &config, const vector<uint8_t> &vector, size_t position);
template pair<QueryResult, size_t> QueryResult::Load(
    const OZKSConfig &config, const vector<byte> &vector, size_t position);
