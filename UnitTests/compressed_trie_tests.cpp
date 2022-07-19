// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#include "oZKS/compressed_trie.h"
#include "oZKS/fourq/random.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"
#include "gtest/gtest.h"

using namespace std;
using namespace ozks;
using namespace ozks::utils;

namespace {
    /**
    Get random bytes and throw if unsuccessful
    */
    void get_random_bytes(unsigned char *dest, size_t size)
    {
        if (!random_bytes(dest, static_cast<unsigned int>(size)))
            throw runtime_error("Failed to get random bytes");
    }
} // namespace

TEST(CompressedTrie, InsertTest)
{
    label_type bytes1 = make_bytes(0xAA, 0xBB, 0xCC, 0xDD, 0xEE);
    label_type bytes2 = make_bytes(0x01, 0x01, 0x01, 0x01, 0x01);
    label_type bytes3 = make_bytes(0x10, 0x10, 0x10, 0x10, 0x10);
    label_type bytes4 = make_bytes(0x80, 0x80, 0x80, 0x80, 0x80);
    label_type bytes5 = make_bytes(0xC0, 0xC0, 0xC0, 0xC0, 0xC0);
    label_type bytes6 = make_bytes(0xE0, 0xE0, 0xE0, 0xE0, 0xE0);
    label_type bytes7 = make_bytes(0xF0, 0xF0, 0xF0, 0xF0, 0xF0);
    append_proof_type append_proof;

    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);
    trie.insert(bytes1, make_bytes(0x01, 0x02, 0x03), append_proof);
    string status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:(null):r:1010101010111011110011001101110111101110;"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);");

    trie.insert(bytes2, make_bytes(0x02, 0x03, 0x04), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:0000000100000001000000010000000100000001:r:1010101010111011110011001101110111101110;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);");

    trie.insert(bytes3, make_bytes(0x03, 0x04, 0x05), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1010101010111011110011001101110111101110;"
        "n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);");

    trie.insert(bytes4, make_bytes(0x04, 0x05, 0x06), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:10;n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:10:l:1000000010000000100000001000000010000000:r:"
        "1010101010111011110011001101110111101110;"
        "n:1000000010000000100000001000000010000000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);");

    trie.insert(bytes5, make_bytes(0x05, 0x06, 0x07), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:1:l:10:r:1100000011000000110000001100000011000000;"
        "n:10:l:1000000010000000100000001000000010000000:r:"
        "1010101010111011110011001101110111101110;"
        "n:1000000010000000100000001000000010000000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);"
        "n:1100000011000000110000001100000011000000:l:(null):r:(null);");

    trie.insert(bytes6, make_bytes(0x06, 0x07, 0x08), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:1:l:10:r:11;"
        "n:10:l:1000000010000000100000001000000010000000:r:"
        "1010101010111011110011001101110111101110;"
        "n:1000000010000000100000001000000010000000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);"
        "n:11:l:1100000011000000110000001100000011000000:r:"
        "1110000011100000111000001110000011100000;"
        "n:1100000011000000110000001100000011000000:l:(null):r:(null);"
        "n:1110000011100000111000001110000011100000:l:(null):r:(null);");

    trie.insert(bytes7, make_bytes(0x07, 0x08, 0x09), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;n:000:l:0000000100000001000000010000000100000001:r:"
        "0001000000010000000100000001000000010000;"
        "n:0000000100000001000000010000000100000001:l:(null):r:(null);"
        "n:0001000000010000000100000001000000010000:l:(null):r:(null);"
        "n:1:l:10:r:11;"
        "n:10:l:1000000010000000100000001000000010000000:r:"
        "1010101010111011110011001101110111101110;"
        "n:1000000010000000100000001000000010000000:l:(null):r:(null);"
        "n:1010101010111011110011001101110111101110:l:(null):r:(null);"
        "n:11:l:1100000011000000110000001100000011000000:r:111;"
        "n:1100000011000000110000001100000011000000:l:(null):r:(null);"
        "n:111:l:1110000011100000111000001110000011100000:r:"
        "1111000011110000111100001111000011110000;"
        "n:1110000011100000111000001110000011100000:l:(null):r:(null);"
        "n:1111000011110000111100001111000011110000:l:(null):r:(null);");
}

