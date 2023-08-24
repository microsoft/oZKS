// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <utility>

// OZKS
#include "oZKS/utilities.h"
#include "oZKS/vrf.h"

using namespace std;
using namespace ozks;

namespace {
    template <size_t Size, typename... Points>
    void append_ecpt_to_buffer(gsl::span<byte, Size> out, Points &&...);

    template <>
    void append_ecpt_to_buffer(gsl::span<byte, 0> out)
    {}

    template <size_t Size, typename First, typename... Rest>
    void append_ecpt_to_buffer(gsl::span<byte, Size> out, First &&pt, Rest &&... rest)
    {
        constexpr size_t subspan_offset = utils::ECPoint::save_size;
        constexpr size_t subspan_size = sizeof...(Rest) * utils::ECPoint::save_size;
        append_ecpt_to_buffer(
            out.template subspan<subspan_offset, subspan_size>(), std::forward<Rest>(rest)...);
        pt.save(out.template subspan<0, utils::ECPoint::save_size>());
    }

    template <typename... Points>
    decltype(VRFProof::c) make_challenge(Points &&... points)
    {
        // This function computes the c in the VRF proof by hashing together curve points

        constexpr byte domain_separator_front{ 0x02 };
        constexpr byte domain_separator_back{ 0x00 };

        // Compute lengths of the different components of the challenge (before hashing)
        constexpr size_t curve_descriptor_size =
            char_traits<char>::length(utils::ECPoint::curve_descriptor);
        constexpr size_t points_size = sizeof...(Points) * utils::ECPoint::save_size;

        // Compute the positions in the buffer
        constexpr size_t curve_descriptor_start = 0;
        constexpr size_t domain_separator_front_start =
            curve_descriptor_start + curve_descriptor_size;
        constexpr size_t points_start =
            domain_separator_front_start + sizeof(domain_separator_front);
        constexpr size_t domain_separator_back_start = points_start + points_size;
        constexpr size_t hash_buf_size =
            domain_separator_back_start + sizeof(domain_separator_back);

        // Create the input buffer to the hash function
        array<byte, hash_buf_size> hash_buf{};
        copy_n(
            reinterpret_cast<const byte *>(utils::ECPoint::curve_descriptor),
            curve_descriptor_size,
            hash_buf.begin() + curve_descriptor_start);
        copy_n(
            &domain_separator_front,
            sizeof(domain_separator_front),
            hash_buf.begin() + domain_separator_front_start);
        gsl::span<byte, points_size> points_span =
            gsl::span(hash_buf).template subspan<points_start, points_size>();
        append_ecpt_to_buffer(points_span, std::forward<Points>(points)...);
        copy_n(
            &domain_separator_back,
            sizeof(domain_separator_back),
            hash_buf.begin() + domain_separator_back_start);

        // Compute by applying a hash function to buffer; reduce modulo group order
        hash_type challenge_hash = utils::compute_hash(hash_buf);
        utils::ECPoint::ReduceModOrder(challenge_hash);
        decltype(VRFProof::c) c{};
        copy_n(challenge_hash.begin(), utils::ECPoint::order_size, c.begin());

        return c;
    }

    utils::ECPoint::scalar_type make_nonce(
        const utils::ECPoint &h2c_data, const utils::ECPoint::scalar_type &key_scalar)
    {
        // This function computes a nonce for the VRF proof

        constexpr size_t point_size = utils::ECPoint::save_size;

        // First hash the secret key to 32-byte buffer
        array<byte, utils::ECPoint::order_size> key_data{};
        array<byte, 64> key_hash{};
        key_scalar.save(key_data);
        utils::compute_hash(gsl::span<const byte>(key_data), gsl::span<byte>(key_hash));

        // Next build the input for the nonce hash
        array<byte, 32 + point_size> nonce_buf{};
        copy_n(key_hash.begin() + 32, 32, nonce_buf.begin());
        gsl::span<byte, point_size> point_span =
            gsl::span(nonce_buf).template subspan<32, point_size>();
        append_ecpt_to_buffer(point_span, h2c_data);

        // To get the nonce, hash one more time and reduce modulo the group order
        hash_type nonce = utils::compute_hash(nonce_buf);
        utils::ECPoint::ReduceModOrder(nonce);
        utils::ECPoint::scalar_type nonce_s{};
        nonce_s.load(nonce);

        return nonce_s;
    }
} // namespace

