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

bool QueryResult::is_member() const
{
    return is_member_from_path();
}

QueryVerificationResult QueryResult::verify_lookup_path(const commitment_type &commitment) const
{
    if (lookup_proof_.empty() || lookup_proof_.size() > PartialLabel::MaxBitCount + 1 ||
        (use_vrf_ && !vrf_proof_)) {
        return QueryVerificationResult::NotValid;
    }

    if (lookup_proof_[0].first.empty()) {
        // An empty trie is represented by the zero root, not an opaque arbitrary root hash.
        return lookup_proof_.size() == 1 && lookup_proof_[0].second == hash_type{} &&
                       commitment == commitment_type{}
                   ? QueryVerificationResult::ValidNotMember
                   : QueryVerificationResult::NotValid;
    }

    const PartialLabel query_label = queried_label();
    auto classify_subtree = [&query_label](const PartialLabel &label) {
        if (label == query_label) {
            return QueryVerificationResult::ValidIsMember;
        }
        // An unopened subtree can exclude the query only when its prefix diverges.
        return PartialLabel::CommonPrefixCount(label, query_label) < label.bit_count()
                   ? QueryVerificationResult::ValidNotMember
                   : QueryVerificationResult::NotValid;
    };

    PartialLabel partial_label = lookup_proof()[0].first;
    hash_type hash = lookup_proof()[0].second;
    auto result = classify_subtree(partial_label);

    for (size_t idx = 1; idx < lookup_proof().size(); idx++) {
        auto &sibling = lookup_proof()[idx];
        PartialLabel common = PartialLabel::CommonPrefix(sibling.first, partial_label);
        if (common.bit_count() >= partial_label.bit_count() ||
            common.bit_count() >= sibling.first.bit_count()) {
            return QueryVerificationResult::NotValid;
        }

        bool current_is_right = partial_label[common.bit_count()];
        if (PartialLabel::CommonPrefixCount(common, query_label) < common.bit_count()) {
            result = QueryVerificationResult::ValidNotMember;
        } else if (query_label[common.bit_count()] != current_is_right) {
            result = classify_subtree(sibling.first);
        }

        // These are sibling nodes
        if (!current_is_right) {
            hash = utils::compute_node_hash(partial_label, hash, sibling.first, sibling.second);
        } else {
            hash = utils::compute_node_hash(sibling.first, sibling.second, partial_label, hash);
        }

        // Up the tree
        partial_label = common;
    }

    if (!partial_label.empty()) {
        // The only unary node is the root; authenticate its missing child explicitly.
        if (partial_label[0] != query_label[0]) {
            result = QueryVerificationResult::ValidNotMember;
        }
        hash = partial_label[0] ? utils::compute_node_hash({}, {}, partial_label, hash)
                                : utils::compute_node_hash(partial_label, hash, {}, {});
    }

    return hash == commitment ? result : QueryVerificationResult::NotValid;
}

PartialLabel QueryResult::queried_label() const
{
    hash_type vrf_value = utils::compute_key_hash(key_);
    if (use_vrf_) {
        auto vrf_val_vec = vrf_proof_->get_vrf_value();
        utils::copy_bytes_to_hash(vrf_val_vec, vrf_value);
    }
    return PartialLabel(vrf_value);
}

bool QueryResult::is_member_from_path() const
{
    if (lookup_proof_.empty() || (use_vrf_ && !vrf_proof_)) {
        return false;
    }
    const auto label = queried_label();
    for (const auto &element : lookup_proof_) {
        if (element.first == label) {
            return true;
        }
    }
    return false;
}

bool QueryResult::verify_vrf_proof(vrf::PublicKey &public_key) const
{
    return utils::verify_vrf_proof(key_, *vrf_proof_, public_key);
}

QueryVerificationResult QueryResult::verify(const Commitment &commitment) const
{
    if (use_vrf_) {
        if (!verify_vrf_proof(*commitment.public_key())) {
            return QueryVerificationResult::NotValid;
        }
    }

    return verify_lookup_path(commitment.root_commitment());
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
        vector<byte> vrf_proof_bytes = vrf_proof_->to_bytes();
        vrf_proof_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(vrf_proof_bytes.data()), vrf_proof_bytes.size());
    }

    fbs::QueryResultBuilder qr_builder(fbs_builder);
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
        query_result.vrf_proof_ = vrf::VRF::ProofFromBytes(gsl::span<const byte>(
            reinterpret_cast<const byte *>(fbs_query_result->vrf_proof()->data()),
            fbs_query_result->vrf_proof()->size()));
    } else {
        query_result.vrf_proof_ = nullptr;
    }

    std::size_t lp_count = fbs_query_result->lookup_proof_count();
    std::size_t lp_size = 0;

    for (size_t idx = 0; idx < lp_count; idx++) {
        lp_size += utils::read_path_element(
            reader, query_result.lookup_proof_[idx].first, query_result.lookup_proof_[idx].second);
    }

    return { std::move(query_result), in_data.size() + lp_size };
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
