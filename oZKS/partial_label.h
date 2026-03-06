// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <array>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <vector>

// oZKS
#include "oZKS/defines.h"

// GSL
#include "gsl/span"

namespace ozks {
    class PartialLabel {
    public:
        /**
        Construct an instance of an empty PartialLabel
        */
        PartialLabel() : label_({}), bit_count_(0)
        {}

        /**
        Construct an instance of a PartialLabel from the given bytes
        If bit_count is provided, will only take that number of bits.
        */
        PartialLabel(gsl::span<const std::byte> input, std::size_t bit_count = 0);

        /**
        Construct an instance of a PartialLabel from the given label and take
        bit_count bits from it.
        */
        PartialLabel(const PartialLabel &label, std::size_t bit_count);

        /**
        Construct an instance of a PartialLabel from a list of bits
        */
        PartialLabel(std::initializer_list<bool> bits);

        /**
        Construct an instance of a PartialLabel from a list of bytes
        */
        PartialLabel(std::initializer_list<std::byte> bytes)
            : PartialLabel(std::vector<std::byte>(bytes))
        {}

        /**
        Get the bit count of the PartialLabel
        */
        std::uint32_t bit_count() const noexcept
        {
            return bit_count_;
        }

        /**
        Get a pointer to the label data
        */
        const std::uint64_t *data() const
        {
            return label_.data();
        }

        /**
        Get the bit at position bit_idx in the label
        */
        bool bit(std::size_t bit_idx) const;

        /**
        Get the bit at position bit_idx in the label
        */
        bool operator[](std::size_t bit_idx) const;

        /**
        Compare equality in two labels
        */
        bool operator==(const PartialLabel &other) const
        {
            return (bit_count_ == other.bit_count_) && (label_ == other.label_);
        }

        /**
        Compare non-equality in two labels
        */
        bool operator!=(const PartialLabel &other) const
        {
            return !(*this == other);
        }

        /**
        Less-than operator
        */
        bool operator<(const PartialLabel &other) const
        {
            if (bit_count_ != other.bit_count_) {
                return bit_count_ < other.bit_count_;
            }

            return label_ < other.label_;
        }

        /**
        Add a bit to the label
        */
        void add_bit(bool bit);

        /**
        Whether the label is empty
        */
        bool empty() const
        {
            return bit_count_ == 0;
        }

        /**
        Clear the contents of this PartialLabel
        */
        void clear();

        /**
        Get the high-order bits that are common between the given PartialLabels
        */
        static PartialLabel CommonPrefix(const PartialLabel &label1, const PartialLabel &label2);

        /**
        Get the count of high-order bits that are common between the given PartialLabels
        */
        static std::uint32_t CommonPrefixCount(
            const PartialLabel &label1, const PartialLabel &label2);

        /**
        Get the label as a byte vector
        */
        std::vector<std::byte> to_bytes() const;

        /**
        How many bytes a PartialLabel is made of
        */
        constexpr static std::size_t ByteCount = 32;

        /**
        Maximum number of bits a PartialLabel can hold
        */
        constexpr static std::size_t MaxBitCount = ByteCount * 8;

        /**
        The serialized size of the PartialLabel
        */
        constexpr static std::size_t SaveSize = ByteCount + sizeof(std::uint32_t);

        /**
        Save the PartialLabel to a buffer
        */
        void save(gsl::span<std::byte, SaveSize> buffer) const;

        /**
        Load a PartialLabel from a buffer.
        */
        void load(gsl::span<const std::byte, SaveSize> buffer);

    private:
        constexpr static std::size_t WordCount = 4;

        std::array<std::uint64_t, WordCount> label_;
        std::uint32_t bit_count_;

        void init(gsl::span<const std::byte> bytes, std::uint32_t bit_count);
        void set_bit(std::uint32_t bit_idx, bool value);
        void zero_trailing_bits();

        static std::uint16_t get_msb(std::uint64_t value);
        static bool bit(std::size_t bit_idx, std::uint64_t value);

        friend std::hash<PartialLabel>;
    };
} // namespace ozks

namespace std {
    template <>
    struct hash<ozks::PartialLabel> {
        std::size_t operator()(const ozks::PartialLabel &label) const
        {
            return std::hash<std::uint64_t>()(label.label_[0]);
        }
    };

} // namespace std
