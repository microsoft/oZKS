// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <stdexcept>

// oZKS
#include "oZKS/insert_result.h"
#include "oZKS/insert_result_generated.h"
#include "oZKS/serialization_helpers.h"
#include "oZKS/utilities.h"

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

        if (append_proof_->size() == 0) {
            throw runtime_error("Append proof cannot be empty");
        }

        PartialLabel partial_label = (*append_proof_)[0].first;
        hash_type hash = (*append_proof_)[0].second;
        hash_type temp_hash;

        for (size_t idx = 1; idx < append_proof_->size(); idx++) {
            auto sibling = (*append_proof_)[idx];
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
        // - the commitment
        // - the last child node hash we need to get the commitment
        static_assert(sizeof(commitment_type) == sizeof(hash_type));
        commitment_type hash_commitment;
        utils::copy_bytes(hash.data(), hash.size(), hash_commitment.data());
        if (hash_commitment == *commitment_) {
            return true;
        }

        if (partial_label.bit_count() == 0) {
            throw runtime_error("partial_label should have 1 bit");
        }

        if (partial_label[0] == 0) {
            temp_hash = utils::compute_node_hash(partial_label, hash, {}, {});
        } else {
            temp_hash = utils::compute_node_hash({}, {}, partial_label, hash);
        }

        utils::copy_bytes(temp_hash.data(), temp_hash.size(), hash_commitment.data());
        return (hash_commitment == *commitment_);
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
