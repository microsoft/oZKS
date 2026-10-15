// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/commitment.h"
#include "oZKS/commitment_generated.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"

using namespace std;
using namespace ozks;

size_t Commitment::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    // Hashed-label commitments carry no VRF public key; the key and its suite are then both
    // absent from the serialized form. The key table is built here, before the commitment
    // builder starts, because FlatBuffers does not allow nested table construction.
    flatbuffers::Offset<fbs::PublicKey> fbs_pk = 0;
    if (public_key_) {
        vector<byte> pk_bin = public_key_->to_bytes();
        auto public_key_data = fbs_builder.CreateVector(
            reinterpret_cast<const uint8_t *>(pk_bin.data()), pk_bin.size());
        fbs_pk = fbs::CreatePublicKey(fbs_builder, public_key_data);
    }

    fbs::RootCommitment root_commitment_data(flatbuffers::span<const uint8_t, 32>{
        reinterpret_cast<const uint8_t *>(root_commitment_.data()), root_commitment_.size() });

    fbs::CommitmentBuilder commitment_builder(fbs_builder);
    commitment_builder.add_version(ozks_serialization_version);
    commitment_builder.add_root_commitment(&root_commitment_data);
    if (public_key_) {
        commitment_builder.add_public_key(fbs_pk);
        // cppvrf also encodes the suite in the first byte of the key bytes. The explicit field
        // is a redundancy check so that a commitment can never be loaded under a suite other
        // than the one it was issued for. It uses cppvrf's own wire numbering (vrf::to_byte).
        commitment_builder.add_vrf_type(
            std::to_integer<uint8_t>(vrf::to_byte(public_key_->get_type())));
    }

    auto fbs_commitment = commitment_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_commitment);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    return fbs_builder.GetSize();
}

size_t Commitment::save(ostream &stream) const
{
    StreamSerializationWriter writer(&stream);
    return save(writer);
}

template <typename T>
size_t Commitment::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

pair<Commitment, size_t> Commitment::Load(SerializationReader &reader)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedCommitmentBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load Commitment: invalid buffer");
    }

    auto fbs_commitment = fbs::GetSizePrefixedCommitment(in_data.data());
    if (!same_serialization_version(fbs_commitment->version())) {
        throw runtime_error("Failed to load Commitment: unsupported version");
    }

    Commitment commitment;
    auto stated_type = fbs_commitment->vrf_type();
    if (fbs_commitment->public_key()) {
        gsl::span<const byte> pk_span = gsl::span(
            reinterpret_cast<const byte *>(fbs_commitment->public_key()->data()->data()),
            fbs_commitment->public_key()->data()->size());
        commitment.public_key_ = vrf::VRF::PublicKeyFromBytes(pk_span);

        if (commitment.public_key_ == nullptr) {
            throw runtime_error("Failed to load Commitment: invalid public key");
        }

        // The parsed key must belong to the suite the commitment states. The field is an
        // optional scalar so that its absence is distinguishable from any suite value.
        if (!stated_type.has_value()) {
            throw runtime_error("Failed to load Commitment: VRF type is missing");
        }
        uint8_t key_type =
            std::to_integer<uint8_t>(vrf::to_byte(commitment.public_key_->get_type()));
        if (*stated_type != key_type) {
            throw runtime_error("Failed to load Commitment: VRF type does not match public key");
        }
    } else if (stated_type.has_value()) {
        // A hashed-label commitment has neither a key nor a suite.
        throw runtime_error("Failed to load Commitment: VRF type present without public key");
    }

    // The schema marks the root commitment required, so the verifier rejects a buffer without
    // it; the check here keeps Load safe should the schema ever change.
    if (!fbs_commitment->root_commitment()) {
        throw runtime_error("Failed to load Commitment: root commitment is missing");
    }
    if (fbs_commitment->root_commitment()->data()->size() != commitment.root_commitment_.size()) {
        throw runtime_error("Serialized commitment size does not match");
    }

    utils::copy_bytes(
        fbs_commitment->root_commitment()->data()->data(),
        fbs_commitment->root_commitment()->data()->size(),
        commitment.root_commitment_.data());

    return { std::move(commitment), in_data.size() };
}

pair<Commitment, size_t> Commitment::Load(istream &stream)
{
    StreamSerializationReader reader(&stream);
    return Load(reader);
}

template <typename T>
pair<Commitment, size_t> Commitment::Load(const vector<T> &vec, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return Load(reader);
}

// Explicit specializations
template size_t Commitment::save(vector<uint8_t> &vec) const;
template size_t Commitment::save(vector<byte> &vec) const;
template pair<Commitment, size_t> Commitment::Load(const vector<uint8_t> &vec, size_t position);
template pair<Commitment, size_t> Commitment::Load(const vector<byte> &vec, size_t position);
