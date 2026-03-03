// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// oZKS
#include "oZKS/partial_label.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

TEST(PartialLabelTests, CommonPrefixTest)
{
    PartialLabel lbl1{ 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1 };
    PartialLabel lbl2{ 1, 0, 1, 0, 1, 0, 0 };

    PartialLabel common = PartialLabel::CommonPrefix(lbl1, lbl2);

    EXPECT_EQ(6, common.bit_count());
    EXPECT_EQ(1, common[0]);
    EXPECT_EQ(0, common[1]);
    EXPECT_EQ(1, common[2]);
    EXPECT_EQ(0, common[3]);
    EXPECT_EQ(1, common[4]);
    EXPECT_EQ(0, common[5]);

    common.clear();
    EXPECT_EQ(0, common.bit_count());

    common = PartialLabel::CommonPrefix(lbl2, lbl1);
    EXPECT_EQ(6, common.bit_count());
    EXPECT_EQ(1, common[0]);
    EXPECT_EQ(0, common[1]);
    EXPECT_EQ(1, common[2]);
    EXPECT_EQ(0, common[3]);
    EXPECT_EQ(1, common[4]);
    EXPECT_EQ(0, common[5]);

    lbl1 = make_bytes<PartialLabel>(
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99);
    lbl2 = make_bytes<PartialLabel>(0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22);
    EXPECT_EQ(120, lbl1.bit_count());
    EXPECT_EQ(64, lbl2.bit_count());

    common.clear();
    EXPECT_EQ(0, common.bit_count());

    common = PartialLabel::CommonPrefix(lbl1, lbl2);

    EXPECT_EQ(64, common.bit_count());
    // AA
    EXPECT_EQ(1, common[0]);
    EXPECT_EQ(0, common[1]);
    EXPECT_EQ(1, common[2]);
    EXPECT_EQ(0, common[3]);
    EXPECT_EQ(1, common[4]);
    EXPECT_EQ(0, common[5]);
    EXPECT_EQ(1, common[6]);
    EXPECT_EQ(0, common[7]);
    // 22
    EXPECT_EQ(0, common[56]);
    EXPECT_EQ(0, common[57]);
    EXPECT_EQ(1, common[58]);
    EXPECT_EQ(0, common[59]);
    EXPECT_EQ(0, common[60]);
    EXPECT_EQ(0, common[61]);
    EXPECT_EQ(1, common[62]);
    EXPECT_EQ(0, common[63]);
}