TEST(CompressedTrie, InsertSimpleTest)
{
    label_type bytes1 = make_bytes(0x11);
    label_type bytes2 = make_bytes(0x01);
    label_type bytes3 = make_bytes(0xEE);
    label_type bytes4 = make_bytes(0xAA);
    label_type bytes5 = make_bytes(0xCC);
    label_type bytes6 = make_bytes(0xFF);
    append_proof_type append_proof;

    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);
    trie.insert(bytes1, make_bytes(0xA0, 0xB0, 0xC0), append_proof);
    string status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:00010001:r:(null);"
        "n:00010001:l:(null):r:(null);");
    EXPECT_EQ(1, trie.epoch());

    trie.insert(bytes2, make_bytes(0xA1, 0xB1, 0xC1), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:(null);"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);");
    EXPECT_EQ(2, trie.epoch());

    trie.insert(bytes3, make_bytes(0xA2, 0xB2, 0xC2), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:11101110;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:11101110:l:(null):r:(null);");
    EXPECT_EQ(3, trie.epoch());

    trie.insert(bytes4, make_bytes(0xA3, 0xB3, 0xC3), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:1:l:10101010:r:11101110;"
        "n:10101010:l:(null):r:(null);"
        "n:11101110:l:(null):r:(null);");
    EXPECT_EQ(4, trie.epoch());

    trie.insert(bytes5, make_bytes(0xA4, 0xB4, 0xC4), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:1:l:10101010:r:11;"
        "n:10101010:l:(null):r:(null);"
        "n:11:l:11001100:r:11101110;"
        "n:11001100:l:(null):r:(null);"
        "n:11101110:l:(null):r:(null);");
    EXPECT_EQ(5, trie.epoch());

    trie.insert(bytes6, make_bytes(0xA5, 0xB5, 0xC5), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:1;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null);"
        "n:00010001:l:(null):r:(null);"
        "n:1:l:10101010:r:11;"
        "n:10101010:l:(null):r:(null);"
        "n:11:l:11001100:r:111;"
        "n:11001100:l:(null):r:(null);"
        "n:111:l:11101110:r:11111111;"
        "n:11101110:l:(null):r:(null);"
        "n:11111111:l:(null):r:(null);");
    EXPECT_EQ(6, trie.epoch());
}