bool VRFProof::is_valid() const noexcept
{
    // The scalars are always valid. At worst, they are not yet reduced modulo
    // the group order, but that has no impact on security.
    // utils::ECPoint::scalar_type scalar_c(c);
    // utils::ECPoint::scalar_type scalar_s(s);

    // The real problem is if the point gamma is not in the valid subgroup.
    utils::ECPoint gamma_pt;
    try {
        gamma_pt.load(gamma);
    } catch (const logic_error &) {
        // Loading failed; gamma is not a valid point encoding
        return false;
    }
    if (!gamma_pt.in_prime_order_subgroup()) {
        return false;
    }

    return true;
}

hash_type VRFProof::compute_vrf_value() const
{
    constexpr byte domain_separator_front{ 0x03 };
    constexpr byte domain_separator_back{ 0x00 };

    // Compute lengths of the different components of the challenge (before hashing)
    constexpr size_t curve_descriptor_size =
        char_traits<char>::length(utils::ECPoint::curve_descriptor);
    constexpr size_t point_size = utils::ECPoint::save_size;

    // Compute the positions in the buffer
    constexpr size_t curve_descriptor_start = 0;
    constexpr size_t domain_separator_front_start = curve_descriptor_start + curve_descriptor_size;
    constexpr size_t point_start = domain_separator_front_start + sizeof(domain_separator_front);
    constexpr size_t domain_separator_back_start = point_start + point_size;
    constexpr size_t hash_buf_size = domain_separator_back_start + sizeof(domain_separator_back);

    // Create the input buffer to the hash function
    array<byte, hash_buf_size> hash_buf{};
    copy_n(
        reinterpret_cast<const byte *>(utils::ECPoint::curve_descriptor),
        curve_descriptor_size,
        hash_buf.begin() + curve_descriptor_start);
    copy_n(
        &domain_separator_front,
        sizeof(domain_separator_front),
        hash_buf.begin() + domain_separator_front_start);
    gsl::span<byte, point_size> point_span = gsl::span(hash_buf).subspan<point_start, point_size>();
    copy_n(gamma.begin(), point_size, point_span.begin());
    copy_n(
        &domain_separator_back,
        sizeof(domain_separator_back),
        hash_buf.begin() + domain_separator_back_start);

    // Compute VRF value by applying a hash function to buffer.
    return utils::compute_hash(hash_buf);
}

void VRFSecretKey::throw_if_uninitialized() const
{
    if (!is_initialized()) {
        throw logic_error("Secret key is uninitialized");
    }
}

void VRFSecretKey::compute_public_key()
{
    // Compute the VRF public key so we have it easily available
    pk_ = utils::ECPoint::MakeGeneratorMultiple(key_scalar_);
}

void VRFSecretKey::initialize()
{
    utils::ECPoint::MakeRandomNonzeroScalar(key_scalar_);
    compute_public_key();
}

void VRFSecretKey::initialize(gsl::span<const byte> seed)
{
    utils::ECPoint::MakeSeededScalar(seed, key_scalar_);
    compute_public_key();
}

VRFProof VRFSecretKey::get_vrf_proof(const hash_type &data) const
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

    utils::ECPoint sk_times_h2c_data(h2c_data);
    sk_times_h2c_data.scalar_multiply(key_scalar_, false);

    utils::ECPoint::scalar_type nonce = make_nonce(h2c_data, key_scalar_);
    utils::ECPoint nonce_times_generator = utils::ECPoint::MakeGeneratorMultiple(nonce);

    utils::ECPoint nonce_times_h2c_data(h2c_data);
    nonce_times_h2c_data.scalar_multiply(nonce, false);

    // Compute c as the hash of all of the above curve points and reduce modulo order
    decltype(VRFProof::c) c = make_challenge(
        pk_.key_point_, h2c_data, sk_times_h2c_data, nonce_times_generator, nonce_times_h2c_data);

    // Next compute s=c*key-nonce mod order
    utils::ECPoint::scalar_type temp;
    utils::ECPoint::MultiplyScalar(utils::ECPoint::scalar_type(c), key_scalar_, temp);
    utils::ECPoint::SubtractScalar(nonce, temp, temp);

    // Save temp as s
    decltype(VRFProof::s) s{};
    temp.save(s);

    // Finally save sk_times_h2c_data to gamma
    decltype(VRFProof::gamma) gamma{};
    sk_times_h2c_data.save(gamma);

    // Return the proof struct
    return { gamma, c, s };
}

