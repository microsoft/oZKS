// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>

// OZKS
#include "oZKS/ecpoint.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace ozks;
using namespace ozks::utils;
using namespace std;

#ifdef OZKS_USE_OPENSSL_P256
TEST(P256PointTests, CreateTestNeutralElement)
{
    P256Point pt1, pt2;
    array<byte, P256Point::save_size> buf1, buf2;
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    // Check that pt1 is neutral element
    pt1.add(pt1);
    pt1.save(buf1);
    ASSERT_EQ(buf1, buf2);

    // Check that pt2 is neutral element
    pt2.add(pt2);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    // Check that p1+p2 behaves correctly
    pt1.add(pt2);
    pt1.save(buf1);
    ASSERT_EQ(buf1, buf2);

    P256Point::scalar_type scalar = {};
    scalar.set_byte(0, 2);
    pt1.scalar_multiply(scalar, true);
    pt1.save(buf1);
    ASSERT_EQ(buf1, buf2);

    P256Point::MakeRandomNonzeroScalar(scalar);
    pt1.scalar_multiply(scalar, true);
    pt1.save(buf1);
    ASSERT_EQ(buf1, buf2);
}

TEST(P256PointTests, MakeGenerator)
{
    P256Point pt0;
    P256Point pt1 = P256Point::MakeGenerator();
    P256Point pt2 = P256Point::MakeGenerator();

    array<byte, P256Point::save_size> buf0, buf1, buf2;
    pt0.save(buf0);
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);
    ASSERT_NE(buf0, buf1);

    pt1.add(pt1);
    pt1.save(buf1);
    ASSERT_NE(buf1, buf2);
    pt2.add(pt2);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    pt1.add(pt2);
    pt1.save(buf1);
    P256Point::scalar_type scalar_four = {};
    scalar_four.set_byte(0, 4);
    pt0 = P256Point::MakeGeneratorMultiple(scalar_four);
    pt0.save(buf0);
    ASSERT_EQ(buf0, buf1);
}

TEST(P256PointTests, MakeGeneratorMultiple)
{
    P256Point::scalar_type scalar = {};
    P256Point pt0;
    P256Point pt1 = P256Point::MakeGeneratorMultiple(scalar);
    array<byte, P256Point::save_size> buf0, buf1;
    pt0.save(buf0);
    pt1.save(buf1);
    ASSERT_EQ(buf0, buf1);

    scalar.set_byte(0, 1);
    pt0 = P256Point::MakeGenerator();
    pt1 = P256Point::MakeGeneratorMultiple(scalar);
    pt0.save(buf0);
    pt1.save(buf1);
    ASSERT_EQ(buf0, buf1);
}

TEST(P256PointTests, AddScalar)
{
    P256Point::scalar_type s1 = {}, s2 = {}, s3;
    array<byte, P256Point::order_size> scalar_data;
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x10));
    s3.load(scalar_data);
    P256Point::AddScalar(s1, s2, s3);
    ASSERT_EQ(s1, s2);
    ASSERT_EQ(s1, s3);

    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x10));
    s2.load(scalar_data);
    P256Point::AddScalar(s1, s2, s3);
    ASSERT_NE(s1, s2);
    ASSERT_EQ(s2, s3);

    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x34));
    s1.load(scalar_data);
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x43));
    s2.load(scalar_data);
    P256Point::AddScalar(s1, s2, s3);
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x77));
    s1.load(scalar_data);
    ASSERT_EQ(s1, s3);

    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x34));
    s1.load(scalar_data);
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0xCB));
    s2.load(scalar_data);
    P256Point::AddScalar(s1, s2, s3);

    // Let's wrap around the modulus
    key_type expected = utils::make_bytes<key_type>(
        0xae,
        0xda,
        0x9c,
        0x03,
        0x3d,
        0x35,
        0x46,
        0x0c,
        0x7b,
        0x61,
        0xe8,
        0x58,
        0x52,
        0x05,
        0x19,
        0x43,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
        0x00,
        0x00,
        0x00);
    s1.load(P256Point::scalar_span_const_type{ expected.data(), P256Point::order_size });
    ASSERT_EQ(s1, s3);
}

