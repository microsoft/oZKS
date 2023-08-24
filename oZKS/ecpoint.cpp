// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <functional>
#include <stdexcept>

// OZKS
#include "oZKS/ecpoint.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;

#ifdef OZKS_USE_OPENSSL_P256

#include <memory>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>

namespace {
    struct EC_GROUP_wrapper {
        EC_GROUP_wrapper() : ec_group(EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1))
        {}

        ~EC_GROUP_wrapper()
        {
            EC_GROUP_free(ec_group);
        }

        EC_GROUP *ec_group = nullptr;
    };

    EC_GROUP *get_ec_group()
    {
        static EC_GROUP_wrapper gw;
        return gw.ec_group;
    }

    class BIGNUM_guard {
    public:
        BIGNUM_guard()
        {
            bn_ = BN_secure_new();
            if (!bn_) {
                throw runtime_error("Failed to create BIGNUM");
            }
        }

        ~BIGNUM_guard()
        {
            BN_clear_free(bn_);
            bn_ = nullptr;
        }

        BIGNUM *get()
        {
            return bn_;
        }

        const BIGNUM *get() const
        {
            return bn_;
        }

        BIGNUM *release()
        {
            BIGNUM *ret = bn_;
            bn_ = nullptr;
            return ret;
        }

    private:
        BIGNUM *bn_;
    };

    class BN_CTX_guard {
    public:
        BN_CTX_guard()
        {
            bn_ctx_ = BN_CTX_secure_new();
            if (!bn_ctx_) {
                throw runtime_error("Failed to create BN_CTX");
            }
        }

        ~BN_CTX_guard()
        {
            BN_CTX_free(bn_ctx_);
            bn_ctx_ = nullptr;
        }

        BN_CTX *get()
        {
            return bn_ctx_;
        }

        const BN_CTX *get() const
        {
            return bn_ctx_;
        }

        BN_CTX *release()
        {
            BN_CTX *ret = bn_ctx_;
            bn_ctx_ = nullptr;
            return ret;
        }

    private:
        BN_CTX *bn_ctx_;
    };

    template <typename T>
    void zero_span(T sp)
    {
        fill(sp.begin(), sp.end(), typename T::value_type{ 0 });
    }

    void reduce_mod_group_order(BIGNUM *value)
    {
        const BIGNUM *order = EC_GROUP_get0_order(get_ec_group());

        // BN_mod requires an allocated BN_CTX
        BN_CTX_guard bcg;
        BIGNUM_guard bn_temp;
        if (1 != BN_mod(bn_temp.get(), value, order, bcg.get())) {
            throw runtime_error("Call to BN_mod failed");
        }
        if (!BN_copy(value, bn_temp.get())) {
            throw runtime_error("Call to BN_copy failed");
        }
    }

    void reduce_mod_p256(BIGNUM *value)
    {
        // BN_nist_mod_256 requires an allocated BN_CTX
        BN_CTX_guard bcg;
        BIGNUM_guard bn_temp;
        if (1 != BN_nist_mod_256(bn_temp.get(), value, nullptr, bcg.get())) {
            throw runtime_error("Call to BN_nist_mod_256 failed");
        }
        if (!BN_copy(value, bn_temp.get())) {
            throw runtime_error("Call to BN_copy failed");
        }
    }

    void random_scalar(utils::P256Point::scalar_type &value)
    {
        BIGNUM *value_bn = reinterpret_cast<BIGNUM *>(value.ptr());
        int bit_count = 2 * 8 * static_cast<int>(value.size()) - 1;
        if (1 != BN_priv_rand_ex(
                     value_bn, bit_count, BN_RAND_TOP_ANY, BN_RAND_BOTTOM_ANY, 256, nullptr)) {
            throw runtime_error("Call to BN_priv_rand_ex failed");
        }

        reduce_mod_group_order(value_bn);
    }

    uint64_t is_nonzero_scalar(const utils::P256Point::scalar_type &value)
    {
        return 1 != BN_is_zero(reinterpret_cast<const BIGNUM *>(value.ptr()));
    }
} // namespace

