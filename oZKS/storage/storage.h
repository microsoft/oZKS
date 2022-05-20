// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD

// OZKS
#include "oZKS/ct_node.h"

namespace ozks {
    namespace storage {
        class Storage {
            /**
            Get a node from storage
            */
            std::size_t LoadCTNode(CTNode &node);

            /**
            Save a node to storage
            */
            std::size_t SaveCTNode(const CTNode &node);
        };
    }
}