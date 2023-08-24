// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD

// OZKS
#include "oZKS/ecpoint.h"
#include "oZKS/utilities.h"

// GTest
#include "gtest/gtest.h"

using namespace ozks;
using namespace ozks::utils;
using namespace std;

TEST(ECPointTests, RandomScalarTest)
{
    ECPoint::scalar_type scalar1;
    ECPoint::scalar_type scalar2;

    ECPoint::MakeRandomNonzeroScalar(scalar1);
    ECPoint::MakeRandomNonzeroScalar(scalar2);

    EXPECT_FALSE(scalar1.is_zero());
    EXPECT_FALSE(scalar2.is_zero());
    EXPECT_TRUE(scalar1 != scalar2);
}

TEST(ECPointTests, SeededScalarTest)
{
    ECPoint::scalar_type scalar1;
    ECPoint::scalar_type scalar2;

    vector<byte> seed1 = make_bytes<vector<byte>>(1, 2, 3, 4, 5);
    vector<byte> seed2 = make_bytes<vector<byte>>(6, 5, 4, 3);

    ECPoint::MakeSeededScalar(seed1, scalar1);
    ECPoint::MakeSeededScalar(seed1, scalar2);

    EXPECT_FALSE(scalar1.is_zero());
    EXPECT_FALSE(scalar2.is_zero());
    EXPECT_TRUE(scalar1 == scalar2);

    ECPoint::MakeSeededScalar(seed2, scalar2);

    EXPECT_FALSE(scalar2.is_zero());
    EXPECT_FALSE(scalar1 == scalar2);

    ECPoint::scalar_type scalar3;
    ECPoint::MakeSeededScalar(seed2, scalar3);

    EXPECT_FALSE(scalar3.is_zero());
    EXPECT_FALSE(scalar1 == scalar3);
    EXPECT_TRUE(scalar2 == scalar3);
}