TEST(P256PointTests, SubtractScalar)
{
    P256Point::scalar_type s1 = {}, s2 = {}, s3;
    array<byte, P256Point::order_size> scalar_data;
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x10));
    s3.load(scalar_data);
    P256Point::SubtractScalar(s1, s2, s3);
    ASSERT_EQ(s1, s2);
    ASSERT_EQ(s1, s3);

    s2.load(scalar_data);
    P256Point::SubtractScalar(s2, s2, s1);
    s3 = {};
    ASSERT_EQ(s1, s3);

    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x77));
    s1.load(scalar_data);
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x43));
    s2.load(scalar_data);
    P256Point::SubtractScalar(s1, s2, s3);
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x34));
    s1.load(scalar_data);
    ASSERT_EQ(s1, s3);

    // Let's wrap around the modulus
    key_type sum = utils::make_bytes<key_type>(
        0xae,
        0xda,
        0x9c,
        0x03,
        0x3d,
        0x35,
        0x46,
        0x0c,
        0x7b,
        0x61,
        0xe8,
        0x58,
        0x52,
        0x05,
        0x19,
        0x43,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
        0x00,
        0x00,
        0x00);
    s3.load(P256Point::scalar_span_const_type{ sum.data(), P256Point::order_size });
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x34));
    s1.load(scalar_data);
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0xCB));
    s2.load(scalar_data);
    P256Point::SubtractScalar(s3, s1, s1);
    ASSERT_EQ(s1, s2);
}

TEST(P256PointTests, MultiplyScalar)
{
    P256Point::scalar_type s1 = {}, s2, s3 = {};
    array<byte, P256Point::order_size> scalar_data;
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0x10));
    s2.load(scalar_data);
    P256Point::MultiplyScalar(s1, s2, s2);
    ASSERT_EQ(s2, s3);

    s2.load(scalar_data);
    s1.set_byte(0, 1);
    P256Point::MultiplyScalar(s1, s2, s3);
    ASSERT_EQ(s2, s3);

    scalar_data = { static_cast<byte>(0), static_cast<byte>(3) };
    s1.load(scalar_data);
    scalar_data = { static_cast<byte>(0), static_cast<byte>(4) };
    s2.load(scalar_data);
    scalar_data = { static_cast<byte>(0), static_cast<byte>(0), static_cast<byte>(12) };
    s3.load(scalar_data);

    P256Point::MultiplyScalar(s1, s2, s2);
    ASSERT_EQ(s2, s3);

    scalar_data = { static_cast<byte>(0), static_cast<byte>(1) }; // 2^8
    s1.load(scalar_data);
    P256Point::MultiplyScalar(s1, s1, s1); // 2^16
    P256Point::MultiplyScalar(s1, s1, s1); // 2^32
    P256Point::MultiplyScalar(s1, s1, s1); // 2^64
    P256Point::MultiplyScalar(s1, s1, s1); // 2^128
    P256Point::MultiplyScalar(s1, s1, s1); // 2^256 - mod

    key_type expected = utils::make_bytes<key_type>(
        0xaf,
        0xda,
        0x9c,
        0x03,
        0x3d,
        0x35,
        0x46,
        0x0c,
        0x7b,
        0x61,
        0xe8,
        0x58,
        0x52,
        0x05,
        0x19,
        0x43,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
        0x00,
        0x00,
        0x00);
    s2.load(P256Point::scalar_span_const_type{ expected.data(), P256Point::order_size });
    ASSERT_EQ(s1, s2);
}

TEST(P256PointTests, MakeRandomNonzeroScalar)
{
    P256Point::scalar_type s1 = {}, s2 = {}, zero = {};
    P256Point::MakeRandomNonzeroScalar(s1);
    ASSERT_NE(s1, zero);
    P256Point::MakeRandomNonzeroScalar(s2);
    ASSERT_NE(s2, zero);
    ASSERT_NE(s1, s2);
}