void utils::P256Point::scalar_type::clean_up()
{
    if (scalar_) {
        BN_clear_free(reinterpret_cast<BIGNUM *>(scalar_));
        scalar_ = nullptr;
    }
}

utils::P256Point::scalar_type::scalar_type()
{
    scalar_ = BN_secure_new();
    if (!scalar_) {
        throw runtime_error("Failed to create BIGNUM");
    }
}

utils::P256Point::scalar_type::scalar_type(void *scalar_ptr)
{
    if (scalar_ptr != scalar_) {
        clean_up();
        scalar_ = scalar_ptr;
    }
}

utils::P256Point::scalar_type::scalar_type(scalar_span_const_type in) : scalar_type()
{
    load(in);
}

utils::P256Point::scalar_type::~scalar_type()
{
    clean_up();
}

auto utils::P256Point::scalar_type::operator=(const scalar_type &assign) -> scalar_type &
{
    BIGNUM *bn_this = reinterpret_cast<BIGNUM *>(scalar_);
    const BIGNUM *bn_assign = reinterpret_cast<const BIGNUM *>(assign.scalar_);
    if (&assign != this) {
        if (!BN_copy(bn_this, bn_assign)) {
            throw runtime_error("Call to BN_copy failed");
        }
    }
    return *this;
}

auto utils::P256Point::scalar_type::scalar_type::operator=(scalar_type &&source) noexcept
    -> scalar_type &
{
    if (&source != this) {
        swap(scalar_, source.scalar_);
    }
    return *this;
}

bool utils::P256Point::scalar_type::scalar_type::operator==(const scalar_type &other) const
{
    BIGNUM *bn_this = reinterpret_cast<BIGNUM *>(scalar_);
    const BIGNUM *bn_other = reinterpret_cast<const BIGNUM *>(other.scalar_);
    return 0 == BN_cmp(bn_this, bn_other);
}

void utils::P256Point::scalar_type::scalar_type::set_zero()
{
    BN_zero_ex(reinterpret_cast<BIGNUM *>(scalar_));
}

template <typename T>
void utils::P256Point::scalar_type::scalar_type::set_byte(size_t index, T value)
{
    if (index >= order_size) {
        throw out_of_range("Byte index is out of range");
    }

    // Read the current value into an array
    array<byte, order_size> value_bytes;
    if (order_size != BN_bn2lebinpad(
                          reinterpret_cast<const BIGNUM *>(scalar_),
                          reinterpret_cast<unsigned char *>(value_bytes.data()),
                          order_size)) {
        throw runtime_error("Call to BN_bn2lebinpad failed");
    }

    // Set the appropriate byte
    value_bytes[index] = static_cast<byte>(value);

    // Read back the bytes
    if (!BN_lebin2bn(
            reinterpret_cast<const unsigned char *>(value_bytes.data()),
            order_size,
            reinterpret_cast<BIGNUM *>(scalar_))) {
        throw runtime_error("Call to BN_lebin2bn failed");
    }
}

template void utils::P256Point::scalar_type::scalar_type::set_byte(size_t index, byte value);

template void utils::P256Point::scalar_type::scalar_type::set_byte(
    size_t index, unsigned char value);

template void utils::P256Point::scalar_type::scalar_type::set_byte(size_t index, int32_t value);

template void utils::P256Point::scalar_type::scalar_type::set_byte(size_t index, uint32_t value);

template void utils::P256Point::scalar_type::scalar_type::set_byte(size_t index, int64_t value);

template void utils::P256Point::scalar_type::scalar_type::set_byte(size_t index, uint64_t value);

bool utils::P256Point::scalar_type::is_zero() const
{
    return 1 == BN_is_zero(reinterpret_cast<const BIGNUM *>(scalar_));
}