TEST(CompressedTrie, AppendProofTest)
{
    label_type bytes1 = make_bytes(0x11);
    label_type bytes2 = make_bytes(0x01);
    label_type bytes3 = make_bytes(0xEE);
    label_type bytes4 = make_bytes(0xAA);
    label_type bytes5 = make_bytes(0xCC);
    label_type bytes6 = make_bytes(0xFF);
    append_proof_type append_proof;
    partial_label_type partial_label;

    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);
    trie.insert(bytes1, make_bytes(0xA0, 0xB0, 0xC0), append_proof);
    EXPECT_EQ(1, append_proof.size());
    partial_label = partial_label_type{ 0, 0, 0, 1, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    EXPECT_EQ(1, trie.epoch());

    trie.insert(bytes2, make_bytes(0xA1, 0xB1, 0xC1), append_proof);
    EXPECT_EQ(2, append_proof.size());
    partial_label = partial_label_type{ 0, 0, 0, 0, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = partial_label_type{ 0, 0, 0, 1, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    EXPECT_EQ(2, trie.epoch());

    trie.insert(bytes3, make_bytes(0xA2, 0xB2, 0xC2), append_proof);
    EXPECT_EQ(2, append_proof.size());
    partial_label = partial_label_type{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    EXPECT_EQ(3, trie.epoch());

    trie.insert(bytes4, make_bytes(0xA3, 0xB3, 0xC3), append_proof);
    EXPECT_EQ(3, append_proof.size());
    partial_label = partial_label_type{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = partial_label_type{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[2].first);
    EXPECT_EQ(4, trie.epoch());

    trie.insert(bytes5, make_bytes(0xA4, 0xB4, 0xC4), append_proof);
    EXPECT_EQ(4, append_proof.size());
    partial_label = partial_label_type{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = partial_label_type{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    partial_label = partial_label_type{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[2].first);
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[3].first);
    EXPECT_EQ(5, trie.epoch());

    trie.insert(bytes6, make_bytes(0xA5, 0xB5, 0xC5), append_proof);
    EXPECT_EQ(5, append_proof.size());
    partial_label = partial_label_type{ 1, 1, 1, 1, 1, 1, 1, 1 };
    EXPECT_EQ(partial_label, append_proof[0].first);
    partial_label = partial_label_type{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[1].first);
    partial_label = partial_label_type{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[2].first);
    partial_label = partial_label_type{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proof[3].first);
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proof[4].first);
    EXPECT_EQ(6, trie.epoch());
}

TEST(CompressedTrie, InsertSimpleBatchTest)
{
    append_proof_batch_type append_proofs;
    label_payload_batch_type label_payload_batch{
        pair<vector<byte>, vector<byte>>{ make_bytes(0x11), make_bytes(0xA0, 0xB0, 0xC0) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0x01), make_bytes(0xA1, 0xB1, 0xC1) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xEE), make_bytes(0xA2, 0xB2, 0xC2) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xAA), make_bytes(0xA3, 0xB3, 0xC3) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xCC), make_bytes(0xA4, 0xB4, 0xC4) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xFF), make_bytes(0xA5, 0xB5, 0xC5) }
    };

    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);
    EXPECT_EQ(0, trie.epoch());

    trie.insert(label_payload_batch, append_proofs);
    string status = trie.to_string(/* include_payload */ true);
    EXPECT_EQ(
        status,
        "n::l:000:r:1;"
        "n:000:l:00000001:r:00010001;"
        "n:00000001:l:(null):r:(null):p:a1b1c1;"
        "n:00010001:l:(null):r:(null):p:a0b0c0;"
        "n:1:l:10101010:r:11;"
        "n:10101010:l:(null):r:(null):p:a3b3c3;"
        "n:11:l:11001100:r:111;"
        "n:11001100:l:(null):r:(null):p:a4b4c4;"
        "n:111:l:11101110:r:11111111;"
        "n:11101110:l:(null):r:(null):p:a2b2c2;"
        "n:11111111:l:(null):r:(null):p:a5b5c5;");
    EXPECT_EQ(1, trie.epoch());
}

TEST(CompressedTrie, AppendProofBatchTest)
{
    label_payload_batch_type label_payload_batch{
        pair<vector<byte>, vector<byte>>{ make_bytes(0x11), make_bytes(0xA0, 0xB0, 0xC0) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0x01), make_bytes(0xA1, 0xB1, 0xC1) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xEE), make_bytes(0xA2, 0xB2, 0xC2) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xAA), make_bytes(0xA3, 0xB3, 0xC3) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xCC), make_bytes(0xA4, 0xB4, 0xC4) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xFF), make_bytes(0xA5, 0xB5, 0xC5) }
    };
    append_proof_batch_type append_proofs;
    partial_label_type partial_label;

    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);
    EXPECT_EQ(0, trie.epoch());

    trie.insert(label_payload_batch, append_proofs);
    EXPECT_EQ(6, append_proofs.size());
    EXPECT_EQ(3, append_proofs[0].size());
    partial_label = partial_label_type{ 0, 0, 0, 1, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proofs[0][0].first);
    partial_label = partial_label_type{ 0, 0, 0, 0, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proofs[0][1].first);
    partial_label = partial_label_type{ 1 };
    EXPECT_EQ(partial_label, append_proofs[0][2].first);

    EXPECT_EQ(3, append_proofs[1].size());
    partial_label = partial_label_type{ 0, 0, 0, 0, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proofs[1][0].first);
    partial_label = partial_label_type{ 0, 0, 0, 1, 0, 0, 0, 1 };
    EXPECT_EQ(partial_label, append_proofs[1][1].first);
    partial_label = partial_label_type{ 1 };
    EXPECT_EQ(partial_label, append_proofs[1][2].first);

    EXPECT_EQ(5, append_proofs[2].size());
    partial_label = partial_label_type{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[2][0].first);
    partial_label = partial_label_type{ 1, 1, 1, 1, 1, 1, 1, 1 };
    EXPECT_EQ(partial_label, append_proofs[2][1].first);
    partial_label = partial_label_type{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[2][2].first);
    partial_label = partial_label_type{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[2][3].first);
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[2][4].first);

    EXPECT_EQ(3, append_proofs[3].size());
    partial_label = partial_label_type{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[3][0].first);
    partial_label = partial_label_type{ 1, 1 };
    EXPECT_EQ(partial_label, append_proofs[3][1].first);
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[3][2].first);

    EXPECT_EQ(4, append_proofs[4].size());
    partial_label = partial_label_type{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[4][0].first);
    partial_label = partial_label_type{ 1, 1, 1 };
    EXPECT_EQ(partial_label, append_proofs[4][1].first);
    partial_label = partial_label_type{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[4][2].first);
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[4][3].first);

    EXPECT_EQ(5, append_proofs[5].size());
    partial_label = partial_label_type{ 1, 1, 1, 1, 1, 1, 1, 1 };
    EXPECT_EQ(partial_label, append_proofs[5][0].first);
    partial_label = partial_label_type{ 1, 1, 1, 0, 1, 1, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[5][1].first);
    partial_label = partial_label_type{ 1, 1, 0, 0, 1, 1, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[5][2].first);
    partial_label = partial_label_type{ 1, 0, 1, 0, 1, 0, 1, 0 };
    EXPECT_EQ(partial_label, append_proofs[5][3].first);
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, append_proofs[5][4].first);

    EXPECT_EQ(1, trie.epoch());
}

