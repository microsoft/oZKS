// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstdint>

namespace ozks {
    /**
    Library version, packed from the CMake major, minor and patch numbers.
    */
    extern const std::uint32_t ozks_version;

    /**
    The single serialization version shared by every oZKS format: commitments, compressed
    tries, stored nodes, query results, insert results, the configuration blob, and any
    container built on them such as the example OZKS instances. Every loader rejects any
    other value.

    The version also determines the commitment-structure hashes: it is embedded in the leaf
    and node hash domains, and the hash input encoding is fixed per version. Bumping it
    therefore invalidates all persisted data and every published commitment, so a bump is a
    full migration: rebuild tries from the authoritative insertion history preserving
    insertion epochs, republish trusted commitments, regenerate proofs. Wire-only or
    storage-only changes must still bump this value; never add an unversioned field or
    format. See the README section "Serialization versioning and compatibility".
    */
    extern const std::uint32_t ozks_serialization_version;

    /**
    True if sv equals the library's serialization version.
    */
    bool same_serialization_version(std::uint32_t sv);
} // namespace ozks
