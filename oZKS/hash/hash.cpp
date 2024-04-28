// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/config.h"
#include "oZKS/hash/hash.h"

#ifdef OZKS_USE_OPENSSL_SHA2
#include <openssl/sha.h>
#else
#include "oZKS/hash/blake2.h"
#endif

using namespace std;
using namespace ozks;

#ifdef OZKS_USE_OPENSSL_SHA2
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
#else
template <std::size_t sz>
void hash::hash(const std::byte *data, std::size_t size, std::byte *hash_out)
{
    blake2b(hash_out, sz, data, size, nullptr, 0);
}

// Explicit instantiations
template void hash::hash<32>(const std::byte *data, std::size_t size, std::byte *hash_out);
template void hash::hash<64>(const std::byte *data, std::size_t size, std::byte *hash_out);
#endif