TEST(CompressedTrie, InsertInPartialLabelTest)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);

    label_type bytes = make_bytes(0x07);
    append_proof_type append_proof;
    trie.insert(bytes, make_bytes(0xF0, 0xE0, 0xD0), append_proof);
    string status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:00000111:r:(null);"
        "n:00000111:l:(null):r:(null);");

    bytes = make_bytes(0x04);
    trie.insert(bytes, make_bytes(0xF1, 0xE1, 0xD1), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000001:r:(null);"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);");

    bytes = make_bytes(0x08);
    trie.insert(bytes, make_bytes(0xF2, 0xE2, 0xD2), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:0000:r:(null);"
        "n:0000:l:000001:r:00001000;"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001000:l:(null):r:(null);");

    bytes = make_bytes(0x0C);
    trie.insert(bytes, make_bytes(0xF3, 0xE3, 0xD3), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:0000:r:(null);"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001:l:00001000:r:00001100;"
        "n:00001000:l:(null):r:(null);"
        "n:00001100:l:(null):r:(null);");

    bytes = make_bytes(0x10);
    trie.insert(bytes, make_bytes(0xF4, 0xE4, 0xD4), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:(null);"
        "n:000:l:0000:r:00010000;"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001:l:00001000:r:00001100;"
        "n:00001000:l:(null):r:(null);"
        "n:00001100:l:(null):r:(null);"
        "n:00010000:l:(null):r:(null);");

    bytes = make_bytes(0x05);
    trie.insert(bytes, make_bytes(0xF5, 0xE5, 0xD5), append_proof);
    status = trie.to_string();
    EXPECT_EQ(
        status,
        "n::l:000:r:(null);"
        "n:000:l:0000:r:00010000;"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:0000010:r:00000111;"
        "n:0000010:l:00000100:r:00000101;"
        "n:00000100:l:(null):r:(null);"
        "n:00000101:l:(null):r:(null);"
        "n:00000111:l:(null):r:(null);"
        "n:00001:l:00001000:r:00001100;"
        "n:00001000:l:(null):r:(null);"
        "n:00001100:l:(null):r:(null);"
        "n:00010000:l:(null):r:(null);");

    status = trie.to_string(/* include_payload */ true);
    EXPECT_EQ(
        status,
        "n::l:000:r:(null);"
        "n:000:l:0000:r:00010000;"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:0000010:r:00000111;"
        "n:0000010:l:00000100:r:00000101;"
        "n:00000100:l:(null):r:(null):p:f1e1d1;"
        "n:00000101:l:(null):r:(null):p:f5e5d5;"
        "n:00000111:l:(null):r:(null):p:f0e0d0;"
        "n:00001:l:00001000:r:00001100;"
        "n:00001000:l:(null):r:(null):p:f2e2d2;"
        "n:00001100:l:(null):r:(null):p:f3e3d3;"
        "n:00010000:l:(null):r:(null):p:f4e4d4;");
}

