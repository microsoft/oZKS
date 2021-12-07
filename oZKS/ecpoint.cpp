// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <functional>
#include <stdexcept>

// OZKS
#include "oZKS/ecpoint.h"
#include "oZKS/utilities.h"

// FourQ
#include "oZKS/fourq/FourQ_api.h"
#include "oZKS/fourq/FourQ_internal.h"
#include "oZKS/fourq/random.h"

using namespace std;
using namespace ozks;

namespace {
    void random_scalar(utils::ECPoint::scalar_span_type value)
    {
        if (!random_bytes(
                reinterpret_cast<unsigned char *>(value.data()),
                static_cast<unsigned int>(value.size()))) {
            throw runtime_error("Failed to get random bytes");
        }
        modulo_order(
            reinterpret_cast<digit_t *>(value.data()), reinterpret_cast<digit_t *>(value.data()));
    }

    digit_t is_nonzero_scalar(utils::ECPoint::scalar_span_type value)
    {
        const digit_t *value_ptr = reinterpret_cast<digit_t *>(value.data());
        digit_t c = 0;

        for (size_t i = 0; i < NWORDS_ORDER; i++) {
            c |= value_ptr[i];
        }

        sdigit_t first_nz = -static_cast<sdigit_t>(c & 1);
        sdigit_t rest_nz = -static_cast<sdigit_t>(c >> 1);
        return static_cast<digit_t>((first_nz | rest_nz) >> (8 * sizeof(digit_t) - 1));
    }
} // namespace

utils::ECPoint::ECPoint(input_span_const_type value)
{
    if (!value.empty()) {
        // Hash the input first
        hash_type value_hash = compute_hash(value, "ecpoint_constructor_hash");

        // Copy the hashed data to an f2elm_t struct; we truncate the hash here
        f2elm_t r;
        static_assert(sizeof(r) <= sizeof(value_hash), "f2elm cannot be larger than hash_type");
        utils::copy_bytes(value_hash.data(), sizeof(f2elm_t), r);

        // Reduce r; note that this does not produce a perfectly uniform distribution modulo
        // 2^127-1, but it is good enough.
        mod1271(r[0]);
        mod1271(r[1]);

        // Create an elliptic curve point
        HashToCurve(r, pt_);
    }
}

void utils::ECPoint::MakeRandomNonzeroScalar(scalar_span_type out)
{
    // Loop until we find a non-zero element
    do {
        random_scalar(out);
    } while (!is_nonzero_scalar(out));
}

utils::ECPoint utils::ECPoint::MakeGenerator()
{
    ECPoint result;
    eccset(result.pt_);

    return result;
}

utils::ECPoint utils::ECPoint::MakeGeneratorMultiple(scalar_span_const_type scalar)
{
    ECPoint result;

    // The ecc_mul_fixed function does not mutate the first input, even though it is not marked
    // const
    digit_t *scalar_ptr = const_cast<digit_t *>(reinterpret_cast<const digit_t *>(scalar.data()));
    ecc_mul_fixed(scalar_ptr, result.pt_);

    return result;
}

void utils::ECPoint::InvertScalar(scalar_span_const_type in, scalar_span_type out)
{
    to_Montgomery(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in.data())),
        reinterpret_cast<digit_t *>(out.data()));
    Montgomery_inversion_mod_order(
        reinterpret_cast<digit_t *>(out.data()), reinterpret_cast<digit_t *>(out.data()));
    from_Montgomery(
        reinterpret_cast<digit_t *>(out.data()), reinterpret_cast<digit_t *>(out.data()));
}

void utils::ECPoint::MultiplyScalar(
    scalar_span_const_type in1, scalar_span_const_type in2, scalar_span_type out)
{
    utils::ECPoint::scalar_type in1_mont, in2_mont;
    to_Montgomery(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in1.data())),
        reinterpret_cast<digit_t *>(in1_mont.data()));
    to_Montgomery(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in2.data())),
        reinterpret_cast<digit_t *>(in2_mont.data()));
    Montgomery_multiply_mod_order(
        reinterpret_cast<digit_t *>(in1_mont.data()),
        reinterpret_cast<digit_t *>(in2_mont.data()),
        reinterpret_cast<digit_t *>(out.data()));
    from_Montgomery(
        reinterpret_cast<digit_t *>(out.data()), reinterpret_cast<digit_t *>(out.data()));
}

void utils::ECPoint::AddScalar(
    scalar_span_const_type in1, scalar_span_const_type in2, scalar_span_type out)
{
    add_mod_order(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in1.data())),
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in2.data())),
        reinterpret_cast<digit_t *>(out.data()));
}

void utils::ECPoint::SubtractScalar(
    scalar_span_const_type in1, scalar_span_const_type in2, scalar_span_type out)
{
    subtract_mod_order(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in1.data())),
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in2.data())),
        reinterpret_cast<digit_t *>(out.data()));
}

