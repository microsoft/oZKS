// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

// OZKS
#include "oZKS/serialization_helpers.h"
#include "oZKS/vrf.h"

namespace ozks {
    /**
    Represents an OZKS commitment
    */
    class Commitment {
    public:
        /**
        Construct an instance of Commitment
        */
        Commitment() = default;

        /**
        Construct an instance of Commitment
        */
        Commitment(const VRFPublicKey &public_key, const commitment_type &root_commitment)
            : public_key_(public_key), root_commitment_(root_commitment)
        {}

        /**
        VRF public key
        */
        const VRFPublicKey &public_key() const
        {
            return public_key_;
        }

        /**
        Commitment of the compressed trie root
        */
        const commitment_type &root_commitment() const
        {
            return root_commitment_;
        }

        /**
        Save commitment to a stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save commitment to a byte vector
        */
        template <typename T>
        std::size_t save(std::vector<T> &vec) const;

        /**
        Load commitment from a stream
        */
        static std::pair<Commitment, std::size_t> Load(std::istream &stream);

        /**
        Load commitment from a byte vector
        */
        template <typename T>
        static std::pair<Commitment, std::size_t> Load(
            const std::vector<T> &vec, std::size_t position = 0);

    private:
        VRFPublicKey public_key_;
        commitment_type root_commitment_{};

        std::size_t save(SerializationWriter &writer) const;
        static std::pair<Commitment, std::size_t> Load(SerializationReader &reader);
    };
} // namespace ozks
