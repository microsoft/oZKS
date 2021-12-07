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
    that a known byte string hashes to a known value. One particular curiosity about this kind of
    VRF is that, in fact, the hash can be derived from the proof and does not need to be sent
    separately at all. Instead, our verification algorithm will output the hash to the verifier.
    */
    struct VRFProof {
        std::array<std::byte, utils::ECPoint::save_size> gamma;

        std::array<std::byte, utils::ECPoint::order_size> c;

        std::array<std::byte, utils::ECPoint::order_size> s;
    };

    // Forward-declaring the VRFPublicKey class.
    class VRFPublicKey;

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
        utils::ECPoint::scalar_type key_ = {};

        void throw_if_uninitialized() const;

    public:
        /**
        Samples a new random VRF secret key.
        */
        void initialize();

        /**
        Returns a VRF public key corresponding to this secret key.
        */
        VRFPublicKey get_public_key() const;

        /**
        Returns the hash value corresponding to a particular input without proof.
        */
        hash_type get_hash(gsl::span<const std::byte> data) const;

        /**
        Returns the proof that a specific hash value comes from a particular input. However, this
        function does not return the hash value itself. Instead, the verifier can obtain the hash
        value from the verification algorithm itself.
        */
        VRFProof get_proof(gsl::span<const std::byte> data) const;

        /**
        The byte-size of a buffer needed to save the VRFSecretKey object.
        */
        static constexpr std::size_t save_size = sizeof(key_);

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

    /**
    This class represents a VRF public key that can be used to verify, with a VRFProof object, that
    the VRF keyed by the corresponding secret key produces a given hash value from a given input.
    The hash value itself is provided by the verification process itself.
    */
    class VRFPublicKey {
        friend class VRFSecretKey;

    public:
        VRFPublicKey() = default;

        /**
        Returns whether a given VRFProof is valid for a given input data and extracts the hash from
        the proof.
        */
        bool verify_proof(
            gsl::span<const std::byte> data, const VRFProof &vrf_proof, hash_type &hash_out) const;

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

        utils::ECPoint key_;
    };
} // namespace ozks