void utils::P256Point::scalar_type::load(scalar_span_const_type in)
{
    BIGNUM *bn_scalar = reinterpret_cast<BIGNUM *>(scalar_);

    if (!BN_lebin2bn(
            reinterpret_cast<const unsigned char *>(in.data()),
            static_cast<int>(in.size()),
            bn_scalar)) {
        throw runtime_error("Call to BN_lebin2bn failed");
    }
}

void utils::P256Point::scalar_type::save(scalar_span_type out) const
{
    if (order_size != BN_bn2lebinpad(
                          reinterpret_cast<const BIGNUM *>(scalar_),
                          reinterpret_cast<unsigned char *>(out.data()),
                          order_size)) {
        throw runtime_error("Call to BN_bn2lebinpad failed");
    }
}

utils::P256Point::P256Point()
{
    EC_GROUP *group = get_ec_group();
    EC_POINT *pt = EC_POINT_new(group);
    if (!pt) {
        throw runtime_error("Failed to create EC_POINT");
    }
    if (1 != EC_POINT_set_to_infinity(group, pt)) {
        throw runtime_error("Call to EC_POINT_set_to_infinity failed");
    }
    pt_ = pt;
}

void utils::P256Point::clean_up()
{
    if (pt_) {
        EC_POINT_clear_free(reinterpret_cast<EC_POINT *>(pt_));
        pt_ = nullptr;
    }
}

utils::P256Point::~P256Point()
{
    clean_up();
}

utils::P256Point::P256Point(const hash_type &data) : P256Point()
{
    // Extend the input first
    array<byte, order_size * 2> ext_value;
    compute_hash(data, "p256_constructor_hash", ext_value);

    // We use all but the last byte
    int byte_count = 2 * static_cast<int>(order_size) - 1;

    BIGNUM_guard bn;
    if (!BN_lebin2bn(
            reinterpret_cast<const unsigned char *>(ext_value.data()), byte_count, bn.get())) {
        throw runtime_error("Call to BN_lebin2bn failed");
    }

    // Reduce down to a possible x-coordinate
    reduce_mod_p256(bn.get());

    // Take the sign from the last byte
    int sign = static_cast<int>(ext_value.back()) & 1;

    // Non-constant-time
    BN_CTX_guard bcg;
    while (0 == EC_POINT_set_compressed_coordinates(
                    get_ec_group(), reinterpret_cast<EC_POINT *>(pt_), bn.get(), sign, bcg.get())) {
        // Increment the x-coordinate
        if (1 != BN_add_word(bn.get(), 1)) {
            throw runtime_error("Call to BN_add_word failed");
        }
        const BIGNUM *bn_p256 = BN_get0_nist_prime_256();
        if (0 == BN_cmp(bn.get(), bn_p256)) {
            BN_zero_ex(bn.get());
        }
    }
}

utils::P256Point::P256Point(const key_type &data) : P256Point(compute_key_hash(data))
{}

utils::P256Point &utils::P256Point::operator=(const P256Point &assign)
{
    if (&assign != this) {
        if (1 != EC_POINT_copy(
                     reinterpret_cast<EC_POINT *>(pt_),
                     reinterpret_cast<const EC_POINT *>(assign.pt_))) {
            throw runtime_error("Call to EC_POINT_copy failed");
        }
    }
    return *this;
}

utils::P256Point &utils::P256Point::operator=(P256Point &&source) noexcept
{
    if (&source != this) {
        swap(pt_, source.pt_);
    }
    return *this;
}

void utils::P256Point::MakeRandomNonzeroScalar(scalar_type &out)
{
    // Loop until we find a non-zero element
    do {
        random_scalar(out);
    } while (!is_nonzero_scalar(out));
}