TEST(CompressedTrie, LookupTest)
{
    label_payload_batch_type label_payload_batch{
        pair<vector<byte>, vector<byte>>{ make_bytes(0x11, 0x01), make_bytes(0xA0, 0xB0, 0xC0) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0x01, 0x02), make_bytes(0xA1, 0xB1, 0xC1) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xEE, 0x03), make_bytes(0xA2, 0xB2, 0xC2) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xAA, 0x04), make_bytes(0xA3, 0xB3, 0xC3) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xCC, 0x05), make_bytes(0xA4, 0xB4, 0xC4) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xFF, 0x06), make_bytes(0xA5, 0xB5, 0xC5) }
    };
    append_proof_batch_type append_proofs;

    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);

    trie.insert(label_payload_batch, append_proofs);

    string status = trie.to_string();
    EXPECT_EQ(
        "n::l:000:r:1;"
        "n:000:l:0000000100000010:r:0001000100000001;"
        "n:0000000100000010:l:(null):r:(null);"
        "n:0001000100000001:l:(null):r:(null);"
        "n:1:l:1010101000000100:r:11;"
        "n:1010101000000100:l:(null):r:(null);"
        "n:11:l:1100110000000101:r:111;"
        "n:1100110000000101:l:(null):r:(null);"
        "n:111:l:1110111000000011:r:1111111100000110;"
        "n:1110111000000011:l:(null):r:(null);"
        "n:1111111100000110:l:(null):r:(null);",
        status);

    lookup_path_type result;

    EXPECT_EQ(false, trie.lookup(make_bytes(0xFF, 0xFF), result));
    EXPECT_EQ(true, trie.lookup(make_bytes(0xFF, 0x06), result));
    EXPECT_EQ(5, result.size());

    // First is the searched node
    EXPECT_EQ(make_bytes(0xFF, 0x06), bools_to_bytes(result[0].first));
    // Second is the node sibling, if any
    EXPECT_EQ(make_bytes(0xEE, 0x03), bools_to_bytes(result[1].first));
    // Third is the sibling of the direct parent of the search node
    EXPECT_EQ(make_bytes(0xCC, 0x05), bools_to_bytes(result[2].first));
    // Fourth is the sibling of the grandparent of the search node
    EXPECT_EQ(make_bytes(0xAA, 0x04), bools_to_bytes(result[3].first));
    // Fifth is the sibling of the great-grandparent of the search node
    partial_label_type partial_label{ 0, 0, 0 };
    EXPECT_EQ(partial_label, result[4].first);

    EXPECT_EQ(false, trie.lookup(make_bytes(0x01, 0x03), result));
    EXPECT_EQ(true, trie.lookup(make_bytes(0x11, 0x01), result));
    EXPECT_EQ(3, result.size());

    // Searched node
    EXPECT_EQ(make_bytes(0x11, 0x01), bools_to_bytes(result[0].first));
    // Sibling
    EXPECT_EQ(make_bytes(0x01, 0x02), bools_to_bytes(result[1].first));
    // Sibling of the parent
    partial_label = partial_label_type{ 1 };
    EXPECT_EQ(partial_label, result[2].first);
}