TEST(P256PointTests, ReduceModOrder)
{
    P256Point::scalar_type s1 = {}, s2 = {}, s3 = {};

    P256Point::ReduceModOrder(s1);
    ASSERT_EQ(s1, s2);

    s1.set_byte(0, 1);
    s2.set_byte(0, 1);
    P256Point::ReduceModOrder(s1);
    ASSERT_EQ(s1, s2);

    array<byte, P256Point::order_size> scalar_data;
    fill(scalar_data.begin(), scalar_data.end(), static_cast<byte>(0xff));
    s3.load(scalar_data);
    P256Point::ReduceModOrder(s3);
    key_type expected = utils::make_bytes<key_type>(
        0xae,
        0xda,
        0x9c,
        0x03,
        0x3d,
        0x35,
        0x46,
        0x0c,
        0x7b,
        0x61,
        0xe8,
        0x58,
        0x52,
        0x05,
        0x19,
        0x43,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xff,
        0xff,
        0xff,
        0xff,
        0x00,
        0x00,
        0x00,
        0x00);
    s2.load(P256Point::scalar_span_const_type{ expected.data(), P256Point::order_size });
    ASSERT_EQ(s2, s3);
}

TEST(P256PointTests, SetPoint)
{
    P256Point pt1 = P256Point::MakeGenerator();
    P256Point pt2;

    array<byte, P256Point::save_size> buf1, buf2;
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_NE(buf1, buf2);

    pt2 = pt1;
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    P256Point::scalar_type s;
    P256Point::MakeRandomNonzeroScalar(s);
    pt1 = P256Point::MakeGeneratorMultiple(s);
    pt1.save(buf1);
    ASSERT_NE(buf1, buf2);

    pt2 = pt1;
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);
}

TEST(P256PointTests, ScalarMultiply)
{
    P256Point pt1, pt2;
    array<byte, P256Point::save_size> buf1, buf2, buf3;
    pt1.save(buf1);

    P256Point::scalar_type s;
    P256Point::MakeRandomNonzeroScalar(s);
    ASSERT_TRUE(pt1.scalar_multiply(s, true));
    pt1.save(buf2);
    ASSERT_EQ(buf1, buf2);

    ASSERT_TRUE(pt2.scalar_multiply(s, false));
    pt2.save(buf3);
    ASSERT_EQ(buf1, buf3);

    pt1 = P256Point::MakeGenerator();
    pt2 = P256Point::MakeGenerator();
    P256Point::scalar_type s1 = {}, s2 = {}, s3 = {};

    array<byte, P256Point::order_size> scalar_data;
    scalar_data = { static_cast<byte>(0), static_cast<byte>(3) };
    s1.load(scalar_data);
    scalar_data = { static_cast<byte>(0), static_cast<byte>(4) };
    s2.load(scalar_data);
    scalar_data = { static_cast<byte>(0), static_cast<byte>(0), static_cast<byte>(12) };
    s3.load(scalar_data);

    pt1.scalar_multiply(s1, false); // The clear cofactor flag is ignored
    pt1.scalar_multiply(s2, true);
    pt2.scalar_multiply(s3, true);
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    P256Point::MakeRandomNonzeroScalar(s1);
    P256Point::MakeRandomNonzeroScalar(s2);
    P256Point::MultiplyScalar(s1, s2, s3);
    pt1 = P256Point::MakeGeneratorMultiple(s1);
    pt1.scalar_multiply(s2, false);
    pt2 = P256Point::MakeGeneratorMultiple(s3);
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);
}

TEST(P256PointTests, InvertScalar)
{
    P256Point::scalar_type s1, s2;
    P256Point::MakeRandomNonzeroScalar(s1);
    P256Point::InvertScalar(s1, s2);
    P256Point::MultiplyScalar(s1, s2, s1);

    P256Point::scalar_type one = {};
    one.set_byte(0, 1);
    ASSERT_EQ(s1, one);
}

