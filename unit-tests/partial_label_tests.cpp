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
