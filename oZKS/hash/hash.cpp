// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/hash/hash.h"

#include <openssl/sha.h>

using namespace std;
using namespace ozks;

template <>
void hash::hash<SHA256_DIGEST_LENGTH>(const std::byte *data, std::size_t size, std::byte *hash_out)
{
    SHA256(
        reinterpret_cast<const unsigned char *>(data),
        size,
        reinterpret_cast<unsigned char *>(hash_out));
}

template <>
void hash::hash<SHA512_DIGEST_LENGTH>(const std::byte *data, std::size_t size, std::byte *hash_out)
{
    SHA512(
        reinterpret_cast<const unsigned char *>(data),
        size,
        reinterpret_cast<unsigned char *>(hash_out));
}