TEST(PartialLabelTests, CommonPrefixFromBytesTest)
{
    PartialLabel lbl1 = make_bytes<PartialLabel>(
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);
    PartialLabel lbl2 = make_bytes<PartialLabel>(
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55);

    PartialLabel common = PartialLabel::CommonPrefix(lbl1, lbl2);
    EXPECT_EQ(96, common.bit_count());
    EXPECT_EQ(1, common.bit(0));
    EXPECT_EQ(0, common.bit(1));
    EXPECT_EQ(1, common.bit(2));
    EXPECT_EQ(0, common.bit(3));
    EXPECT_EQ(1, common.bit(4));
    EXPECT_EQ(0, common.bit(5));
    EXPECT_EQ(1, common.bit(6));
    EXPECT_EQ(0, common.bit(7));
    EXPECT_EQ(1, common.bit(8));
    EXPECT_EQ(0, common.bit(9));
    EXPECT_EQ(1, common.bit(10));
    EXPECT_EQ(1, common.bit(11));

    lbl2 = make_bytes<PartialLabel>(0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x00, 0x01);

    common = PartialLabel::CommonPrefix(lbl1, lbl2);

    EXPECT_EQ(24, common.bit_count());
    // AA
    EXPECT_EQ(1, common.bit(0));
    EXPECT_EQ(0, common.bit(1));
    EXPECT_EQ(1, common.bit(2));
    EXPECT_EQ(0, common.bit(3));
    EXPECT_EQ(1, common.bit(4));
    EXPECT_EQ(0, common.bit(5));
    EXPECT_EQ(1, common.bit(6));
    EXPECT_EQ(0, common.bit(7));
    // CC
    EXPECT_EQ(1, common.bit(16));
    EXPECT_EQ(1, common.bit(17));
    EXPECT_EQ(0, common.bit(18));
    EXPECT_EQ(0, common.bit(19));
    EXPECT_EQ(1, common.bit(20));
    EXPECT_EQ(1, common.bit(21));
    EXPECT_EQ(0, common.bit(22));
    EXPECT_EQ(0, common.bit(23));

    lbl1 = make_bytes<PartialLabel>(
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x11,
        0xAA);
    lbl2 = make_bytes<PartialLabel>(
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        0x66,
        0x77,
        0x88,
        0x11,
        0xAC,
        0xBB);

    EXPECT_EQ(144, lbl1.bit_count());
    EXPECT_EQ(152, lbl2.bit_count());

    common.clear();
    EXPECT_EQ(0, common.bit_count());

    common = PartialLabel::CommonPrefix(lbl1, lbl2);

    EXPECT_EQ(141, common.bit_count());
    // 11
    EXPECT_EQ(0, common[0]);
    EXPECT_EQ(0, common[1]);
    EXPECT_EQ(0, common[2]);
    EXPECT_EQ(1, common[3]);
    EXPECT_EQ(0, common[4]);
    EXPECT_EQ(0, common[5]);
    EXPECT_EQ(0, common[6]);
    EXPECT_EQ(1, common[7]);
    // 11
    EXPECT_EQ(0, common[128]);
    EXPECT_EQ(0, common[129]);
    EXPECT_EQ(0, common[130]);
    EXPECT_EQ(1, common[131]);
    EXPECT_EQ(0, common[132]);
    EXPECT_EQ(0, common[133]);
    EXPECT_EQ(0, common[134]);
    EXPECT_EQ(1, common[135]);
    // A8
    EXPECT_EQ(1, common[136]);
    EXPECT_EQ(0, common[137]);
    EXPECT_EQ(1, common[138]);
    EXPECT_EQ(0, common[139]);
    EXPECT_EQ(1, common[140]);
}

TEST(PartialLabelTests, EqualityTest)
{
    PartialLabel lbl1{ 1, 0, 1, 1, 0, 0, 1, 0 };
    PartialLabel lbl2{ 1, 0, 1, 1, 0, 0, 1, 0 };
    EXPECT_EQ(lbl1, lbl2);

    // Different last bit
    PartialLabel lbl3{ 1, 0, 1, 1, 0, 0, 1, 1 };
    EXPECT_NE(lbl1, lbl3);

    // Different bit count
    PartialLabel lbl4{ 1, 0, 1, 1, 0, 0, 1 };
    EXPECT_NE(lbl1, lbl4);

    // Empty labels
    PartialLabel empty1;
    PartialLabel empty2;
    EXPECT_EQ(empty1, empty2);
    EXPECT_NE(empty1, lbl1);

    // Full-size labels
    PartialLabel lbl5 = make_bytes<PartialLabel>(
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00);
    PartialLabel lbl6 = make_bytes<PartialLabel>(
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00);
    EXPECT_EQ(lbl5, lbl6);
}

TEST(PartialLabelTests, EqualityIgnoresTrailingBitsTest)
{
    // 8-bit label: only word 0's top byte is significant
    PartialLabel lbl1 = make_bytes<PartialLabel>(0xAB);

    array<uint8_t, PartialLabel::SaveSize> buffer{};
    lbl1.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    // Dirty trailing bytes in unused words
    array<uint8_t, PartialLabel::SaveSize> dirty_buffer = buffer;
    dirty_buffer[8] = 0xFF;   // word 1
    dirty_buffer[16] = 0xDE;  // word 2
    dirty_buffer[24] = 0xAD;  // word 3

    PartialLabel lbl2;
    lbl2.load(gsl::span<uint8_t, PartialLabel::SaveSize>(dirty_buffer));

    EXPECT_EQ(lbl1.bit_count(), lbl2.bit_count());
    EXPECT_EQ(lbl1, lbl2);

    // 72-bit label: word 0 fully used + 8 bits in word 1
    PartialLabel lbl3 = make_bytes<PartialLabel>(
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99);

    array<uint8_t, PartialLabel::SaveSize> buffer3{};
    lbl3.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer3));

    // Dirty unused words 2 and 3
    array<uint8_t, PartialLabel::SaveSize> dirty_buffer3 = buffer3;
    dirty_buffer3[16] = 0xFF;  // word 2
    dirty_buffer3[24] = 0xFF;  // word 3

    PartialLabel lbl4;
    lbl4.load(gsl::span<uint8_t, PartialLabel::SaveSize>(dirty_buffer3));

    EXPECT_EQ(lbl3, lbl4);
}

