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

    partial_label_type common;
    partial_label_type partial_label = lookup_proof()[0].first;
    hash_type hash = lookup_proof()[0].second;
    hash_type temp_hash;

    for (size_t idx = 1; idx < lookup_proof().size(); idx++) {
        auto sibling = lookup_proof()[idx];
        common = utils::get_common_prefix(sibling.first, partial_label);

        if (!is_member_ && sibling.first == partial_label) {
            // In the case of non-membership, this case will happen. Verify that the hash
            // matches.
            if (hash != sibling.second) {
                return false;
            }

            continue;
        }

        // These are sibling nodes
        if (partial_label[common.size()] == 0) {
            utils::compute_node_hash(partial_label, hash, sibling.first, sibling.second, temp_hash);
        } else {
            utils::compute_node_hash(sibling.first, sibling.second, partial_label, hash, temp_hash);
        }

        // Up the tree
        partial_label = common;
        hash = temp_hash;
    }

    // At the end, the resulting hash is either:
    // - The commitment
    // - the last child node hash we need to get the commitment
    commitment_type hash_commitment(hash.begin(), hash.end());
    if (hash_commitment == commitment) {
        return true;
    }

    if (partial_label.size() == 0) {
        // This means the current hash should have matched the commitment, but it didn't
        return false;
    }

    if (partial_label[0] == 0) {
        utils::compute_node_hash(partial_label, hash, {}, {}, temp_hash);
    } else {
        utils::compute_node_hash({}, {}, partial_label, hash, temp_hash);
    }

    hash_commitment = commitment_type(temp_hash.begin(), temp_hash.end());
    return (hash_commitment == commitment);
}

bool QueryResult::verify_vrf_proof(const key_type &key, const VRFPublicKey &public_key) const
{
    hash_type hash;
    return public_key.verify_proof(key, vrf_proof(), hash);
}

bool QueryResult::verify(const key_type &key, const Commitment &commitment) const
{
    bool verified = verify_lookup_path(commitment.root_commitment());
    if (include_vrf_) {
        verified = verified && verify_vrf_proof(key, commitment.public_key());
    }
    return verified;
}

size_t QueryResult::save(ostream &stream) const
{
    StreamSerializationWriter writer(&stream);
    return save(writer);
}

template <class T>
size_t QueryResult::save(vector<T> &vector) const
{
    VectorSerializationWriter writer(&vector);
    return save(writer);
}

size_t QueryResult::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    auto payload_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(payload().data()), payload().size());

    fbs::LookupProof *lookup_proofs;
    auto lookup_proof_data = fbs_builder.CreateUninitializedVectorOfStructs<fbs::LookupProof>(
        lookup_proof().size(), &lookup_proofs);

    for (size_t idx = 0; idx < lookup_proof().size(); idx++) {
        auto label_bytes = utils::bools_to_bytes(lookup_proof()[idx].first);
        uint8_t label_array[64];
        utils::copy_bytes(label_bytes.data(), label_bytes.size(), label_array);
        uint8_t hash_array[64];

        utils::copy_bytes(
            lookup_proof()[idx].second.data(), lookup_proof()[idx].second.size(), hash_array);

        lookup_proofs[idx] = fbs::LookupProof(
            label_array,
            static_cast<uint32_t>(lookup_proof()[idx].first.size()),
            hash_array,
            static_cast<uint32_t>(hash_size));
    }

    auto randomness_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(randomness().data()), randomness().size());

    flatbuffers::Offset<flatbuffers::Vector<uint8_t>> vrf_proof_data = 0;

    if (include_vrf_) {
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
    qr_builder.add_payload(payload_data);
    qr_builder.add_lookup_proof(lookup_proof_data);
    qr_builder.add_vrf_proof(vrf_proof_data);
    qr_builder.add_randomness(randomness_data);
    qr_builder.add_include_vrf(include_vrf_);

    auto fbs_query_result = qr_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_query_result);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    return fbs_builder.GetSize();
}

size_t QueryResult::Load(QueryResult &query_result, SerializationReader &reader)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedQueryResultBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load QueryResult: invalid buffer");
    }

    auto fbs_query_result = fbs::GetSizePrefixedQueryResult(in_data.data());

    query_result.is_member_ = fbs_query_result->is_member();
    query_result.include_vrf_ = fbs_query_result->include_vrf();

    query_result.payload_.resize(fbs_query_result->payload()->size());
    utils::copy_bytes(
        fbs_query_result->payload()->data(),
        fbs_query_result->payload()->size(),
        query_result.payload_.data());
    query_result.randomness_.resize(fbs_query_result->randomness()->size());
    utils::copy_bytes(
        fbs_query_result->randomness()->data(),
        fbs_query_result->randomness()->size(),
        query_result.randomness_.data());

    query_result.lookup_proof_.resize(fbs_query_result->lookup_proof()->size());
    for (size_t idx = 0; idx < fbs_query_result->lookup_proof()->size(); idx++) {
        query_result.lookup_proof_[idx].first = utils::bytes_to_bools(
            reinterpret_cast<const byte *>(fbs_query_result->lookup_proof()
                                               ->Get(static_cast<flatbuffers::uoffset_t>(idx))
                                               ->partial_label()
                                               ->data()),
            fbs_query_result->lookup_proof()
                ->Get(static_cast<flatbuffers::uoffset_t>(idx))
                ->partial_label_size());

        utils::copy_bytes(
            fbs_query_result->lookup_proof()
                ->Get(static_cast<flatbuffers::uoffset_t>(idx))
                ->hash()
                ->data(),
            fbs_query_result->lookup_proof()
                ->Get(static_cast<flatbuffers::uoffset_t>(idx))
                ->hash_size(),
            query_result.lookup_proof_[idx].second.data());
    }

    if (query_result.include_vrf_) {
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

    return in_data.size();
}

size_t QueryResult::Load(QueryResult &query_result, istream &stream)
{
    StreamSerializationReader reader(&stream);
    return Load(query_result, reader);
}

template <class T>
size_t QueryResult::Load(QueryResult &query_result, const vector<T> &vector, size_t position)
{
    VectorSerializationReader reader(&vector, position);
    return Load(query_result, reader);
}

// Explicit instantiations
template size_t QueryResult::save(vector<uint8_t> &vector) const;
template size_t QueryResult::save(vector<byte> &vector) const;
template size_t QueryResult::Load(
    QueryResult &query_result, const vector<uint8_t> &vector, size_t position);
template size_t QueryResult::Load(
    QueryResult &query_result, const vector<byte> &vector, size_t position);