VRFProof VRFSecretKey::get_vrf_proof(const key_type &data) const
{
    hash_type data_hash = utils::compute_key_hash(data);
    return get_vrf_proof(data_hash);
}

hash_type VRFSecretKey::get_vrf_value(const hash_type &data) const
{
    throw_if_uninitialized();

    utils::ECPoint sk_times_h2c_data(data);
    sk_times_h2c_data.scalar_multiply(key_scalar_, false);

    // We write the VRF value in a VRFProof struct, then extract the hash
    VRFProof vrf_proof;
    sk_times_h2c_data.save(vrf_proof.gamma);
    return vrf_proof.compute_vrf_value();
}

hash_type VRFSecretKey::get_vrf_value(const key_type &data) const
{
    hash_type data_hash = utils::compute_key_hash(data);
    return get_vrf_value(data_hash);
}

void VRFSecretKey::save(gsl::span<byte, save_size> out) const
{
    key_scalar_.save(out);
}

void VRFSecretKey::load(gsl::span<const byte, save_size> in)
{
    VRFSecretKey new_key;
    new_key.key_scalar_.load(in);

    // Did we load a non-zero secret key? In that case compute the public key
    if (new_key.is_initialized()) {
        new_key.compute_public_key();
    }

    // Done with loading
    swap(*this, new_key);
}

bool VRFSecretKey::is_initialized() const
{
    return !key_scalar_.is_zero();
}

VRFPublicKey VRFSecretKey::get_vrf_public_key() const
{
    throw_if_uninitialized();
    return pk_;
}

VRFPublicKey::VRFPublicKey(utils::ECPoint key) : key_point_(std::move(key))
{}

void VRFPublicKey::save(gsl::span<byte, save_size> out) const
{
    key_point_.save(out);
}

void VRFPublicKey::load(gsl::span<const byte, save_size> in)
{
    utils::ECPoint new_key_point;
    try {
        new_key_point.load(in);
    } catch (const logic_error &) {
        throw runtime_error("Failed to load a valid VRF public key");
    }
    if (!new_key_point.in_prime_order_subgroup()) {
        throw runtime_error("Loaded VRF public key is not in the prime-order subgroup");
    }

    key_point_ = std::move(new_key_point);
}

bool VRFPublicKey::verify_vrf_proof(const hash_type &data, const VRFProof &vrf_proof) const
{
    // Verify that the given VRFProof is valid
    if (!vrf_proof.is_valid()) {
        return false;
    }

    // Compute u=c*pk+s*generator (this should equal nonce*generator for a valid proof)
    utils::ECPoint u(key_point_);
    utils::ECPoint::scalar_type scalar_c(vrf_proof.c);
    utils::ECPoint::scalar_type scalar_s(vrf_proof.s);
    u.double_scalar_multiply(scalar_c, scalar_s);

    // Next compute hash-to-curve of data
    utils::ECPoint h2c_data(data); // cofactor cleared

    // Load gamma. We already know this will succeed from checking validity above.
    utils::ECPoint gamma_pt;
    gamma_pt.load(vrf_proof.gamma);

    // Compute v=c*gamma+s*h2c_data (this should equal nonce*h2c_data for a valid proof)
    utils::ECPoint v(gamma_pt), temp(h2c_data);
    v.scalar_multiply(scalar_c, false);
    temp.scalar_multiply(scalar_s, false);
    v.add(temp);

    // Compute c_comp by hashing together all of the curve points and check that it equals c
    utils::ECPoint generator = utils::ECPoint::MakeGenerator();
    decltype(VRFProof::c) c_comp = make_challenge(key_point_, h2c_data, gamma_pt, u, v);
    if (c_comp != vrf_proof.c) {
        return false;
    }

    return true;
}

bool VRFPublicKey::verify_vrf_proof(const key_type &data, const VRFProof &vrf_proof) const
{
    hash_type data_hash = utils::compute_key_hash(data);
    return verify_vrf_proof(data_hash, vrf_proof);
}
