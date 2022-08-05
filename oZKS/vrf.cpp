// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <array>
#include <stdexcept>
#include <utility>

// OZKS
#include "oZKS/utilities.h"
#include "oZKS/vrf.h"

using namespace std;
using namespace ozks;

namespace {
    template <typename... Points>
    vector<byte> append_ecpt_to_buffer(Points...);

    template <>
    vector<byte> append_ecpt_to_buffer()
    {
        return {};
    }

    template <typename First, typename... Rest>
    vector<byte> append_ecpt_to_buffer(First &&pt, Rest &&...rest)
    {
        vector<byte> result = append_ecpt_to_buffer(forward<Rest>(rest)...);
        array<byte, utils::ECPoint::save_size> pt_buf;
        pt.save(pt_buf);
        copy(pt_buf.begin(), pt_buf.end(), back_inserter(result));
        return result;
    }

    template <typename... Points>
    decltype(VRFProof::c) hash_points(Points &&...points)
    {
        // This function computes the c in the VRF proof by hashing together curve points

        // Write all points together into a single buffer
        vector<byte> hash_buf = append_ecpt_to_buffer(forward<Points>(points)...);

        // Compute by applying BLAKE2 to buffer; truncate and reduce modulo group order
        hash_type proof_hash = utils::compute_hash(hash_buf, "vrf_proof_hash");
        decltype(VRFProof::c) c;
        static_assert(sizeof(proof_hash) >= sizeof(c), "hash_type is smaller than scalar type");
        copy_n(proof_hash.begin(), c.size(), c.begin());
        utils::ECPoint::ReduceModOrder(c);

        return c;
    }
} // namespace

void VRFSecretKey::throw_if_uninitialized() const
{
    if (!is_initialized()) {
        throw logic_error("Secret key is uninitialized");
    }
}

void VRFSecretKey::initialize()
{
    // Sample a random non-zero number modulo the group order
    utils::ECPoint::MakeRandomNonzeroScalar(key_);
}

hash_type VRFSecretKey::get_hash(gsl::span<const byte> data) const
{
    throw_if_uninitialized();

    // Computes only the hash without proof
    utils::ECPoint h2c_data(data); // cofactor cleared
    h2c_data.scalar_multiply(key_, false);
    return h2c_data.extract_hash();
}

VRFProof VRFSecretKey::get_proof(gsl::span<const byte> data) const
{
    throw_if_uninitialized();

    // The proof requires the hash of the following data:
    // 1. generator
    // 2. hash-to-curve(data)
    // 3. generator multiplied by secret key (public key)
    // 4. hash-to-curve(data) multiplied by secret key
    // 5. generator multiplied by nonce
    // 6. hash-to-curve(data) multiplied by nonce

    utils::ECPoint generator = utils::ECPoint::MakeGenerator();
    utils::ECPoint h2c_data(data); // cofactor cleared
    utils::ECPoint public_key = utils::ECPoint::MakeGeneratorMultiple(key_);

    utils::ECPoint sk_times_h2c_data(h2c_data);
    sk_times_h2c_data.scalar_multiply(key_, false);

    utils::ECPoint::scalar_type nonce;
    utils::ECPoint::MakeRandomNonzeroScalar(nonce);
    utils::ECPoint nonce_times_generator = utils::ECPoint::MakeGeneratorMultiple(nonce);

    utils::ECPoint nonce_times_h2c_data(h2c_data);
    nonce_times_h2c_data.scalar_multiply(nonce, false);

    // Compute c as the hash of all of the above curve points and reduce modulo order
    decltype(VRFProof::c) c = hash_points(
        generator,
        h2c_data,
        public_key,
        sk_times_h2c_data,
        nonce_times_generator,
        nonce_times_h2c_data);

    // Next compute s=nonce-c*key mod order
    decltype(VRFProof::s) s;
    utils::ECPoint::scalar_type temp;
    utils::ECPoint::MultiplyScalar(c, key_, temp);
    utils::ECPoint::SubtractScalar(nonce, temp, s);

    // Finally save sk_times_h2c_data to gamma
    decltype(VRFProof::gamma) gamma;
    sk_times_h2c_data.save(gamma);

    // Return the proof struct
    return { gamma, c, s };
}

void VRFSecretKey::save(gsl::span<byte, save_size> out) const
{
    copy(key_.begin(), key_.end(), out.begin());
}

void VRFSecretKey::load(gsl::span<const byte, save_size> in)
{
    copy(in.begin(), in.end(), key_.begin());
    utils::ECPoint::scalar_type key_copy = key_;

    // Reduce key_copy modulo order and check it remains the same as key_
    utils::ECPoint::ReduceModOrder(key_copy);
    bool reduced = equal(key_.begin(), key_.end(), key_copy.begin());
    if (!reduced) {
        throw runtime_error("Failed to load a valid VRF secret key");
    }
}

bool VRFSecretKey::is_initialized() const
{
    // Multiple the elliptic curve generator point with the secret key scalar
    return !all_of(key_.begin(), key_.end(), [](byte b) { return b == byte{}; });
}

VRFPublicKey VRFSecretKey::get_public_key() const
{
    throw_if_uninitialized();
    return { utils::ECPoint::MakeGeneratorMultiple(key_) };
}

VRFPublicKey::VRFPublicKey(utils::ECPoint key) : key_(move(key))
{}

void VRFPublicKey::save(gsl::span<byte, save_size> out) const
{
    key_.save(out);
}

void VRFPublicKey::load(gsl::span<const byte, save_size> in)
{
    utils::ECPoint new_key;
    try {
        new_key.load(in);
    } catch (const logic_error &) {
        throw runtime_error("Failed to load a valid VRF public key");
    }
    if (!new_key.in_prime_order_subgroup()) {
        throw runtime_error("Loaded VRF public key is not in the prime-order subgroup");
    }

    key_ = new_key;
}

bool VRFPublicKey::verify_proof(
    gsl::span<const byte> data, const VRFProof &vrf_proof, hash_type &hash_out) const
{
    // First compute u=c*pk+s*generator (this should equal nonce*generator for a valid proof)
    utils::ECPoint u(key_);
    u.double_scalar_multiply(vrf_proof.c, vrf_proof.s);

    // Next compute hash-to-curve of data
    utils::ECPoint h2c_data(data); // cofactor cleared

    // Check that gamma is in the prime-order subgroup; if not, return false
    utils::ECPoint gamma_pt;
    try {
        gamma_pt.load(vrf_proof.gamma);
    } catch (const logic_error &) {
        // Loading failed; gamma is not a valid point encoding
        return false;
    }
    if (!gamma_pt.in_prime_order_subgroup()) {
        return false;
    }

    // Compute v=c*gamma+s*h2c_data (this should equal nonce*h2c_data for a valid proof)
    utils::ECPoint v(gamma_pt), temp(h2c_data);
    v.scalar_multiply(vrf_proof.c, false);
    temp.scalar_multiply(vrf_proof.s, false);
    v.add(temp);

    // Compute c_comp by hashing together all of the curve points and check that it equals c
    utils::ECPoint generator = utils::ECPoint::MakeGenerator();
    decltype(VRFProof::c) c_comp = hash_points(generator, h2c_data, key_, gamma_pt, u, v);
    if (c_comp != vrf_proof.c) {
        return false;
    }

    // The proof is correct; extract gamma_pt to hash
    hash_out = gamma_pt.extract_hash();

    return true;
}
