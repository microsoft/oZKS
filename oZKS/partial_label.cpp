// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <cstddef>

// oZKS
#include "oZKS/config.h"
#include "oZKS/partial_label.h"
#include "oZKS/utilities.h"

// Intrinsics
#ifdef OZKS_USE_INTRIN
#ifdef __WINDOWS__
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#endif

using namespace std;
using namespace ozks;

PartialLabel::PartialLabel(gsl::span<const byte> input, size_t bit_count)
{
    if (input.size() > ByteCount) {
        throw invalid_argument("Cannot have input bigger than available bytes");
    }
    if (bit_count > input.size() * 8) {
        throw invalid_argument("Bit count is bigger than available bits");
    }
    if (bit_count == 0) {
        bit_count_ = static_cast<uint32_t>(input.size()) * 8;
    } else {
        bit_count_ = static_cast<uint32_t>(bit_count);
    }

    init(input, bit_count_);
}

PartialLabel::PartialLabel(const PartialLabel &label, size_t bit_count)
{
    if (bit_count > MaxBitCount) {
        throw invalid_argument("Cannot have label bigger than MaxBitCount");
    }
    if (bit_count > label.bit_count_) {
        throw invalid_argument(
            "Bit count of new label should be equal or less than original label");
    }

    init(
        gsl::span(reinterpret_cast<const byte *>(label.data()), ByteCount),
        static_cast<uint32_t>(bit_count));
}

PartialLabel::PartialLabel(initializer_list<bool> bits)
{
    if (bits.size() > MaxBitCount) {
        throw invalid_argument("Cannot have more than MaxBitCount bits");
    }

    label_[0] = 0;
    label_[1] = 0;
    label_[2] = 0;
    label_[3] = 0;

    uint32_t bit_idx = 0;

    bit_count_ = static_cast<uint32_t>(bits.size());

    for (auto it = bits.begin(); it != bits.end(); it++) {
        set_bit(bit_idx, *it);
        bit_idx++;
    }
}

bool PartialLabel::bit(size_t bit_index) const
{
    if (bit_count_ == 0) {
        throw runtime_error("Label is empty");
    }
    if (bit_index >= MaxBitCount) {
        throw invalid_argument("Index out of range");
    }
    if (bit_index >= bit_count_) {
        throw invalid_argument("Index out of range");
    }

    size_t word_idx = static_cast<size_t>(bit_index) / 64;
    return bit(bit_index % 64, label_[word_idx]);
}

bool PartialLabel::operator[](size_t bit_idx) const
{
    return bit(bit_idx);
}

void PartialLabel::add_bit(bool bit)
{
    if (static_cast<size_t>(bit_count_) >= MaxBitCount) {
        throw runtime_error("Label is already at max size");
    }

    bit_count_++;
    set_bit(bit_count_ - 1, bit);
}

PartialLabel PartialLabel::CommonPrefix(const PartialLabel &label1, const PartialLabel &label2)
{
    PartialLabel common_prefix;

    common_prefix.bit_count_ = 0;
    uint32_t max_bit_count = std::min(label1.bit_count_, label2.bit_count_);

    size_t word_idx = 0;
    // Skip words that are the same
    while ((max_bit_count >= sizeof(uint64_t) * 8) &&
           (label1.label_[word_idx] == label2.label_[word_idx])) {
        common_prefix.label_[word_idx] = label1.label_[word_idx];
        common_prefix.bit_count_ += static_cast<uint32_t>(sizeof(uint64_t) * 8);

        word_idx++;
        max_bit_count -= static_cast<uint32_t>(sizeof(uint64_t) * 8);
    }

    // Compute common bits in the word that actually differs
    if (max_bit_count > 0) {
        uint64_t xord = label1.label_[word_idx] ^ label2.label_[word_idx];
        uint16_t shift = (xord == 0)
                             ? static_cast<uint16_t>(max_bit_count)
                             : std::min(get_msb(xord), static_cast<uint16_t>(max_bit_count));
        uint64_t mask = ~(0xFFFFFFFFFFFFFFFFULL >> shift);
        common_prefix.label_[word_idx] = label1.label_[word_idx] & mask;
        common_prefix.bit_count_ += shift;
    }

    // Zero out the rest of the words
    for (word_idx++; word_idx < WordCount; word_idx++) {
        common_prefix.label_[word_idx] = 0;
    }

    return common_prefix;
}

