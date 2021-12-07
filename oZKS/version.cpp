// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "oZKS/config.h"
#include "oZKS/version.h"

namespace ozks {
    const uint32_t ozks_version =
        (OZKS_VERSION_PATCH << 20) + (OZKS_VERSION_MINOR << 10) + OZKS_VERSION_MAJOR;

    const uint32_t ozks_serialization_version = 1;

    bool same_serialization_version(uint32_t sv)
    {
        return sv == ozks_serialization_version;
    }
} // namespace ozks