void utils::P256Point::MakeSeededScalar(input_span_const_type seed, scalar_type &out)
{
    BIGNUM *out_bn = reinterpret_cast<BIGNUM *>(out.ptr());
    int byte_count = 2 * static_cast<int>(out.size());
    vector<unsigned char> buf(byte_count);
    vector<byte> hash(byte_count);
    compute_hash(seed, "seeded_scalar", hash);
    if (byte_count > hash.size()) {
        throw logic_error("Output should be at least equal in size to hash");
    }

    utils::copy_bytes(hash.data(), byte_count, buf.data());
    if (!BN_bin2bn(buf.data(), byte_count, out_bn)) {
        throw logic_error("Unable to convert hash to bignum");
    }

    reduce_mod_group_order(out_bn);
}

utils::P256Point utils::P256Point::MakeGenerator()
{
    P256Point result;
    const EC_POINT *gen = EC_GROUP_get0_generator(get_ec_group());
    if (1 != EC_POINT_copy(reinterpret_cast<EC_POINT *>(result.pt_), gen)) {
        throw runtime_error("Call to EC_POINT_copy failed");
    }
    return result;
}

utils::P256Point utils::P256Point::MakeGeneratorMultiple(const scalar_type &scalar)
{
    P256Point result;
    if (1 != EC_POINT_mul(
                 get_ec_group(),
                 reinterpret_cast<EC_POINT *>(result.pt_),
                 reinterpret_cast<const BIGNUM *>(scalar.ptr()),
                 nullptr,
                 nullptr,
                 nullptr)) {
        throw runtime_error("Call to EC_POINT_mul failed");
    }

    return result;
}

void utils::P256Point::InvertScalar(const scalar_type &in, scalar_type &out)
{
    const BIGNUM *order = EC_GROUP_get0_order(get_ec_group());
    if (!BN_mod_inverse(
            reinterpret_cast<BIGNUM *>(out.ptr()),
            reinterpret_cast<const BIGNUM *>(in.ptr()),
            order,
            nullptr)) {
        throw runtime_error("Call to BN_mod_inverse failed");
    }
}

void utils::P256Point::MultiplyScalar(
    const scalar_type &in1, const scalar_type &in2, scalar_type &out)
{
    const BIGNUM *order = EC_GROUP_get0_order(get_ec_group());

    // BN_mod_mul requires an allocated BN_CTX
    BN_CTX_guard bcg;
    if (1 != BN_mod_mul(
                 reinterpret_cast<BIGNUM *>(out.ptr()),
                 reinterpret_cast<const BIGNUM *>(in1.ptr()),
                 reinterpret_cast<const BIGNUM *>(in2.ptr()),
                 order,
                 bcg.get())) {
        throw runtime_error("Call to BN_mod_mul failed");
    }
}

void utils::P256Point::AddScalar(const scalar_type &in1, const scalar_type &in2, scalar_type &out)
{
    const BIGNUM *order = EC_GROUP_get0_order(get_ec_group());

    // We require that the output is different from inputs
    if (out.ptr() == in1.ptr() || out.ptr() == in2.ptr()) {
        BIGNUM_guard bn_temp;
        if (1 != BN_mod_add_quick(
                     reinterpret_cast<BIGNUM *>(bn_temp.get()),
                     reinterpret_cast<const BIGNUM *>(in1.ptr()),
                     reinterpret_cast<const BIGNUM *>(in2.ptr()),
                     order)) {
            throw runtime_error("Call to BN_mod_add_quick failed");
        }
        if (!BN_copy(reinterpret_cast<BIGNUM *>(out.ptr()), bn_temp.get())) {
            throw runtime_error("Call to BN_copy failed");
        }
    } else {
        if (1 != BN_mod_add_quick(
                     reinterpret_cast<BIGNUM *>(out.ptr()),
                     reinterpret_cast<const BIGNUM *>(in1.ptr()),
                     reinterpret_cast<const BIGNUM *>(in2.ptr()),
                     order)) {
            throw runtime_error("Call to BN_mod_add_quick failed");
        }
    }
}