TEST(CompressedTrie, FailedLookupTest)
{
    label_payload_batch_type label_payload_batch{
        pair<vector<byte>, vector<byte>>{ make_bytes(0x11, 0x01), make_bytes(0xA0, 0xB0, 0xC0) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0x01, 0x02), make_bytes(0xA1, 0xB1, 0xC1) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xEE, 0x03), make_bytes(0xA2, 0xB2, 0xC2) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xAA, 0x04), make_bytes(0xA3, 0xB3, 0xC3) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xCC, 0x05), make_bytes(0xA4, 0xB4, 0xC4) },
        pair<vector<byte>, vector<byte>>{ make_bytes(0xFF, 0x06), make_bytes(0xA5, 0xB5, 0xC5) }
    };
    append_proof_batch_type append_proofs;

    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);

    trie.insert(label_payload_batch, append_proofs);

    string status = trie.to_string();
    EXPECT_EQ(
        "n::l:000:r:1;"
        "n:000:l:0000000100000010:r:0001000100000001;"
        "n:0000000100000010:l:(null):r:(null);"
        "n:0001000100000001:l:(null):r:(null);"
        "n:1:l:1010101000000100:r:11;"
        "n:1010101000000100:l:(null):r:(null);"
        "n:11:l:1100110000000101:r:111;"
        "n:1100110000000101:l:(null):r:(null);"
        "n:111:l:1110111000000011:r:1111111100000110;"
        "n:1110111000000011:l:(null):r:(null);"
        "n:1111111100000110:l:(null):r:(null);",
        status);

    lookup_path_type result;

    EXPECT_EQ(false, trie.lookup(make_bytes(0xFF, 0xFF), result));
    EXPECT_EQ(6, result.size());
    EXPECT_EQ(make_bytes(0xEE, 0x03), bools_to_bytes(result[0].first));
    EXPECT_EQ(make_bytes(0xFF, 0x06), bools_to_bytes(result[1].first));
    partial_label_type partial_label{ 1, 1, 1 };
    EXPECT_EQ(partial_label, result[2].first);
    EXPECT_EQ(make_bytes(0xCC, 0x05), bools_to_bytes(result[3].first));
    EXPECT_EQ(make_bytes(0xAA, 0x04), bools_to_bytes(result[4].first));
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, result[5].first);

    EXPECT_EQ(false, trie.lookup(make_bytes(0x11, 0x02), result));
    EXPECT_EQ(4, result.size());
    EXPECT_EQ(make_bytes(0x01, 0x02), bools_to_bytes(result[0].first));
    EXPECT_EQ(make_bytes(0x11, 0x01), bools_to_bytes(result[1].first));
    partial_label = partial_label_type{ 0, 0, 0 };
    EXPECT_EQ(partial_label, result[2].first);
    partial_label = partial_label_type{ 1 };
    EXPECT_EQ(partial_label, result[3].first);
}

TEST(CompressedTrie, SaveLoadTest)
{
    shared_ptr<storage::Storage> storage = make_shared<storage::MemoryStorage>();
    CompressedTrie trie(storage);
    append_proof_type append_proof;

    trie.insert(make_bytes(0b00010000), make_bytes(0x01, 0x02, 0x03, 0x04, 0x05), append_proof);
    trie.insert(make_bytes(0b00001110), make_bytes(0x02, 0x03, 0x04, 0x05, 0x06), append_proof);
    trie.insert(make_bytes(0b00001100), make_bytes(0x03, 0x04, 0x05, 0x06, 0x07), append_proof);
    trie.insert(make_bytes(0b00001000), make_bytes(0x04, 0x05, 0x06, 0x07, 0x08), append_proof);
    trie.insert(make_bytes(0b00000111), make_bytes(0x05, 0x06, 0x07, 0x08, 0x09), append_proof);
    trie.insert(make_bytes(0b00000100), make_bytes(0x06, 0x07, 0x08, 0x09, 0x0A), append_proof);

    string status = trie.to_string(/* include_payload */ true);

    EXPECT_EQ(
        "n::l:000:r:(null);"
        "n:000:l:0000:r:00010000;"
        "n:0000:l:000001:r:00001;"
        "n:000001:l:00000100:r:00000111;"
        "n:00000100:l:(null):r:(null):p:060708090a;"
        "n:00000111:l:(null):r:(null):p:0506070809;"
        "n:00001:l:00001000:r:000011;"
        "n:00001000:l:(null):r:(null):p:0405060708;"
        "n:000011:l:00001100:r:00001110;"
        "n:00001100:l:(null):r:(null):p:0304050607;"
        "n:00001110:l:(null):r:(null):p:0203040506;"
        "n:00010000:l:(null):r:(null):p:0102030405;",
        status);

    stringstream ss;

    size_t save_size = trie.save(ss);

    CompressedTrie trie2(storage);
    size_t load_size = CompressedTrie::Load(trie2, ss);
    EXPECT_EQ(load_size, save_size);

    string status2 = trie2.to_string(/* include_payload */ true);
    EXPECT_EQ(status2, status);
    EXPECT_EQ(trie.epoch(), trie2.epoch());
}

