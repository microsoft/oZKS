// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>

namespace ozks {
    namespace hash {
        // Call the chosen hash function.
        template <std::size_t sz>
        void hash(const std::byte *data, std::size_t size, std::byte *hash_out);
    } // namespace hash
} // namespace ozks