TEST(PartialLabelTests, LessThanTest)
{
    // Shorter bit count is less
    PartialLabel lbl1{ 1, 0, 1 };
    PartialLabel lbl2{ 1, 0, 1, 0 };
    EXPECT_TRUE(lbl1 < lbl2);
    EXPECT_FALSE(lbl2 < lbl1);

    // Same bit count, different significant bits
    PartialLabel lbl3{ 0, 1, 1 };
    PartialLabel lbl4{ 1, 0, 0 };
    EXPECT_TRUE(lbl3 < lbl4);
    EXPECT_FALSE(lbl4 < lbl3);

    // Equal labels: neither is less
    PartialLabel lbl5{ 1, 0, 1 };
    PartialLabel lbl6{ 1, 0, 1 };
    EXPECT_FALSE(lbl5 < lbl6);
    EXPECT_FALSE(lbl6 < lbl5);

    // Empty labels
    PartialLabel empty1;
    PartialLabel empty2;
    EXPECT_FALSE(empty1 < empty2);

    // Multi-word comparison
    PartialLabel lbl7 = make_bytes<PartialLabel>(
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x33);
    PartialLabel lbl8 = make_bytes<PartialLabel>(
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22, 0x44);
    EXPECT_TRUE(lbl7 < lbl8);
    EXPECT_FALSE(lbl8 < lbl7);
}

TEST(PartialLabelTests, LessThanIgnoresTrailingBitsTest)
{
    PartialLabel lbl1 = make_bytes<PartialLabel>(0xAB);

    array<uint8_t, PartialLabel::SaveSize> buffer{};
    lbl1.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    // Dirty trailing bytes in unused words
    array<uint8_t, PartialLabel::SaveSize> dirty_buffer = buffer;
    dirty_buffer[8] = 0xFF;
    dirty_buffer[16] = 0xFF;
    dirty_buffer[24] = 0xFF;

    PartialLabel lbl2;
    lbl2.load(gsl::span<uint8_t, PartialLabel::SaveSize>(dirty_buffer));

    EXPECT_FALSE(lbl1 < lbl2);
    EXPECT_FALSE(lbl2 < lbl1);
}

TEST(PartialLabelTests, SaveLoadRoundTripTest)
{
    // Empty label
    PartialLabel empty;
    array<uint8_t, PartialLabel::SaveSize> buffer{};
    empty.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    PartialLabel loaded_empty;
    loaded_empty.load(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));
    EXPECT_EQ(empty, loaded_empty);
    EXPECT_EQ(0, loaded_empty.bit_count());

    // Short label from bits
    PartialLabel bits_label{ 1, 0, 1, 1, 0, 0, 1, 0, 1, 1 };
    bits_label.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    PartialLabel loaded_bits;
    loaded_bits.load(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));
    EXPECT_EQ(bits_label, loaded_bits);
    EXPECT_EQ(10, loaded_bits.bit_count());
    for (uint32_t i = 0; i < loaded_bits.bit_count(); i++) {
        EXPECT_EQ(bits_label[i], loaded_bits[i]);
    }

    // Byte-aligned label
    PartialLabel byte_label = make_bytes<PartialLabel>(0xAB, 0xCD, 0xEF);
    byte_label.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    PartialLabel loaded_byte;
    loaded_byte.load(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));
    EXPECT_EQ(byte_label, loaded_byte);
    EXPECT_EQ(24, loaded_byte.bit_count());

    // Multi-word label (> 64 bits)
    PartialLabel multi_word = make_bytes<PartialLabel>(
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA);
    multi_word.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    PartialLabel loaded_multi;
    loaded_multi.load(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));
    EXPECT_EQ(multi_word, loaded_multi);
    EXPECT_EQ(80, loaded_multi.bit_count());

    // Full-size label (256 bits)
    PartialLabel full_label = make_bytes<PartialLabel>(
        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10);
    full_label.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    PartialLabel loaded_full;
    loaded_full.load(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));
    EXPECT_EQ(full_label, loaded_full);
    EXPECT_EQ(256, loaded_full.bit_count());
}

