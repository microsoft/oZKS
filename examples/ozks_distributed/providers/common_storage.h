// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <memory>

// OZKS
#include "oZKS/storage/storage.h"

namespace ozks_distributed {
    namespace providers {
        std::shared_ptr<ozks::storage::Storage> get_common_storage();
    } // namespace providers
} // namespace ozks_distributed
