// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <stdexcept>

// oZKS
#include "oZKS/insert_result.h"
#include "oZKS/insert_result_generated.h"
#include "oZKS/path_element_generated.h"
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

        auto commitment_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(commitment().data()), commitment().size());

        fbs::InsertResultBuilder ir_builder(fbs_builder);
        ir_builder.add_commitment(commitment_data);
        ir_builder.add_append_proof_count(static_cast<uint32_t>(append_proof().size()));

        auto fbs_insert_result = ir_builder.Finish();
        fbs_builder.FinishSizePrefixed(fbs_insert_result);

        writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

        // Write all append proof elements after the main structure
        size_t ap_size = 0;
        for (size_t idx = 0; idx < append_proof().size(); idx++) {
            flatbuffers::FlatBufferBuilder f_builder;
            const auto &ap = append_proof()[idx];

            auto hash_data = f_builder.CreateVector(
                reinterpret_cast<const uint8_t *>(ap.second.data()), ap.second.size());

            vector<byte> label_bytes = utils::bools_to_bytes(ap.first);
            auto label_data = f_builder.CreateVector(
                reinterpret_cast<const uint8_t *>(label_bytes.data()), label_bytes.size());

            fbs::PathElementBuilder pe_builder(f_builder);

            pe_builder.add_hash(hash_data);
            pe_builder.add_partial_label(label_data);
            pe_builder.add_partial_label_size(static_cast<uint32_t>(ap.first.size()));

            auto fbs_append_proof = pe_builder.Finish();
            f_builder.FinishSizePrefixed(fbs_append_proof);

            writer.write(f_builder.GetBufferPointer(), f_builder.GetSize());
            ap_size += f_builder.GetSize();
        }

        return fbs_builder.GetSize() + ap_size;
    }

    size_t InsertResult::Load(InsertResult &insert_result, SerializationReader &reader)
    {
        vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

        auto verifier =
            flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
        bool safe = fbs::VerifySizePrefixedInsertResultBuffer(verifier);

        if (!safe) {
            throw runtime_error("Failed to load InsertResult: invalid InsertResult buffer");
        }

        auto fbs_insert_result = fbs::GetSizePrefixedInsertResult(in_data.data());

        commitment_type commitment(fbs_insert_result->commitment()->size());

        utils::copy_bytes(
            fbs_insert_result->commitment()->data(),
            fbs_insert_result->commitment()->size(),
            commitment.data());

        size_t ap_count = fbs_insert_result->append_proof_count();
        size_t ap_size = 0;
        append_proof_type append_proof(ap_count);

        for (size_t idx = 0; idx < ap_count; idx++) {
            vector<unsigned char> pe_data(utils::read_from_serialization_reader(reader));
            auto pe_verifier =
                flatbuffers::Verifier(reinterpret_cast<uint8_t *>(pe_data.data()), pe_data.size());
            bool pe_safe = fbs::VerifySizePrefixedPathElementBuffer(pe_verifier);
            if (!pe_safe) {
                throw runtime_error("Failed to load PathElement: invalid PathElement buffer");
            }

            auto fbs_path_element = fbs::GetSizePrefixedPathElement(pe_data.data());

            append_proof[idx].first = utils::bytes_to_bools(
                reinterpret_cast<const byte *>(fbs_path_element->partial_label()->data()),
                fbs_path_element->partial_label_size());

            // Hash size is fixed
            if (append_proof[idx].second.size() != fbs_path_element->hash()->size()) {
                throw runtime_error("Serialized hash size does not match");
            }

            utils::copy_bytes(
                fbs_path_element->hash()->data(),
                fbs_path_element->hash()->size(),
                append_proof[idx].second.data());

            ap_size += pe_data.size();
        }

        insert_result.init_result(commitment, append_proof);

        return in_data.size() + ap_size;
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