void utils::ECPoint::ReduceModOrder(scalar_span_type scalar)
{
    modulo_order(
        reinterpret_cast<digit_t *>(scalar.data()), reinterpret_cast<digit_t *>(scalar.data()));
}

bool utils::ECPoint::scalar_multiply(scalar_span_const_type scalar, bool clear_cofactor)
{
    // The ecc_mul functions returns false when the input point is not a valid curve point
    return ecc_mul(
        pt_,
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(scalar.data())),
        pt_,
        clear_cofactor);
}

bool utils::ECPoint::double_scalar_multiply(
    scalar_span_const_type scalar1, scalar_span_const_type scalar2)
{
    // The double_ecc_mul functions returns false when the input point is not a valid curve point
    return ecc_mul_double(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(scalar2.data())),
        pt_,
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(scalar1.data())),
        pt_);
}

bool utils::ECPoint::in_prime_order_subgroup() const
{
    // Encode this point
    array<byte, save_size> encoded_pt;
    point_t pt_copy{ pt_[0] };
    encode(pt_copy, reinterpret_cast<unsigned char *>(encoded_pt.data()));

    // Clear cofactor
    point_extproj_t proj_pt;
    point_setup(pt_copy, proj_pt);
    cofactor_clearing(proj_pt);
    eccnorm(proj_pt, pt_copy);

    // Multiply by 392^(-1) mod order; this should recover the original point
    // if it was in the prime-order subgroup
    scalar_type inv_cofactor = {};
    reinterpret_cast<digit_t *>(inv_cofactor.data())[0] = 392;
    InvertScalar(inv_cofactor, inv_cofactor);
    ecc_mul(pt_copy, reinterpret_cast<digit_t *>(inv_cofactor.data()), pt_copy, false);

    // Encode cofactor-cleared point
    array<byte, save_size> encoded_pt2;
    encode(pt_copy, reinterpret_cast<unsigned char *>(encoded_pt2.data()));

    // The two encodings must match
    bool are_same = (memcmp(encoded_pt.data(), encoded_pt2.data(), save_size) == 0);
    return are_same;
}

void utils::ECPoint::add(const ECPoint &other)
{
    ECPoint other_copy(other);
    point_extproj_t this_proj_pt, other_proj_pt, sum_proj_pt;
    point_setup(pt_, this_proj_pt);
    point_setup(other_copy.pt_, other_proj_pt);

    point_extproj_precomp_t this_proj_prec_pt, other_proj_prec_pt;
    R1_to_R2(this_proj_pt, this_proj_prec_pt);
    R1_to_R3(other_proj_pt, other_proj_prec_pt);
    eccadd_core(this_proj_prec_pt, other_proj_prec_pt, sum_proj_pt);

    eccnorm(sum_proj_pt, pt_);
}

utils::ECPoint &utils::ECPoint::operator=(const ECPoint &assign)
{
    if (&assign != this) {
        pt_[0] = assign.pt_[0];
    }
    return *this;
}

void utils::ECPoint::save(ostream &stream) const
{
    auto old_ex_mask = stream.exceptions();
    stream.exceptions(ios_base::failbit | ios_base::badbit);

    try {
        array<unsigned char, save_size> buf;
        point_t pt_copy{ pt_[0] };
        encode(pt_copy, buf.data());
        stream.write(reinterpret_cast<const char *>(buf.data()), save_size);
    } catch (const ios_base::failure &) {
        stream.exceptions(old_ex_mask);
        throw;
    }
    stream.exceptions(old_ex_mask);
}

void utils::ECPoint::load(istream &stream)
{
    auto old_ex_mask = stream.exceptions();
    stream.exceptions(ios_base::failbit | ios_base::badbit);

    try {
        array<unsigned char, save_size> buf;
        stream.read(reinterpret_cast<char *>(buf.data()), save_size);
        if (decode(buf.data(), pt_) != ECCRYPTO_SUCCESS) {
            stream.exceptions(old_ex_mask);
            throw logic_error("Invalid point");
        }
    } catch (const ios_base::failure &) {
        stream.exceptions(old_ex_mask);
        throw;
    }
    stream.exceptions(old_ex_mask);
}

void utils::ECPoint::save(point_save_span_type out) const
{
    point_t pt_copy{ pt_[0] };
    encode(pt_copy, reinterpret_cast<unsigned char *>(out.data()));
}

void utils::ECPoint::load(point_save_span_const_type in)
{
    if (decode(reinterpret_cast<const unsigned char *>(in.data()), pt_) != ECCRYPTO_SUCCESS) {
        throw logic_error("Invalid point");
    }
}

hash_type utils::ECPoint::extract_hash() const
{
    // Compute a hash of the output
    return compute_hash(
        gsl::span<const byte>(reinterpret_cast<const byte *>(pt_->y), sizeof(f2elm_t)),
        "ecpoint_extraction_hash");
}
