// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <stdexcept>

// oZKS
#include "oZKS/insert_result.h"
#include "oZKS/insert_result_generated.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"

using namespace std;

namespace ozks {
    InsertResult::InsertResult(
        const commitment_type &commitment, const append_proof_type &append_proof)
    {
        init_result(commitment, append_proof);
    }

    bool InsertResult::verify() const
    {
        if (!initialized()) {
            throw runtime_error("This result has not been initialized");
        }

        // A malformed proof is simply not a valid proof. Validate the structure before any
        // label is indexed so that untrusted input can never make verification throw.
        const append_proof_type &proof = *append_proof_;
        if (proof.empty() || proof.size() > PartialLabel::MaxBitCount + 1 ||
            proof[0].first.empty()) {
            return false;
        }

        PartialLabel partial_label = proof[0].first;
        hash_type hash = proof[0].second;

        for (size_t idx = 1; idx < proof.size(); idx++) {
            const auto &sibling = proof[idx];
            PartialLabel common = PartialLabel::CommonPrefix(sibling.first, partial_label);
            // Siblings must both extend their common prefix, so neither label may equal or be
            // a prefix of the other.
            if (common.bit_count() >= partial_label.bit_count() ||
                common.bit_count() >= sibling.first.bit_count()) {
                return false;
            }

            // These are sibling nodes
            if (!partial_label[common.bit_count()]) {
                hash = utils::compute_node_hash(partial_label, hash, sibling.first, sibling.second);
            } else {
                hash = utils::compute_node_hash(sibling.first, sibling.second, partial_label, hash);
            }

            // Up the tree
            partial_label = common;
        }

        // If the chain stopped short of the empty root label, the last node is the root's
        // only child and the root itself still has to be hashed, with its other child
        // missing. The root is folded before comparing so that a supplied hash can never be
        // accepted as the commitment directly.
        if (!partial_label.empty()) {
            hash = partial_label[0] ? utils::compute_node_hash({}, {}, partial_label, hash)
                                    : utils::compute_node_hash(partial_label, hash, {}, {});
        }

        static_assert(sizeof(commitment_type) == sizeof(hash_type));
        commitment_type hash_commitment;
        utils::copy_bytes(hash.data(), hash.size(), hash_commitment.data());
        return hash_commitment == *commitment_;
    }

    size_t InsertResult::save(ostream &stream) const
    {
        StreamSerializationWriter writer(&stream);
        return save(writer);
    }

    template <typename T>
    size_t InsertResult::save(vector<T> &vector) const
    {
        VectorSerializationWriter writer(&vector);
        return save(writer);
    }

    size_t InsertResult::save(SerializationWriter &writer) const
    {
        if (!initialized()) {
            throw logic_error("Cannot save an uninitialized insert result");
        }

        flatbuffers::FlatBufferBuilder fbs_builder;

        auto &root_commitment = commitment();
        fbs::RootCommitment root_commitment_data(flatbuffers::span<const uint8_t, 32>{
            reinterpret_cast<const uint8_t *>(root_commitment.data()), root_commitment.size() });

        fbs::InsertResultBuilder ir_builder(fbs_builder);
        ir_builder.add_commitment(&root_commitment_data);
        ir_builder.add_append_proof_count(static_cast<uint32_t>(append_proof().size()));
        ir_builder.add_version(ozks_serialization_version);

        auto fbs_insert_result = ir_builder.Finish();
        fbs_builder.FinishSizePrefixed(fbs_insert_result);

        writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

        // Write all append proof elements after the main structure
        size_t ap_size = 0;
        for (size_t idx = 0; idx < append_proof().size(); idx++) {
            ap_size += utils::write_path_element(
                (*append_proof_)[idx].first, (*append_proof_)[idx].second, writer);
        }

        return fbs_builder.GetSize() + ap_size;
    }

    pair<InsertResult, size_t> InsertResult::Load(SerializationReader &reader)
    {
        vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

        auto verifier =
            flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
        bool safe = fbs::VerifySizePrefixedInsertResultBuffer(verifier);

        if (!safe) {
            throw runtime_error("Failed to load InsertResult: invalid InsertResult buffer");
        }

        auto fbs_insert_result = fbs::GetSizePrefixedInsertResult(in_data.data());
        // Append proofs are only meaningful under the serialization version that produced
        // them. Legacy results have no version field and read back as version 0.
        if (!same_serialization_version(fbs_insert_result->version())) {
            throw runtime_error("Failed to load InsertResult: unsupported version");
        }

        commitment_type root_commitment;
        if (fbs_insert_result->commitment()->data()->size() != root_commitment.size()) {
            throw runtime_error("Serialized commitment size does not match");
        }

        utils::copy_bytes(
            fbs_insert_result->commitment()->data()->data(),
            fbs_insert_result->commitment()->data()->size(),
            root_commitment.data());

        size_t ap_count = fbs_insert_result->append_proof_count();
        size_t ap_size = 0;
        append_proof_type append_proof(ap_count);

        for (size_t idx = 0; idx < ap_count; idx++) {
            ap_size +=
                utils::read_path_element(reader, append_proof[idx].first, append_proof[idx].second);
        }

        // InsertResult insert_result(root_commitment, append_proof);
        return { InsertResult(root_commitment, append_proof),
                 static_cast<size_t>(in_data.size() + ap_size) };
    }

    pair<InsertResult, size_t> InsertResult::Load(istream &stream)
    {
        StreamSerializationReader reader(&stream);
        return Load(reader);
    }

    template <typename T>
    pair<InsertResult, size_t> InsertResult::Load(const vector<T> &vector, size_t position)
    {
        VectorSerializationReader reader(&vector, position);
        return Load(reader);
    }

    // Explicit instantiations
    template size_t InsertResult::save(vector<uint8_t> &vector) const;
    template size_t InsertResult::save(vector<byte> &vector) const;
    template pair<InsertResult, size_t> InsertResult::Load(
        const vector<uint8_t> &vector, size_t position);
    template pair<InsertResult, size_t> InsertResult::Load(
        const vector<byte> &vector, size_t position);

} // namespace ozks