void utils::P256Point::SubtractScalar(
    const scalar_type &in1, const scalar_type &in2, scalar_type &out)
{
    const BIGNUM *order = EC_GROUP_get0_order(get_ec_group());

    // We require that the output is different from inputs
    if (out.ptr() == in1.ptr() || out.ptr() == in2.ptr()) {
        BIGNUM_guard bn_temp;
        if (1 != BN_mod_sub_quick(
                     reinterpret_cast<BIGNUM *>(bn_temp.get()),
                     reinterpret_cast<const BIGNUM *>(in1.ptr()),
                     reinterpret_cast<const BIGNUM *>(in2.ptr()),
                     order)) {
            throw runtime_error("Call to BN_mod_sub_quick failed");
        }
        if (!BN_copy(reinterpret_cast<BIGNUM *>(out.ptr()), bn_temp.get())) {
            throw runtime_error("Call to BN_copy failed");
        }
    } else {
        if (1 != BN_mod_sub_quick(
                     reinterpret_cast<BIGNUM *>(out.ptr()),
                     reinterpret_cast<const BIGNUM *>(in1.ptr()),
                     reinterpret_cast<const BIGNUM *>(in2.ptr()),
                     order)) {
            throw runtime_error("Call to BN_mod_sub_quick failed");
        }
    }
}

void utils::P256Point::ReduceModOrder(scalar_type &scalar)
{
    reduce_mod_group_order(reinterpret_cast<BIGNUM *>(scalar.ptr()));
}

void utils::P256Point::ReduceModOrder(hash_type &value)
{
    BIGNUM_guard bn;
    if (!BN_lebin2bn(
            reinterpret_cast<const unsigned char *>(value.data()),
            hash_size,
            reinterpret_cast<BIGNUM *>(bn.get()))) {
        throw runtime_error("Call to BN_lebin2bn failed");
    }

    reduce_mod_group_order(reinterpret_cast<BIGNUM *>(bn.get()));

    if (hash_size != BN_bn2lebinpad(
                         reinterpret_cast<const BIGNUM *>(bn.get()),
                         reinterpret_cast<unsigned char *>(value.data()),
                         hash_size)) {
        throw runtime_error("Call to BN_bn2lebinpad failed");
    }
}

bool utils::P256Point::scalar_multiply(
    const scalar_type &scalar, bool clear_cofactor [[maybe_unused]])
{
    int poc = EC_POINT_is_on_curve(get_ec_group(), reinterpret_cast<EC_POINT *>(pt_), nullptr);
    if (0 == poc) {
        // If this point is not on curve, return false
        return false;
    }
    if (1 != poc) {
        throw runtime_error("Call to EC_POINT_is_on_curve failed");
    }

    if (1 != EC_POINT_mul(
                 get_ec_group(),
                 reinterpret_cast<EC_POINT *>(pt_),
                 nullptr,
                 reinterpret_cast<const EC_POINT *>(pt_),
                 reinterpret_cast<const BIGNUM *>(scalar.ptr()),
                 nullptr)) {
        throw runtime_error("Call to EC_POINT_mul failed");
    }

    return true;
}

bool utils::P256Point::double_scalar_multiply(
    const scalar_type &scalar1, const scalar_type &scalar2)
{
    // Computes scalar1*this + scalar2*generator

    int poc = EC_POINT_is_on_curve(get_ec_group(), reinterpret_cast<EC_POINT *>(pt_), nullptr);
    if (0 == poc) {
        // If this point is not on curve, return false
        return false;
    }
    if (1 != poc) {
        throw runtime_error("Call to EC_POINT_is_on_curve failed");
    }

    if (1 != EC_POINT_mul(
                 get_ec_group(),
                 reinterpret_cast<EC_POINT *>(pt_),
                 reinterpret_cast<const BIGNUM *>(scalar2.ptr()),
                 reinterpret_cast<EC_POINT *>(pt_),
                 reinterpret_cast<const BIGNUM *>(scalar1.ptr()),
                 nullptr)) {
        throw runtime_error("Call to EC_POINT_mul failed");
    }

    return true;
}