TEST(PartialLabelTests, LoadCleansTrailingBitsTest)
{
    // Create a 10-bit label, save it, then dirty the buffer's trailing bytes
    PartialLabel original{ 1, 0, 1, 1, 0, 0, 1, 0, 1, 1 };

    array<uint8_t, PartialLabel::SaveSize> buffer{};
    original.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    // Fill all trailing bytes with 0xFF
    array<uint8_t, PartialLabel::SaveSize> dirty_buffer;
    memset(dirty_buffer.data(), 0xFF, PartialLabel::SaveSize);
    // Preserve the original data
    memcpy(dirty_buffer.data(), buffer.data(), PartialLabel::SaveSize);
    // Now dirty the unused words
    dirty_buffer[8] = 0xFF;
    dirty_buffer[9] = 0xFF;
    dirty_buffer[16] = 0xFF;
    dirty_buffer[24] = 0xFF;

    PartialLabel loaded;
    loaded.load(gsl::span<uint8_t, PartialLabel::SaveSize>(dirty_buffer));

    // Should be equal despite dirty trailing bytes
    EXPECT_EQ(original, loaded);

    // Save the loaded label again - buffer should be clean
    array<uint8_t, PartialLabel::SaveSize> resaved_buffer{};
    loaded.save(gsl::span<uint8_t, PartialLabel::SaveSize>(resaved_buffer));

    // The resaved buffer should match the original clean buffer
    EXPECT_EQ(buffer, resaved_buffer);
}

TEST(PartialLabelTests, InitWithSubByteBitCountTest)
{
    // Construct from bytes 0xAB = 1010 1011 with only 5 significant bits: 10101
    vector<byte> input = { byte{ 0xAB } };
    PartialLabel lbl1(gsl::span<const byte>(input), 5);

    EXPECT_EQ(5, lbl1.bit_count());
    EXPECT_EQ(1, lbl1[0]);
    EXPECT_EQ(0, lbl1[1]);
    EXPECT_EQ(1, lbl1[2]);
    EXPECT_EQ(0, lbl1[3]);
    EXPECT_EQ(1, lbl1[4]);

    // Should equal a label built from the same 5 bits
    PartialLabel lbl2{ 1, 0, 1, 0, 1 };
    EXPECT_EQ(lbl1, lbl2);

    // Construct from 0xFF with 3 significant bits: 111
    input = { byte{ 0xFF } };
    PartialLabel lbl3(gsl::span<const byte>(input), 3);

    EXPECT_EQ(3, lbl3.bit_count());
    EXPECT_EQ(1, lbl3[0]);
    EXPECT_EQ(1, lbl3[1]);
    EXPECT_EQ(1, lbl3[2]);

    PartialLabel lbl4{ 1, 1, 1 };
    EXPECT_EQ(lbl3, lbl4);

    // Construct from two bytes 0xFF 0xFF with 10 significant bits
    input = { byte{ 0xFF }, byte{ 0xFF } };
    PartialLabel lbl5(gsl::span<const byte>(input), 10);

    EXPECT_EQ(10, lbl5.bit_count());
    for (uint32_t i = 0; i < 10; i++) {
        EXPECT_EQ(1, lbl5[i]);
    }

    // Should equal a label built from 10 one-bits
    PartialLabel lbl6{ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    EXPECT_EQ(lbl5, lbl6);

    // Save and reload should round-trip correctly
    array<uint8_t, PartialLabel::SaveSize> buffer{};
    lbl1.save(gsl::span<uint8_t, PartialLabel::SaveSize>(buffer));

    PartialLabel loaded;
    loaded.load(gsl::span<const uint8_t, PartialLabel::SaveSize>(buffer));
    EXPECT_EQ(lbl1, loaded);
}
