// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <array>
#include <cstddef>
#include <iostream>

// OZKS
#include "oZKS/config.h"
#include "oZKS/defines.h"

// GSL
#include "gsl/span"

#ifdef OZKS_USE_OPENSSL_P256
namespace ozks {
    namespace utils {
        class P256Point {
        public:
            // Size with point compression
            static constexpr std::size_t save_size = 33;
            static constexpr std::size_t order_size = 32;

            using scalar_span_const_type = gsl::span<const std::byte, order_size>;
            using scalar_span_type = gsl::span<std::byte, order_size>;
            using input_span_const_type = gsl::span<const std::byte>;
            using point_save_span_type = gsl::span<std::byte, save_size>;
            using point_save_span_const_type = gsl::span<const std::byte, save_size>;
            using encode_to_curve_salt_type = point_save_span_const_type;

            class scalar_type {
            public:
                scalar_type();

                ~scalar_type();

                scalar_type(void *scalar_ptr);

                scalar_type(scalar_span_const_type in);

                scalar_type &operator=(const scalar_type &assign);

                scalar_type(const scalar_type &assign) : scalar_type()
                {
                    operator=(assign);
                }

                scalar_type &operator=(scalar_type &&source) noexcept;

                scalar_type(scalar_type &&source) noexcept
                {
                    operator=(std::move(source));
                }

                bool operator==(const scalar_type &other) const;

                bool operator!=(const scalar_type &other) const
                {
                    return !operator==(other);
                }

                void *ptr() noexcept
                {
                    return scalar_;
                }

                const void *ptr() const noexcept
                {
                    return scalar_;
                }

                static constexpr std::size_t Size() noexcept
                {
                    return order_size;
                }

                constexpr std::size_t size() noexcept
                {
                    return Size();
                }

                void set_zero();

                template <typename T>
                void set_byte(std::size_t index, T value);

                bool is_zero() const;

                void load(scalar_span_const_type in);

                void save(scalar_span_type out) const;

            private:
                void clean_up();

                void *scalar_ = nullptr;
            };

            // Output hash size is 32 bytes
            static constexpr std::size_t hash_size = 32;

            // A string differentiating curve implementations in the library.
            static constexpr char curve_descriptor[] = "p256_openssl_ozks";

            // Initializes the P256Point with the neutral element
            P256Point();

            ~P256Point();

            P256Point &operator=(const P256Point &assign);

            P256Point &operator=(P256Point &&source) noexcept;

            P256Point(const P256Point &copy) : P256Point()
            {
                operator=(copy);
            }

            P256Point(P256Point &&source) noexcept
            {
                operator=(std::move(source));
            }

            // This function hashes the input to a uniformly random elliptic curve point.
            // This function is *not* constant-time.
            P256Point(const hash_type &data, encode_to_curve_salt_type salt);

            // This function hashes the input to a uniformly random elliptic curve point.
            // This function is *not* constant-time.
            P256Point(const key_type &data, encode_to_curve_salt_type salt);

            // Creates a random non-zero number modulo the prime order subgroup.
            static void MakeRandomNonzeroScalar(scalar_type &out);

            static void MakeSeededScalar(input_span_const_type seed, scalar_type &out);

            static P256Point MakeGenerator();

            static P256Point MakeGeneratorMultiple(const scalar_type &scalar);

            static void InvertScalar(const scalar_type &in, scalar_type &out);

            static void MultiplyScalar(
                const scalar_type &in1, const scalar_type &in2, scalar_type &out);

            static void AddScalar(const scalar_type &in1, const scalar_type &in2, scalar_type &out);

            static void SubtractScalar(
                const scalar_type &in1, const scalar_type &in2, scalar_type &out);

            static void ReduceModOrder(scalar_type &scalar);

            static void ReduceModOrder(hash_type &value);

            bool scalar_multiply(const scalar_type &scalar, bool clear_cofactor);

