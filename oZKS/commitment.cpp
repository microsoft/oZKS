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

    uint8_t pk_bin[VRFPublicKey::save_size];
    public_key_.save(gsl::span<byte, VRFPublicKey::save_size>(
        reinterpret_cast<byte *>(pk_bin), VRFPublicKey::save_size));

    auto public_key_data = fbs_builder.CreateVector(pk_bin, VRFPublicKey::save_size);
    auto fbs_pk = fbs::CreatePublicKey(fbs_builder, public_key_data);

    auto root_commitment_data = fbs_builder.CreateVector(
        reinterpret_cast<const uint8_t *>(root_commitment_.data()), root_commitment_.size());
    auto fbs_rc = fbs::CreateRootCommitment(fbs_builder, root_commitment_data);

    fbs::CommitmentBuilder commitment_builder(fbs_builder);
    commitment_builder.add_version(ozks_serialization_version);
    commitment_builder.add_public_key(fbs_pk);
    commitment_builder.add_root_commitment(fbs_rc);

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

template <class T>
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

    if (fbs_commitment->public_key()->data()->size() != VRFPublicKey::save_size) {
        throw runtime_error("VRF public key size does not match.");
    }

    Commitment commitment;
    commitment.public_key_.load(gsl::span<const byte, VRFPublicKey::save_size>(
        reinterpret_cast<const byte *>(fbs_commitment->public_key()->data()->data()),
        VRFPublicKey::save_size));

    if (fbs_commitment->root_commitment()->data()->size() != commitment.root_commitment_.size()) {
        throw runtime_error("Serialized commitment size does not match");
    }

    utils::copy_bytes(
        fbs_commitment->root_commitment()->data()->data(),
        fbs_commitment->root_commitment()->data()->size(),
        commitment.root_commitment_.data());

    return { commitment, in_data.size() };
}

pair<Commitment, size_t> Commitment::Load(istream &stream)
{
    StreamSerializationReader reader(&stream);
    return Load(reader);
}

template <class T>
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