uint32_t PartialLabel::CommonPrefixCount(const PartialLabel &label1, const PartialLabel &label2)
{
    return CommonPrefix(label1, label2).bit_count_;
}

vector<byte> PartialLabel::to_bytes() const
{
    vector<byte> bytes;
    size_t byte_count = (bit_count_ + 7) / 8;

    array<uint64_t, PartialLabel::WordCount> label;
    utils::copy_bytes(data(), PartialLabel::ByteCount, label.data());

    for (auto &lbl : label) {
        byte *pbytes = reinterpret_cast<byte *>(&lbl);
        reverse(pbytes, pbytes + sizeof(uint64_t));
    }

    // Make the bytes vector allocation big enough
    bytes.resize(byte_count);
    utils::copy_bytes(label.data(), byte_count, bytes.data());

    return bytes;
}

void PartialLabel::clear()
{
    label_[0] = 0;
    label_[1] = 0;
    label_[2] = 0;
    label_[3] = 0;
    bit_count_ = 0;
}

void PartialLabel::init(gsl::span<const byte> bytes, uint32_t bit_count)
{
    uint32_t byte_size = (bit_count + 7) / 8;
    uint32_t mask_size = bit_count % 8;

    memset(label_.data(), 0, ByteCount);
    utils::copy_bytes(bytes.data(), byte_size, label_.data());

    if (mask_size > 0) {
        byte mask = byte{ 0xFF };
        mask <<= (8 - mask_size);
        reinterpret_cast<byte *>(label_.data())[byte_size - 1] &= mask;
    }

    for (auto &lbl : label_) {
        byte *pbytes = reinterpret_cast<byte *>(&lbl);
        reverse(pbytes, pbytes + sizeof(uint64_t));
    }
}

void PartialLabel::set_bit(uint32_t bit_index, bool value)
{
    if (bit_index >= bit_count_) {
        throw invalid_argument("Bit index is out of range");
    }

    size_t word_idx = bit_index / 64;
    // This performs a byte index reversal, so that byte 0 will be the highest byte,
    // and byte 7 the lowest byte in the uint64_t.
    size_t byte_idx = static_cast<size_t>(std::abs(((static_cast<int>(bit_index) % 64) / 8) - 7));
    size_t bit_idx = bit_index % 8;
    byte mask = byte{ 0x80 };
    mask >>= bit_idx;

    byte *pbyte = reinterpret_cast<byte *>(&label_[word_idx]);
    if (value) {
        pbyte[byte_idx] |= mask;
    } else {
        mask = ~mask;
        pbyte[byte_idx] &= mask;
    }
}

uint16_t PartialLabel::get_msb(uint64_t value)
{
#ifdef OZKS_USE__BITSCANREVERSE64

    unsigned long index;
    _BitScanReverse64(&index, value);
    return static_cast<uint16_t>(63UL - index);

#else
#ifdef OZKS_USE___BUILTIN_CLZLL

    uint16_t result = static_cast<uint16_t>(__builtin_clzll(value));
    return result;

#else

    // Use generic implementation
    for (size_t i = 0; i < sizeof(uint64_t) * 8; i++) {
        if (bit(i, value) == 1) {
            return static_cast<uint16_t>(i);
        }
    }

#endif
#endif
}

bool PartialLabel::bit(size_t bit_index, uint64_t value)
{
    if (bit_index >= sizeof(uint64_t) * 8) {
        throw invalid_argument("Bit index out of range");
    }

    uint64_t mask = 0x8000000000000000ULL;
    mask >>= bit_index;
    bool result = (value & mask) != 0;
    return result;
}
