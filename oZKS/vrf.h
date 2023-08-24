// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <vector>

// OZKS
#include "oZKS/defines.h"
#include "oZKS/ecpoint.h"

// GSL
#include "gsl/span"

namespace ozks {
    /**
    The VRFProof struct contains the data for a VRF proof. It can be used to prove that a given
    hash (of type hash_type) is the output of a keyed hash function (VRF). The VRF is keyed with
    a secret key known only to the proof creator, but a corresponding public key is needed for
    the verification process. Verification also requires the data itself, i.e., the proof shows
    that a known byte string hashes to a known value.
    */
    struct VRFProof {
        std::array<std::byte, utils::ECPoint::save_size> gamma{};

        std::array<std::byte, utils::ECPoint::order_size> c{};

        std::array<std::byte, utils::ECPoint::order_size> s{};

        // Return whether the proof is valid.
        bool is_valid() const noexcept;

        // Compute the VRF value from the VRFProof.
        hash_type compute_vrf_value() const;
    };

    // Forward-declaring the VRFSecretKey class.
    class VRFSecretKey;

    /**
    This class represents a VRF public key that can be used to verify, with a VRFProof object, that
    the VRF keyed by the corresponding secret key produces a given hash value from a given input.
    The hash value itself is provided by the verification process itself.
    */
    class VRFPublicKey {
        // VRFSecretKey needs to be able to call the private constructor of VRFPublicKey
        friend class VRFSecretKey;

    public:
        VRFPublicKey() = default;

        /**
        Returns whether a given VRFProof is valid for a given input data.
        */
        bool verify_vrf_proof(const hash_type &data, const VRFProof &vrf_proof) const;

        /**
        Returns whether a given VRFProof is valid for a given input data.
        */
        bool verify_vrf_proof(const key_type &data, const VRFProof &vrf_proof) const;

        /**
        The byte-size of a buffer needed to save the VRFSecretKey object.
        */
        static constexpr std::size_t save_size = utils::ECPoint::save_size;

        /**
        Writes the VRFPublicKey to a given buffer.
        */
        void save(gsl::span<std::byte, save_size> out) const;

        /**
        Reads the VRFPublicKey from a given buffer.
        */
        void load(gsl::span<const std::byte, save_size> in);

    private:
        VRFPublicKey(utils::ECPoint key);

        utils::ECPoint key_point_;
    };

    /**
    This class represents a VRF secret key, that can be used to compute hash values for byte string
    inputs and create associated VRFProof structs that prove that the hash value actually came from
    the given byte string through the VRF. The verification process requires a public key that can
    be created from the VRFSecretKey.

    The VRFSecretKey is initially set to zero (uninitialized). It must be initialized by calling the
    member function VRFSecretKey::initialize before performing any operations. This sets the key to
    a random value.
    */
    class VRFSecretKey {
    private:
        utils::ECPoint::scalar_type key_scalar_ = {};

        VRFPublicKey pk_;

        void throw_if_uninitialized() const;

        void compute_public_key();

    public:
        /**
        Samples a new random VRF secret key.
        */
        void initialize();

        /**
        Sames a new random VRF secret key using the given seed
        */
        void initialize(gsl::span<const std::byte> seed);

        /**
        Returns a VRF public key corresponding to this secret key.
        */
        VRFPublicKey get_vrf_public_key() const;

        /**
        Computes a VRF proof (including the VRF value) from a given input.
        */
        VRFProof get_vrf_proof(const hash_type &data) const;

        /**
        Computes a VRF proof (including the VRF value) from a given input.
        */
        VRFProof get_vrf_proof(const key_type &data) const;

        /**
        Returns the VRF value (hash) for a given input. The value can also be computed
        from a VRFProof struct using utils::compute_vrf_value_hash, but computing
        the full proof is unnecessarily costly if only the value (hash) is needed.
        */
        hash_type get_vrf_value(const hash_type &data) const;

        /**
        Returns the VRF value (hash) for a given input. The value can also be computed
        from a VRFProof struct using utils::compute_vrf_value_hash, but computing
        the full proof is unnecessarily costly if only the value (hash) is needed.
        */
        hash_type get_vrf_value(const key_type &data) const;

        /**
        The byte-size of a buffer needed to save the VRFSecretKey object.
        */
        static constexpr std::size_t save_size = utils::ECPoint::order_size;

        /**
        Writes the VRFSecretKey to a given buffer.
        */
        void save(gsl::span<std::byte, save_size> out) const;

        /**
        Reads the VRFSecretKey from a given buffer.
        */
        void load(gsl::span<const std::byte, save_size> in);

        /**
        Returns whether this VRFSecretKey is initialized.
        */
        bool is_initialized() const;
    };
} // namespace ozks