bool utils::P256Point::in_prime_order_subgroup() const
{
    int poc = EC_POINT_is_on_curve(get_ec_group(), reinterpret_cast<EC_POINT *>(pt_), nullptr);
    if (-1 == poc) {
        throw runtime_error("Call to EC_POINT_is_on_curve failed");
    }

    return poc == 1;
}

void utils::P256Point::add(const P256Point &other)
{
    if (1 != EC_POINT_add(
                 get_ec_group(),
                 reinterpret_cast<EC_POINT *>(pt_),
                 reinterpret_cast<EC_POINT *>(pt_),
                 reinterpret_cast<EC_POINT *>(other.pt_),
                 nullptr)) {
        throw runtime_error("Call to EC_POINT_add failed");
    }
}

void utils::P256Point::save(ostream &stream) const
{
    auto old_ex_mask = stream.exceptions();
    stream.exceptions(ios_base::failbit | ios_base::badbit);

    try {
        array<unsigned char, save_size> buf = {};
        if (0 == EC_POINT_point2oct(
                     get_ec_group(),
                     reinterpret_cast<EC_POINT *>(pt_),
                     POINT_CONVERSION_COMPRESSED,
                     buf.data(),
                     buf.size(),
                     nullptr)) {
            stream.exceptions(old_ex_mask);
            throw runtime_error("Call to EC_POINT_point2oct failed");
        }
        stream.write(reinterpret_cast<const char *>(buf.data()), save_size);
    } catch (const ios_base::failure &) {
        stream.exceptions(old_ex_mask);
        throw;
    }

    stream.exceptions(old_ex_mask);
}

void utils::P256Point::load(istream &stream)
{
    auto old_ex_mask = stream.exceptions();
    stream.exceptions(ios_base::failbit | ios_base::badbit);

    try {
        array<unsigned char, save_size> buf;
        stream.read(reinterpret_cast<char *>(buf.data()), save_size);
        if (1 != EC_POINT_oct2point(
                     get_ec_group(),
                     reinterpret_cast<EC_POINT *>(pt_),
                     buf.data(),
                     buf.size(),
                     nullptr)) {
            stream.exceptions(old_ex_mask);
            throw logic_error("Call to EC_POINT_oct2point failed");
        }
    } catch (const ios_base::failure &) {
        stream.exceptions(old_ex_mask);
        throw;
    }

    stream.exceptions(old_ex_mask);
}

void utils::P256Point::save(point_save_span_type out) const
{
    zero_span(out);
    size_t ret = EC_POINT_point2oct(
        get_ec_group(),
        reinterpret_cast<EC_POINT *>(pt_),
        POINT_CONVERSION_COMPRESSED,
        reinterpret_cast<unsigned char *>(out.data()),
        out.size(),
        nullptr);

    if (ret == 0) {
        throw runtime_error("Call to EC_POINT_point2oct failed");
    }
}

void utils::P256Point::load(point_save_span_const_type in)
{
    if (1 != EC_POINT_oct2point(
                 get_ec_group(),
                 reinterpret_cast<EC_POINT *>(pt_),
                 reinterpret_cast<const unsigned char *>(in.data()),
                 in.size(),
                 nullptr)) {
        throw runtime_error("Call to EC_POINT_oct2point failed");
    }
}

#else

// FourQ
#include "oZKS/fourq/FourQ_api.h"
#include "oZKS/fourq/FourQ_internal.h"

namespace {
    void random_scalar(utils::FourQPoint::scalar_type &value)
    {
        if (!utils::random_bytes(value.data(), value.size())) {
            throw runtime_error("Failed to get random bytes");
        }
        modulo_order(
            reinterpret_cast<digit_t *>(value.data()), reinterpret_cast<digit_t *>(value.data()));
    }

