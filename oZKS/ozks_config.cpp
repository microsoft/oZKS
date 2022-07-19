// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/ozks_config.h"

using namespace std;
using namespace ozks;

size_t OZKSConfig::save(SerializationWriter &writer) const
{
    writer.write(reinterpret_cast<const uint8_t *>(&payload_randomness_), sizeof(bool));
    writer.write(reinterpret_cast<const uint8_t *>(&include_vrf_), sizeof(bool));

    return sizeof(bool) + sizeof(bool);
}

template <class T>
size_t OZKSConfig::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

size_t OZKSConfig::Load(OZKSConfig &config, SerializationReader &reader)
{
    reader.read(reinterpret_cast<uint8_t *>(&config.payload_randomness_), sizeof(bool));
    reader.read(reinterpret_cast<uint8_t *>(&config.include_vrf_), sizeof(bool));

    return sizeof(bool) + sizeof(bool);
}

template <class T>
size_t OZKSConfig::Load(OZKSConfig &config, const vector<T> &vec, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return Load(config, reader);
}

// Explicit instantiations
template size_t OZKSConfig::save(vector<uint8_t> &vec) const;
template size_t OZKSConfig::save(vector<byte> &vec) const;
template size_t OZKSConfig::Load(OZKSConfig &config, const vector<uint8_t> &vec, size_t position);
template size_t OZKSConfig::Load(OZKSConfig &config, const vector<byte> &vec, size_t position);