            // Computes scalar1*this+scalar2*generator; does not clear cofactor
            bool double_scalar_multiply(const scalar_type &scalar1, const scalar_type &scalar2);

            bool in_prime_order_subgroup() const;

            void add(const P256Point &other);

            void save(std::ostream &stream) const;

            void load(std::istream &stream);

            void save(point_save_span_type out) const;

            void load(point_save_span_const_type in);

        private:
            void clean_up();

            void *pt_ = nullptr;
        }; // class P256Point

        using ECPoint = P256Point;
    } // namespace utils
} // namespace ozks
#else
#include "oZKS/fourq/FourQ.h"

namespace ozks {
    namespace utils {
        class FourQPoint {
        public:
            static constexpr std::size_t save_size = sizeof(f2elm_t);
            static constexpr std::size_t order_size = sizeof(digit_t) * NWORDS_ORDER;

            using scalar_span_const_type = gsl::span<const std::byte, order_size>;
            using scalar_span_type = gsl::span<std::byte, order_size>;
            using input_span_const_type = gsl::span<const std::byte>;
            using point_save_span_type = gsl::span<std::byte, save_size>;
            using point_save_span_const_type = gsl::span<const std::byte, save_size>;
            using encode_to_curve_salt_type = point_save_span_const_type;

            class scalar_type : public std::array<std::byte, order_size> {
            public:
                scalar_type() = default;

                scalar_type(scalar_span_const_type in);

                bool is_zero() const;

                void load(scalar_span_const_type in);

                void save(scalar_span_type out) const;
            };

            // Output hash size is 32 bytes
            static constexpr std::size_t hash_size = 32;

            // A string differentiating curve implementations in the library.
            static constexpr char curve_descriptor[] = "fourq_ozks";

            // Initializes the FourQPoint with the neutral element
            FourQPoint() = default;

            FourQPoint &operator=(const FourQPoint &assign);

            FourQPoint(const FourQPoint &copy)
            {
                operator=(copy);
            }

            // This function hashes the input to a uniformly random elliptic curve point.
            FourQPoint(const hash_type &value, encode_to_curve_salt_type salt);

            // This function hashes the input to a uniformly random elliptic curve point.
            FourQPoint(const key_type &data, encode_to_curve_salt_type salt);

            // Creates a random non-zero number modulo the prime order subgroup.
            static void MakeRandomNonzeroScalar(scalar_type &out);

            // Creates a number modulo the prime order subgropup order using the given data as seed
            static void MakeSeededScalar(input_span_const_type seed, scalar_type &out);

            static FourQPoint MakeGenerator();

            static FourQPoint MakeGeneratorMultiple(const scalar_type &scalar);

            static void InvertScalar(const scalar_type &in, scalar_type &out);

            static void MultiplyScalar(
                const scalar_type &in1, const scalar_type &in2, scalar_type &out);

            static void AddScalar(const scalar_type &in1, const scalar_type &in2, scalar_type &out);

            static void SubtractScalar(
                const scalar_type &in1, const scalar_type &in2, scalar_type &out);

            static void ReduceModOrder(scalar_type &scalar);

            static void ReduceModOrder(hash_type &value);

            bool scalar_multiply(const scalar_type &scalar, bool clear_cofactor);

            // Computes scalar1*this+scalar2*generator; does not clear cofactor
            bool double_scalar_multiply(const scalar_type &scalar1, const scalar_type &scalar2);

            bool in_prime_order_subgroup() const;

            void add(const FourQPoint &other);

            void save(std::ostream &stream) const;

            void load(std::istream &stream);

            void save(point_save_span_type out) const;

            void load(point_save_span_const_type in);

        private:
            // Initialize to neutral element
            point_t pt_ = { { { { 0 } }, { { 1 } } } }; // { {.x = { 0 }, .y = { 1 } }};
        };                                              // class FourQPoint

        using ECPoint = FourQPoint;
    } // namespace utils
} // namespace ozks
#endif
