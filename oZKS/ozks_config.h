// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <iostream>
#include <vector>

// OZKS
#include "oZKS/serialization_helpers.h"

namespace ozks {
    class OZKSConfig {
    public:
        /**
        Construct an instance of OZKSConfig
        */
        OZKSConfig() = default;

        /**
        Construct an instance of OZKSConfig
        */
        OZKSConfig(bool payload_randomness, bool include_vrf)
            : payload_randomness_(payload_randomness), include_vrf_(include_vrf)
        {}

        /**
        Whether to compute payload randomness when inserting
        */
        bool payload_randomness() const
        {
            return payload_randomness_;
        }

        /**
        Whether to include VRF
        */
        bool include_vrf() const
        {
            return include_vrf_;
        }

        /**
        Save the current OZKSConfig object to the given serialization writer
        */
        std::size_t save(SerializationWriter &writer) const;

        /**
        Save the current OZKSConfig instance to a byte vector
        */
        template <class T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Load an OZKSConfig object from the given serialization reader
        */
        static std::size_t Load(OZKSConfig &config, SerializationReader &reader);

        /**
        Load an oZKS instance from a byte vector
        */
        template <class T>
        static std::size_t Load(
            OZKSConfig &config, const std::vector<T> &vec, std::size_t position = 0);

    private:
        bool payload_randomness_ = true;
        bool include_vrf_ = true;
    };
} // namespace ozks