    digit_t is_nonzero_scalar(utils::FourQPoint::scalar_type &value)
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

utils::FourQPoint::FourQPoint(const hash_type &data)
{
    // Copy the hashed data to an f2elm_t struct
    f2elm_t r;
    static_assert(sizeof(r) <= sizeof(data), "f2elm cannot be larger than hash_type");
    utils::copy_bytes(data.data(), sizeof(f2elm_t), r);

    // Reduce r; note that this does not produce a perfectly uniform distribution modulo
    // 2^127-1, but it is good enough.
    mod1271(r[0]);
    mod1271(r[1]);

    // Create an elliptic curve point
    HashToCurve(r, pt_);
}

utils::FourQPoint::FourQPoint(const key_type &data) : FourQPoint(compute_key_hash(data))
{}

void utils::FourQPoint::MakeRandomNonzeroScalar(scalar_type &out)
{
    // Loop until we find a non-zero element
    do {
        random_scalar(out);
    } while (!is_nonzero_scalar(out));
}

void utils::FourQPoint::MakeSeededScalar(input_span_const_type seed, scalar_type &out)
{
    hash_type hash = compute_hash(seed, "seeded_scalar");
    if (out.size() > hash.size()) {
        throw logic_error("Output should be at least equal in size to hash");
    }

    utils::copy_bytes(hash.data(), out.size(), out.data());
    modulo_order(reinterpret_cast<digit_t *>(out.data()), reinterpret_cast<digit_t *>(out.data()));
}

utils::FourQPoint utils::FourQPoint::MakeGenerator()
{
    FourQPoint result;
    eccset(result.pt_);

    return result;
}

utils::FourQPoint utils::FourQPoint::MakeGeneratorMultiple(const scalar_type &scalar)
{
    FourQPoint result;

    // The ecc_mul_fixed function does not mutate the first input, even though it is not marked
    // const
    digit_t *scalar_ptr = const_cast<digit_t *>(reinterpret_cast<const digit_t *>(scalar.data()));
    ecc_mul_fixed(scalar_ptr, result.pt_);

    return result;
}

void utils::FourQPoint::InvertScalar(const scalar_type &in, scalar_type &out)
{
    to_Montgomery(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in.data())),
        reinterpret_cast<digit_t *>(out.data()));
    Montgomery_inversion_mod_order(
        reinterpret_cast<digit_t *>(out.data()), reinterpret_cast<digit_t *>(out.data()));
    from_Montgomery(
        reinterpret_cast<digit_t *>(out.data()), reinterpret_cast<digit_t *>(out.data()));
}

void utils::FourQPoint::MultiplyScalar(
    const scalar_type &in1, const scalar_type &in2, scalar_type &out)
{
    utils::FourQPoint::scalar_type in1_mont{}, in2_mont{};
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

void utils::FourQPoint::AddScalar(const scalar_type &in1, const scalar_type &in2, scalar_type &out)
{
    add_mod_order(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in1.data())),
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in2.data())),
        reinterpret_cast<digit_t *>(out.data()));
}

void utils::FourQPoint::SubtractScalar(
    const scalar_type &in1, const scalar_type &in2, scalar_type &out)
{
    subtract_mod_order(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in1.data())),
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(in2.data())),
        reinterpret_cast<digit_t *>(out.data()));
}

void utils::FourQPoint::ReduceModOrder(scalar_type &scalar)
{
    modulo_order(
        reinterpret_cast<digit_t *>(scalar.data()), reinterpret_cast<digit_t *>(scalar.data()));
}

void utils::FourQPoint::ReduceModOrder(hash_type &value)
{
    // Truncate and then reduce; this is not ideal
    modulo_order(
        reinterpret_cast<digit_t *>(value.data()), reinterpret_cast<digit_t *>(value.data()));
    fill(value.begin() + order_size, value.end(), byte{ 0 });
}