TEST(CompressedTrie, SaveLoadTest2)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie(storage);
    append_proof_type append_proof;

    trie.insert(make_bytes(0b10010000), make_bytes(0x01, 0x02, 0x03, 0x04, 0x05), append_proof);
    trie.insert(make_bytes(0b10001110), make_bytes(0x02, 0x03, 0x04, 0x05, 0x06), append_proof);
    trie.insert(make_bytes(0b10001100), make_bytes(0x03, 0x04, 0x05, 0x06, 0x07), append_proof);
    trie.insert(make_bytes(0b10001000), make_bytes(0x04, 0x05, 0x06, 0x07, 0x08), append_proof);
    trie.insert(make_bytes(0b10000111), make_bytes(0x05, 0x06, 0x07, 0x08, 0x09), append_proof);
    trie.insert(make_bytes(0b10000100), make_bytes(0x06, 0x07, 0x08, 0x09, 0x0A), append_proof);

    string status = trie.to_string(/* include_payload */ true);

    EXPECT_EQ(
        "n::l:(null):r:100;"
        "n:100:l:1000:r:10010000;"
        "n:1000:l:100001:r:10001;"
        "n:100001:l:10000100:r:10000111;"
        "n:10000100:l:(null):r:(null):p:060708090a;"
        "n:10000111:l:(null):r:(null):p:0506070809;"
        "n:10001:l:10001000:r:100011;"
        "n:10001000:l:(null):r:(null):p:0405060708;"
        "n:100011:l:10001100:r:10001110;"
        "n:10001100:l:(null):r:(null):p:0304050607;"
        "n:10001110:l:(null):r:(null):p:0203040506;"
        "n:10010000:l:(null):r:(null):p:0102030405;",
        status);

    stringstream ss;

    size_t save_size = trie.save(ss);

    CompressedTrie trie2(storage);
    size_t load_size = CompressedTrie::Load(trie2, ss);
    EXPECT_EQ(load_size, save_size);

    string status2 = trie2.to_string(/* include_payload */ true);
    EXPECT_EQ(status2, status);
    EXPECT_EQ(trie.epoch(), trie2.epoch());
}

TEST(CompressedTrie, SaveLoadTest3)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie1(storage);

    label_type label(40);
    payload_type payload(40);
    append_proof_type append_proof;
    vector<label_type> labels;

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(label.data()), label.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        trie1.insert(label, payload, append_proof);

        labels.push_back(label);
    }

    stringstream ss;
    trie1.save(ss);

    CompressedTrie trie2(storage);
    CompressedTrie::Load(trie2, ss);

    for (size_t i = 0; i < labels.size(); i++) {
        lookup_path_type path1;
        lookup_path_type path2;

        auto lookup_result1 = trie1.lookup(labels[i], path1);
        auto lookup_result2 = trie2.lookup(labels[i], path2);

        EXPECT_EQ(lookup_result1, lookup_result2);
        EXPECT_EQ(path1.size(), path2.size());
    }
}

TEST(CompressedTrie, SaveLoadToVectorTest)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie1(storage);

    label_type label(40);
    payload_type payload(40);
    append_proof_type append_proof;
    vector<label_type> labels;

    for (size_t i = 0; i < 10000; i++) {
        get_random_bytes(reinterpret_cast<unsigned char *>(label.data()), label.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());

        trie1.insert(label, payload, append_proof);

        labels.push_back(label);
    }

    vector<byte> vec;
    trie1.save(vec);

    CompressedTrie trie2(storage);
    CompressedTrie::Load(trie2, vec);

    for (size_t i = 0; i < labels.size(); i++) {
        lookup_path_type path1;
        lookup_path_type path2;

        auto lookup_result1 = trie1.lookup(labels[i], path1);
        auto lookup_result2 = trie2.lookup(labels[i], path2);

        EXPECT_EQ(lookup_result1, lookup_result2);
        EXPECT_EQ(path1.size(), path2.size());
    }
}

TEST(CompressedTrie, EmptyTreesTest)
{
    shared_ptr<ozks::storage::Storage> storage = make_shared<ozks::storage::MemoryStorage>();
    CompressedTrie trie1(storage);
    CompressedTrie trie2(storage);

    commitment_type comm1;
    commitment_type comm2;
    trie1.get_commitment(comm1);
    trie2.get_commitment(comm2);

    EXPECT_EQ(comm1, comm2);
}
