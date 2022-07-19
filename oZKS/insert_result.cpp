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
    bool InsertResult::verify() const
    {
        if (!initialized()) {
            throw runtime_error("This result has not been initialized");
        }

        if (append_proof_->size() == 0) {
            throw runtime_error("Append proof cannot be empty");
        }

        partial_label_type common;
        partial_label_type partial_label = (*append_proof_)[0].first;
        hash_type hash = (*append_proof_)[0].second;
        hash_type temp_hash;

        for (size_t idx = 1; idx < append_proof_->size(); idx++) {
            auto sibling = (*append_proof_)[idx];
            common = utils::get_common_prefix(sibling.first, partial_label);

            // These are sibling nodes
            if (partial_label[common.size()] == 0) {
                utils::compute_node_hash(
                    partial_label, hash, sibling.first, sibling.second, temp_hash);
            } else {
                utils::compute_node_hash(
                    sibling.first, sibling.second, partial_label, hash, temp_hash);
            }

            // Up the tree
            partial_label = common;
            hash = temp_hash;
        }

        // At the end, the resulting hash is either:
        // - the commitment
        // - the last child node hash we need to get the commitment
        commitment_type hash_commitment(hash.begin(), hash.end());
        if (hash_commitment == *commitment_) {
            return true;
        }

        if (partial_label.size() == 0) {
            throw runtime_error("partial_label should have 1 bit");
        }

        if (partial_label[0] == 0) {
            utils::compute_node_hash(partial_label, hash, {}, {}, temp_hash);
        } else {
            utils::compute_node_hash({}, {}, partial_label, hash, temp_hash);
        }

        hash_commitment = commitment_type(temp_hash.begin(), temp_hash.end());
        return (hash_commitment == *commitment_);
    }

    size_t InsertResult::save(ostream &stream) const
    {
        StreamSerializationWriter writer(&stream);
        return save(writer);
    }

    template <class T>
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

        fbs::AppendProof *append_proofs;
        auto append_proof_data = fbs_builder.CreateUninitializedVectorOfStructs<fbs::AppendProof>(
            append_proof().size(), &append_proofs);

        for (size_t idx = 0; idx < append_proof().size(); idx++) {
            auto label_bytes = utils::bools_to_bytes(append_proof()[idx].first);
            uint8_t label_array[64];
            utils::copy_bytes(label_bytes.data(), label_bytes.size(), label_array);
            uint8_t hash_array[64];

            utils::copy_bytes(
                append_proof()[idx].second.data(), append_proof()[idx].second.size(), hash_array);

            append_proofs[idx] = fbs::AppendProof(
                label_array,
                static_cast<uint32_t>(append_proof()[idx].first.size()),
                hash_array,
                static_cast<uint32_t>(hash_size));
        }

        auto commitment_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(commitment().data()), commitment().size());

        fbs::InsertResultBuilder ir_builder(fbs_builder);
        ir_builder.add_commitment(commitment_data);
        ir_builder.add_append_proof(append_proof_data);

        auto fbs_insert_result = ir_builder.Finish();
        fbs_builder.FinishSizePrefixed(fbs_insert_result);

        writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

        return fbs_builder.GetSize();
    }

    size_t InsertResult::Load(InsertResult &insert_result, SerializationReader &reader)
    {
        vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

        auto verifier =
            flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
        bool safe = fbs::VerifySizePrefixedInsertResultBuffer(verifier);

        if (!safe) {
            throw runtime_error("Failed to load InsertResult: invalid buffer");
        }

        auto fbs_insert_result = fbs::GetSizePrefixedInsertResult(in_data.data());

        commitment_type commitment(fbs_insert_result->commitment()->size());
        append_proof_type append_proof(fbs_insert_result->append_proof()->size());

        utils::copy_bytes(
            fbs_insert_result->commitment()->data(),
            fbs_insert_result->commitment()->size(),
            commitment.data());

        for (size_t idx = 0; idx < fbs_insert_result->append_proof()->size(); idx++) {
            append_proof[idx].first = utils::bytes_to_bools(
                reinterpret_cast<const byte *>(fbs_insert_result->append_proof()
                                                   ->Get(static_cast<flatbuffers::uoffset_t>(idx))
                                                   ->partial_label()
                                                   ->data()),
                fbs_insert_result->append_proof()
                    ->Get(static_cast<flatbuffers::uoffset_t>(idx))
                    ->partial_label_size());

            utils::copy_bytes(
                fbs_insert_result->append_proof()
                    ->Get(static_cast<flatbuffers::uoffset_t>(idx))
                    ->hash()
                    ->data(),
                fbs_insert_result->append_proof()
                    ->Get(static_cast<flatbuffers::uoffset_t>(idx))
                    ->hash_size(),
                append_proof[idx].second.data());
        }

        insert_result.init_result(commitment, append_proof);

        return in_data.size();
    }

    size_t InsertResult::Load(InsertResult &insert_result, istream &stream)
    {
        StreamSerializationReader reader(&stream);
        return Load(insert_result, reader);
    }

    template <class T>
    size_t InsertResult::Load(InsertResult &insert_result, const vector<T> &vector, size_t position)
    {
        VectorSerializationReader reader(&vector, position);
        return Load(insert_result, reader);
    }

    // Explicit instantiations
    template size_t InsertResult::save(vector<uint8_t> &vector) const;
    template size_t InsertResult::save(vector<byte> &vector) const;
    template size_t InsertResult::Load(
        InsertResult &query_result, const vector<uint8_t> &vector, size_t position);
    template size_t InsertResult::Load(
        InsertResult &query_result, const vector<byte> &vector, size_t position);

} // namespace ozks