bool utils::FourQPoint::scalar_multiply(const scalar_type &scalar, bool clear_cofactor)
{
    // The ecc_mul functions returns false when the input point is not a valid curve point
    return ecc_mul(
        pt_,
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(scalar.data())),
        pt_,
        clear_cofactor);
}

bool utils::FourQPoint::double_scalar_multiply(
    const scalar_type &scalar1, const scalar_type &scalar2)
{
    // The double_ecc_mul functions returns false when the input point is not a valid curve point
    return ecc_mul_double(
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(scalar2.data())),
        pt_,
        const_cast<digit_t *>(reinterpret_cast<const digit_t *>(scalar1.data())),
        pt_);
}

bool utils::FourQPoint::in_prime_order_subgroup() const
{
    // Encode this point
    array<byte, save_size> encoded_pt{};
    point_t pt_copy{ pt_[0] };
    encode(pt_copy, reinterpret_cast<unsigned char *>(encoded_pt.data()));

    // Clear cofactor
    point_extproj_t proj_pt{};
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
    array<byte, save_size> encoded_pt2{};
    encode(pt_copy, reinterpret_cast<unsigned char *>(encoded_pt2.data()));

    // The two encodings must match
    bool are_same = (memcmp(encoded_pt.data(), encoded_pt2.data(), save_size) == 0);
    return are_same;
}

void utils::FourQPoint::add(const FourQPoint &other)
{
    FourQPoint other_copy(other);
    point_extproj_t this_proj_pt{}, other_proj_pt{}, sum_proj_pt{};
    point_setup(pt_, this_proj_pt);
    point_setup(other_copy.pt_, other_proj_pt);

    point_extproj_precomp_t this_proj_prec_pt, other_proj_prec_pt;
    R1_to_R2(this_proj_pt, this_proj_prec_pt);
    R1_to_R3(other_proj_pt, other_proj_prec_pt);
    eccadd_core(this_proj_prec_pt, other_proj_prec_pt, sum_proj_pt);

    eccnorm(sum_proj_pt, pt_);
}

utils::FourQPoint::scalar_type::scalar_type(scalar_span_const_type in) : scalar_type()
{
    load(in);
}

bool utils::FourQPoint::scalar_type::is_zero() const
{
    return *this == scalar_type{};
}

void utils::FourQPoint::scalar_type::load(scalar_span_const_type in)
{
    copy(in.begin(), in.end(), begin());
}

void utils::FourQPoint::scalar_type::save(scalar_span_type out) const
{
    copy(begin(), end(), out.begin());
}

utils::FourQPoint &utils::FourQPoint::operator=(const FourQPoint &assign)
{
    if (&assign != this) {
        pt_[0] = assign.pt_[0];
    }
    return *this;
}

void utils::FourQPoint::save(ostream &stream) const
{
    auto old_ex_mask = stream.exceptions();
    stream.exceptions(ios_base::failbit | ios_base::badbit);

    try {
        array<unsigned char, save_size> buf{};
        point_t pt_copy{ pt_[0] };
        encode(pt_copy, buf.data());
        stream.write(reinterpret_cast<const char *>(buf.data()), save_size);
    } catch (const ios_base::failure &) {
        stream.exceptions(old_ex_mask);
        throw;
    }
    stream.exceptions(old_ex_mask);
}

void utils::FourQPoint::load(istream &stream)
{
    auto old_ex_mask = stream.exceptions();
    stream.exceptions(ios_base::failbit | ios_base::badbit);

    try {
        array<unsigned char, save_size> buf{};
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

void utils::FourQPoint::save(point_save_span_type out) const
{
    point_t pt_copy{ pt_[0] };
    encode(pt_copy, reinterpret_cast<unsigned char *>(out.data()));
}

void utils::FourQPoint::load(point_save_span_const_type in)
{
    if (decode(reinterpret_cast<const unsigned char *>(in.data()), pt_) != ECCRYPTO_SUCCESS) {
        throw logic_error("Invalid point");
    }
}

#endif