TEST(P256PointTests, DoubleScalarMultiply)
{
    P256Point::scalar_type s1 = {}, s2 = {};
    P256Point pt1 = P256Point::MakeGenerator();
    pt1.double_scalar_multiply(s1, s2);

    P256Point pt2;
    array<byte, P256Point::save_size> buf1, buf2;
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    s1.set_byte(0, 1);
    s2.set_byte(0, 0);
    pt1 = P256Point::MakeGenerator();
    pt2 = pt1;
    pt1.double_scalar_multiply(s1, s2);
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    s1.set_byte(0, 1);
    s2.set_byte(0, 1);
    P256Point::scalar_type s3 = {};
    s3.set_byte(0, 2);
    pt1 = P256Point::MakeGenerator();
    pt2 = P256Point::MakeGeneratorMultiple(s3);
    pt1.double_scalar_multiply(s1, s2);
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    s1.set_byte(0, 2);
    s2.set_byte(0, 3);
    s3.set_byte(0, 9);
    pt1 = P256Point::MakeGeneratorMultiple(s2);
    pt2 = P256Point::MakeGeneratorMultiple(s3);
    pt1.double_scalar_multiply(s1, s2);
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    P256Point::MakeRandomNonzeroScalar(s1);
    P256Point::MakeRandomNonzeroScalar(s2);
    P256Point::MakeRandomNonzeroScalar(s3);
    pt1 = P256Point::MakeGeneratorMultiple(s1);
    pt1.double_scalar_multiply(s2, s3);
    P256Point::scalar_type s4;
    P256Point::MultiplyScalar(s1, s2, s4);
    P256Point::AddScalar(s4, s3, s4);
    pt2 = P256Point::MakeGeneratorMultiple(s4);
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);
}

TEST(P256PointTests, Add)
{
    P256Point pt1, pt2, pt3;
    pt1.add(pt2);
    array<byte, P256Point::save_size> buf1, buf2;
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    pt1 = P256Point::MakeGenerator();
    pt2 = P256Point();
    pt3 = P256Point::MakeGenerator();
    pt1.add(pt2);
    pt1.save(buf1);
    pt3.save(buf2);
    ASSERT_EQ(buf1, buf2);

    pt1 = P256Point::MakeGenerator();
    pt2 = P256Point();
    pt3 = P256Point::MakeGenerator();
    pt2.add(pt1);
    pt2.save(buf1);
    pt3.save(buf2);
    ASSERT_EQ(buf1, buf2);

    P256Point::scalar_type s1, s2, s3;
    P256Point::MakeRandomNonzeroScalar(s1);
    P256Point::MakeRandomNonzeroScalar(s2);
    P256Point::AddScalar(s1, s2, s3);
    pt1 = P256Point::MakeGeneratorMultiple(s1);
    pt2 = P256Point::MakeGeneratorMultiple(s2);
    pt3 = P256Point::MakeGeneratorMultiple(s3);
    pt1.add(pt2);
    pt1.save(buf1);
    pt3.save(buf2);
    ASSERT_EQ(buf1, buf2);
}

TEST(P256PointTests, SaveLoad)
{
    stringstream ss;

    P256Point::scalar_type s1, s2, s3;
    P256Point::MakeRandomNonzeroScalar(s1);
    P256Point::MakeRandomNonzeroScalar(s2);
    P256Point::MakeRandomNonzeroScalar(s3);

    P256Point pt1, pt2, pt3, pt4, pt5, pt6;
    pt1 = P256Point::MakeGeneratorMultiple(s1);
    pt2 = P256Point::MakeGeneratorMultiple(s2);
    pt3 = P256Point::MakeGeneratorMultiple(s3);

    pt1.save(ss);
    pt2.save(ss);
    pt3.save(ss);

    pt4.load(ss);
    pt5.load(ss);
    pt6.load(ss);

    array<byte, P256Point::save_size> buf1, buf2;
    pt1.save(buf1);
    pt4.save(buf2);
    ASSERT_EQ(buf1, buf2);

    pt2.save(buf1);
    pt5.save(buf2);
    ASSERT_EQ(buf1, buf2);

    pt3.save(buf1);
    pt6.save(buf2);
    ASSERT_EQ(buf1, buf2);
}

TEST(P256PointTests, Hash2Curve)
{
    auto str_to_key_type = [](const string &str) -> key_type {
        key_type out;
        transform(str.c_str(), str.c_str() + str.size(), back_inserter(out), [](unsigned char ch) {
            return static_cast<byte>(ch);
        });
        return out;
    };

    string str1 = "hello worla", str2 = "hello worla";
    P256Point pt1(str_to_key_type(str1));
    P256Point pt2(str_to_key_type(str1));

    array<byte, P256Point::save_size> buf1{}, buf2{};
    pt1.save(buf1);
    pt2.save(buf2);
    ASSERT_EQ(buf1, buf2);

    str2 = "hello worlh";
    pt2 = P256Point(str_to_key_type(str2));
    pt2.save(buf2);
    ASSERT_NE(buf1, buf2);
}

#endif
