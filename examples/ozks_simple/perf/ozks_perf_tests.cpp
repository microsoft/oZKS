// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

// OZKS
#include "../ozks.h"
#include "oZKS/storage/memory_storage.h"
#include "oZKS/utilities.h"
#include "mem_utils.h"

// Google Benchmark
#include <benchmark/benchmark.h>

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks_simple;

namespace {
    shared_ptr<OZKS> ozks_;
    vector<key_type> keys_;
    size_t keys_max_size_ = 65'536;
    size_t thread_count_;

    string save_file_;

    /**
    Get random bytes and throw if unsuccessful
    */
    void get_random_bytes(byte *dest, size_t size)
    {
        if (!utils::random_bytes(dest, size))
            throw runtime_error("Failed to get random bytes");
    }
} // namespace

static double DoInsert(benchmark::State &state, OZKS &ozks, bool do_flush)
{
    key_type key(32);
    payload_type payload(32);
    keys_.clear();

    // Measure total duration
    auto start = chrono::high_resolution_clock::now();

    // Insertion
    for (auto _ : state) {
        state.PauseTiming();
        get_random_bytes(key.data(), key.size());
        get_random_bytes(payload.data(), payload.size());
        state.ResumeTiming();

        auto insert_result = ozks.insert(key, payload);
        if (do_flush) {
            ozks.flush();
        }

        state.PauseTiming();
        // Max 1 million keys
        if (keys_.size() < keys_max_size_) {
            keys_.push_back(key);
        }
        state.ResumeTiming();
    }

    // Report how long it took to perform all repetitions
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;
    auto total_ms = chrono::duration_cast<chrono::milliseconds>(diff);
    double total_ms_double = static_cast<double>(total_ms.count());

    return total_ms_double;
}

static void DoInsertReport(benchmark::State &state, OZKS &ozks)
{
    double total_ms = DoInsert(state, ozks, /* do_flush */ true);

    auto mem_use = get_current_mem();
    state.counters["Elapsed"] = benchmark::Counter(total_ms);
    state.counters["WorkingSet"] = benchmark::Counter(
        static_cast<double>(mem_use.first),
        benchmark::Counter::kDefaults,
        benchmark::Counter::OneK::kIs1024);
    state.counters["PrivateSet"] = benchmark::Counter(
        static_cast<double>(mem_use.second),
        benchmark::Counter::kDefaults,
        benchmark::Counter::OneK::kIs1024);
    state.counters["Thread count"] = benchmark::Counter(
        static_cast<double>(utils::get_insertion_thread_limit(nullptr, thread_count_)));
    state.counters["VRF cache size"] =
        benchmark::Counter(static_cast<double>(ozks.get_vrf_cache().max_size()));
}

static void DoInsertFlushReport(benchmark::State &state, OZKS &ozks)
{
    double total_ms = DoInsert(state, ozks, /* do_flush */ false);

    auto flush_start = chrono::high_resolution_clock::now();
    ozks.flush();
    auto flush_end = chrono::high_resolution_clock::now();

    // Report how long it took to perform all repetitions
    chrono::duration<double> flush_diff = flush_end - flush_start;
    auto flush_total_ms = chrono::duration_cast<chrono::milliseconds>(flush_diff);

    auto mem_use = get_current_mem();
    state.counters["Elapsed"] = benchmark::Counter(total_ms);
    state.counters["FlushElapsed"] =
        benchmark::Counter(static_cast<double>(flush_total_ms.count()));
    state.counters["WorkingSet"] = benchmark::Counter(
        static_cast<double>(mem_use.first),
        benchmark::Counter::kDefaults,
        benchmark::Counter::OneK::kIs1024);
    state.counters["PrivateSet"] = benchmark::Counter(
        static_cast<double>(mem_use.second),
        benchmark::Counter::kDefaults,
        benchmark::Counter::OneK::kIs1024);
    state.counters["Thread count"] = benchmark::Counter(
        static_cast<double>(utils::get_insertion_thread_limit(nullptr, thread_count_)));
    state.counters["VRF cache size"] =
        benchmark::Counter(static_cast<double>(ozks.get_vrf_cache().max_size()));
}

static double DoBatchInsertFlush(
    benchmark::State &state, OZKS &ozks, size_t batch_size, size_t prime_size)
{
    key_type key(32);
    payload_type payload(32);

    // Measure total duration
    chrono::duration<double> total{};

    auto generate_data = [batch_size, &key, &payload](size_t count) -> key_payload_batch_type {
        key_payload_batch_type batch;
        batch.reserve(count);
        for (size_t i = 0; i < batch_size; ++i) {
            get_random_bytes(key.data(), key.size());
            get_random_bytes(payload.data(), payload.size());
            batch.emplace_back(key, payload);
        }

        // Max 1 million keys
        if (keys_.size() < keys_max_size_) {
            keys_.push_back(key);
        }

        return batch;
    };

    // Prime the oZKS with prime_size many keys to ensure iterations are
    // stable in multi-threaded experiments.
    if (prime_size) {
        auto prime_batch = generate_data(prime_size);
        auto insert_result = ozks.insert(prime_batch);
        ozks.flush();
    }

    // Insertion
    for (auto _ : state) {
        state.PauseTiming();
        key_payload_batch_type batch = generate_data(batch_size);
        state.ResumeTiming();

        auto start = chrono::high_resolution_clock::now();
        auto insert_result = ozks.insert(batch);
        ozks.flush();
        auto end = chrono::high_resolution_clock::now();
        total += end - start;
    }

    // Report how long it took to perform all repetitions
    auto total_ms = chrono::duration_cast<chrono::milliseconds>(total);
    double total_ms_double = static_cast<double>(total_ms.count());

    return total_ms_double;
}

static void DoBatchInsertFlushReport(
    benchmark::State &state, OZKS &ozks, size_t batch_size, size_t prime_size)
{
    double total_ms = DoBatchInsertFlush(state, ozks, batch_size, prime_size);

    auto mem_use = get_current_mem();
    state.counters["InsertElapsed"] = benchmark::Counter(total_ms);
    state.counters["Batch size"] = benchmark::Counter(static_cast<double>(batch_size));
    state.counters["WorkingSet"] = benchmark::Counter(
        static_cast<double>(mem_use.first),
        benchmark::Counter::kDefaults,
        benchmark::Counter::OneK::kIs1024);
    state.counters["PrivateSet"] = benchmark::Counter(
        static_cast<double>(mem_use.second),
        benchmark::Counter::kDefaults,
        benchmark::Counter::OneK::kIs1024);
    state.counters["Thread count"] = benchmark::Counter(
        static_cast<double>(utils::get_insertion_thread_limit(nullptr, thread_count_)));
    state.counters["VRF cache size"] =
        benchmark::Counter(static_cast<double>(ozks.get_vrf_cache().max_size()));
}

static void OZKSInsertStored(benchmark::State &state)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::UncommitedPayload,
                          LabelType::HashedLabels,
                          TrieType::Stored,
                          storage,
                          vrf_seed,
                          /* vrf_cache_size */ 0,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertReport(state, *ozks_);
}

static void OZKSInsertFlushStored(benchmark::State &state)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::UncommitedPayload,
                          LabelType::HashedLabels,
                          TrieType::Stored,
                          storage,
                          vrf_seed,
                          /* vrf_cache_size */ 0,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertFlushReport(state, *ozks_);
}

static void OZKSInsertStoredVRF(benchmark::State &state, size_t vrf_cache_size)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::CommitedPayload,
                          LabelType::VRFLabels,
                          TrieType::Stored,
                          storage,
                          vrf_seed,
                          vrf_cache_size,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertReport(state, *ozks_);
}

static void OZKSInsertFlushStoredVRF(benchmark::State &state, size_t vrf_cache_size)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::CommitedPayload,
                          LabelType::VRFLabels,
                          TrieType::Stored,
                          storage,
                          vrf_seed,
                          vrf_cache_size,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertFlushReport(state, *ozks_);
}

static void OZKSBatchInsertFlushStoredVRF(
    benchmark::State &state, size_t vrf_cache_size, size_t batch_size)
{
    keys_.clear();

    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::CommitedPayload,
                          LabelType::VRFLabels,
                          TrieType::Stored,
                          storage,
                          vrf_seed,
                          vrf_cache_size,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoBatchInsertFlushReport(state, *ozks_, batch_size, 0 /* Priming the oZKS is not needed */);
}

static void OZKSInsertLinked(benchmark::State &state)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::UncommitedPayload,
                          LabelType::HashedLabels,
                          TrieType::Linked,
                          storage,
                          vrf_seed,
                          /* vrf_cache_size */ 0,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertReport(state, *ozks_);
}

static void OZKSInsertFlushLinked(benchmark::State &state)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::UncommitedPayload,
                          LabelType::HashedLabels,
                          TrieType::Linked,
                          storage,
                          vrf_seed,
                          /* vrf_cache_size */ 0,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertFlushReport(state, *ozks_);
}

static void OZKSInsertLinkedVRF(benchmark::State &state, size_t vrf_cache_size)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::CommitedPayload,
                          LabelType::VRFLabels,
                          TrieType::Linked,
                          storage,
                          vrf_seed,
                          vrf_cache_size,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertReport(state, *ozks_);
}

static void OZKSInsertFlushLinkedVRF(benchmark::State &state, size_t vrf_cache_size)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::CommitedPayload,
                          LabelType::VRFLabels,
                          TrieType::Linked,
                          storage,
                          vrf_seed,
                          vrf_cache_size,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertFlushReport(state, *ozks_);
}

static void OZKSInsertFlushLinkedNoStorage(benchmark::State &state)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::UncommitedPayload,
                          LabelType::HashedLabels,
                          TrieType::LinkedNoStorage,
                          storage,
                          vrf_seed,
                          /* vrf_cache_size */ 0,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertFlushReport(state, *ozks_);
}

static void OZKSInsertFlushLinkedVRFNoStorage(benchmark::State &state, size_t vrf_cache_size)
{
    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::CommitedPayload,
                          LabelType::VRFLabels,
                          TrieType::LinkedNoStorage,
                          storage,
                          vrf_seed,
                          vrf_cache_size,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoInsertFlushReport(state, *ozks_);
}

static void OZKSBatchInsertFlushLinkedVRFNoStorage(
    benchmark::State &state, size_t batch_size, size_t prime_size)
{
    keys_.clear();

    auto storage = make_shared<MemoryStorage>();
    vector<byte> vrf_seed;
    OZKSConfig config = { PayloadCommitmentType::CommitedPayload,
                          LabelType::VRFLabels,
                          TrieType::LinkedNoStorage,
                          storage,
                          vrf_seed,
                          /* vrf_cache_size */ 0,
                          thread_count_ };
    ozks_ = make_shared<OZKS>(config);

    DoBatchInsertFlushReport(state, *ozks_, batch_size, prime_size);
}

static void OZKSInsertFlushAdditional(benchmark::State &state)
{
    // Add additional elements to an existing oZKS instance.
    // This is done to test parallel insertion. The first one will prepare the tree,
    // this one will use the existing tree to perform parallel insertion.
    DoInsertFlushReport(state, *ozks_);
}

static void OZKSBatchInsertFlushAdditional(benchmark::State &state, size_t batch_size)
{
    // Add additional elements to an existing oZKS instance.
    // This is done to test parallel insertion. The first one will prepare the tree,
    // this one will use the existing tree to perform parallel insertion.
    DoBatchInsertFlushReport(state, *ozks_, batch_size, 0);
}

enum class vrf_cache_behavior { none, ensure_hit, ensure_miss };

static void OZKSLookup(benchmark::State &state, vrf_cache_behavior behavior)
{
    size_t accumulated_depth = 0;
    size_t lookup_count = 0;
    size_t idx = 0;

    // Ensure that the cache contents are cleared
    ozks_->get_vrf_cache().clear();

    for (auto _ : state) {
        state.PauseTiming();
        if (behavior == vrf_cache_behavior::ensure_hit) {
            ozks_->query(keys_[idx]);
        }
        state.ResumeTiming();

        auto query_result = ozks_->query(keys_[idx]);

        state.PauseTiming();
        lookup_count++;
        accumulated_depth += query_result.lookup_proof().size() - 1;

        if (!query_result.is_member()) {
            state.SkipWithError("query should have been found");
        }

        idx++;
        if (idx >= keys_.size()) {
            idx = 0;

            if (behavior == vrf_cache_behavior::ensure_miss) {
                // Ensure that the cache contents are cleared
                ozks_->get_vrf_cache().clear_contents();
            }
        }
        state.ResumeTiming();
    }

    double avg_depth = static_cast<double>(accumulated_depth) / static_cast<double>(lookup_count);
    state.counters["Avg depth"] = benchmark::Counter(avg_depth);
    state.counters["VRF cache size"] =
        benchmark::Counter(static_cast<double>(ozks_->get_vrf_cache().max_size()));
    state.counters["VRF cache hits"] =
        benchmark::Counter(static_cast<double>(ozks_->get_vrf_cache().cache_hits()));
    state.counters["VRF cache misses"] =
        benchmark::Counter(static_cast<double>(ozks_->get_vrf_cache().cache_misses()));
}

static void OZKSFailedLookup(benchmark::State &state, vrf_cache_behavior behavior)
{
    size_t accumulated_depth = 0;
    size_t lookup_count = 0;
    key_type key(40);

    // Ensure that the cache contents are cleared
    ozks_->get_vrf_cache().clear();

    for (auto _ : state) {
        state.PauseTiming();
        get_random_bytes(key.data(), key.size());
        if (behavior == vrf_cache_behavior::ensure_hit) {
            ozks_->query(key);
        }
        state.ResumeTiming();

        auto query_result = ozks_->query(key);

        state.PauseTiming();
        lookup_count++;
        if (query_result.lookup_proof().size() > 2) {
            accumulated_depth += query_result.lookup_proof().size() - 2;
        }

        if (query_result.is_member()) {
            state.SkipWithError("query should not have been found");
        }
        state.ResumeTiming();
    }

    double avg_depth = static_cast<double>(accumulated_depth) / static_cast<double>(lookup_count);
    state.counters["Avg depth"] = benchmark::Counter(avg_depth);
    state.counters["VRF cache size"] =
        benchmark::Counter(static_cast<double>(ozks_->get_vrf_cache().max_size()));
    state.counters["VRF cache hits"] =
        benchmark::Counter(static_cast<double>(ozks_->get_vrf_cache().cache_hits()));
    state.counters["VRF cache misses"] =
        benchmark::Counter(static_cast<double>(ozks_->get_vrf_cache().cache_misses()));
}

static void OZKSVerifySuccessfulQuery(benchmark::State &state)
{
    size_t idx = 0;
    for (auto _ : state) {
        if (idx >= keys_.size()) {
            idx = 0;
        }

        state.PauseTiming();
        auto query_result = ozks_->query(keys_[idx]);
        state.ResumeTiming();

        if (!query_result.is_member()) {
            state.SkipWithError("query should have been found");
        }

        bool verification_result = query_result.verify(ozks_->get_commitment());
        idx++;

        if (!verification_result) {
            state.SkipWithError("Verification should have succeeded");
        }
    }
}

static void OZKSVerifyFailedQuery(benchmark::State &state)
{
    key_type key(32);

    for (auto _ : state) {
        state.PauseTiming();
        get_random_bytes(key.data(), key.size());
        auto query_result = ozks_->query(key);
        state.ResumeTiming();

        if (query_result.is_member()) {
            state.SkipWithError("query should not have been found");
        }

        bool verification_result = query_result.verify(ozks_->get_commitment());
        if (!verification_result) {
            state.SkipWithError("Verification should have succeeded");
        }
    }
}

static void OZKSSave(benchmark::State &state)
{
    for (auto _ : state) {
        ofstream output_file;
        output_file.open(save_file_, ios::out | ios::binary | ios::trunc);
        ozks_->save(output_file);
        output_file.close();
    }
}

static void OZKSLoad(benchmark::State &state)
{
    ozks_->clear();

    auto storage = make_shared<MemoryStorage>();

    for (auto _ : state) {
        ifstream input_file;
        input_file.open(save_file_, ios::in | ios::binary);
        auto loaded = OZKS::Load(storage, input_file);
        input_file.close();
    }
}

static void VRFGetProof(benchmark::State &state)
{
    VRFSecretKey sk;
    sk.initialize();
    VRFPublicKey pk = sk.get_vrf_public_key();
    vector<byte> data(32);

    for (auto _ : state) {
        state.PauseTiming();
        utils::random_bytes(data.data(), data.size());
        state.ResumeTiming();

        auto vrf_proof = sk.get_vrf_proof(data);

        state.PauseTiming();
        auto verification_result = pk.verify_vrf_proof(data, vrf_proof);
        if (!verification_result) {
            state.SkipWithError("Verification should have succeeded");
        }
        state.ResumeTiming();
    }
}

static void VRFGetValue(benchmark::State &state)
{
    VRFSecretKey sk;
    sk.initialize();
    vector<byte> data(32);

    for (auto _ : state) {
        state.PauseTiming();
        utils::random_bytes(data.data(), data.size());
        state.ResumeTiming();

        [[maybe_unused]] auto vrf_value = sk.get_vrf_value(data);
    }
}

static void VRFVerifyProof(benchmark::State &state)
{
    VRFSecretKey sk;
    sk.initialize();
    VRFPublicKey pk = sk.get_vrf_public_key();
    vector<byte> data(32);

    for (auto _ : state) {
        state.PauseTiming();
        utils::random_bytes(data.data(), data.size());
        auto proof = sk.get_vrf_proof(data);
        state.ResumeTiming();

        auto verification_result = pk.verify_vrf_proof(data, proof);
        if (!verification_result) {
            state.SkipWithError("Verification should have succeeded");
        }
    }
}

int main(int argc, char **argv)
{
    size_t vrf_cache_size = 65536;
    size_t insert_batch_size = 1024;
    size_t ozks_prime_size = 1024;
    long iterations = 100'000;

    save_file_ = "save_load_bench.tmp";
    thread_count_ = 0;
    if (filesystem::exists(save_file_)) {
        stringstream ss;
        ss << "Save file " << save_file_ << " exists";
        cout << ss.str() << endl;
        throw runtime_error(ss.str());
    }

    using namespace std::placeholders;

    benchmark::RegisterBenchmark("VRFGetValue", VRFGetValue);
    benchmark::RegisterBenchmark("VRFGetProof", VRFGetProof);
    benchmark::RegisterBenchmark("VRFVerifyProof", VRFVerifyProof);

    // 2^20
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushStoredVRF",
        bind(OZKSBatchInsertFlushStoredVRF, _1, vrf_cache_size, insert_batch_size))
        ->Iterations((1 << 20) / insert_batch_size);
    benchmark::RegisterBenchmark(
        "OZKSLookupStoredVRF (ensure cache hit)",
        bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));
    benchmark::RegisterBenchmark(
        "OZKSFailedLookupStoredVRF (ensure cache hit)",
        bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_hit));
    benchmark::RegisterBenchmark(
        "OZKSLookupStoredVRF (ensure cache miss)",
        bind(OZKSLookup, _1, vrf_cache_behavior::ensure_miss));
    benchmark::RegisterBenchmark(
        "OZKSFailedLookupStoredVRF (ensure cache miss)",
        bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_miss));

    // 2^22
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushStoredVRF",
        bind(OZKSBatchInsertFlushStoredVRF, _1, vrf_cache_size, insert_batch_size))
        ->Iterations((1 << 22) / insert_batch_size);
    benchmark::RegisterBenchmark(
        "OZKSLookupStoredVRF (ensure cache hit)",
        bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));
    benchmark::RegisterBenchmark(
        "OZKSFailedLookupStoredVRF (ensure cache hit)",
        bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_hit));
    benchmark::RegisterBenchmark(
        "OZKSLookupStoredVRF (ensure cache miss)",
        bind(OZKSLookup, _1, vrf_cache_behavior::ensure_miss));
    benchmark::RegisterBenchmark(
        "OZKSFailedLookupStoredVRF (ensure cache miss)",
        bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_miss));

    // 2^24
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushStoredVRF",
        bind(OZKSBatchInsertFlushStoredVRF, _1, vrf_cache_size, insert_batch_size))
        ->Iterations((1 << 24) / insert_batch_size);
    benchmark::RegisterBenchmark(
        "OZKSLookupStoredVRF (ensure cache hit)",
        bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));
    benchmark::RegisterBenchmark(
        "OZKSFailedLookupStoredVRF (ensure cache hit)",
        bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_hit));
    benchmark::RegisterBenchmark(
        "OZKSLookupStoredVRF (ensure cache miss)",
        bind(OZKSLookup, _1, vrf_cache_behavior::ensure_miss));
    benchmark::RegisterBenchmark(
        "OZKSFailedLookupStoredVRF (ensure cache miss)",
        bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_miss));

    // // 2^26
    // benchmark::RegisterBenchmark(
    //     "OZKSBatchInsertFlushStoredVRF",
    //     bind(OZKSBatchInsertFlushStoredVRF, _1, vrf_cache_size, insert_batch_size))
    //     ->Iterations((1 << 26) / insert_batch_size);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupStoredVRF (ensure cache hit)",
    //     bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));
    // benchmark::RegisterBenchmark(
    //     "OZKSFailedLookupStoredVRF (ensure cache hit)",
    //     bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_hit));
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupStoredVRF (ensure cache miss)",
    //     bind(OZKSLookup, _1, vrf_cache_behavior::ensure_miss));
    // benchmark::RegisterBenchmark(
    //     "OZKSFailedLookupStoredVRF (ensure cache miss)",
    //     bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_miss));

    // 2^20
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushLinkedVRFNoStorage",
        bind(OZKSBatchInsertFlushLinkedVRFNoStorage, _1, insert_batch_size, ozks_prime_size))
        ->Iterations((1 << 20) / insert_batch_size);
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushLinkedVRFNoStorageAdditional",
        bind(OZKSBatchInsertFlushAdditional, _1, insert_batch_size));

    // 2^22
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushLinkedVRFNoStorage",
        bind(OZKSBatchInsertFlushLinkedVRFNoStorage, _1, insert_batch_size, ozks_prime_size))
        ->Iterations((1 << 22) / insert_batch_size);
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushLinkedVRFNoStorageAdditional",
        bind(OZKSBatchInsertFlushAdditional, _1, insert_batch_size));

    // 2^24
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushLinkedVRFNoStorage",
        bind(OZKSBatchInsertFlushLinkedVRFNoStorage, _1, insert_batch_size, ozks_prime_size))
        ->Iterations((1 << 24) / insert_batch_size);
    benchmark::RegisterBenchmark(
        "OZKSBatchInsertFlushLinkedVRFNoStorageAdditional",
        bind(OZKSBatchInsertFlushAdditional, _1, insert_batch_size));

    // // 2^26
    // benchmark::RegisterBenchmark(
    //     "OZKSBatchInsertFlushLinkedVRFNoStorage",
    //     bind(OZKSBatchInsertFlushLinkedVRFNoStorage, _1, insert_batch_size, ozks_prime_size))
    //     ->Iterations((1 << 26) / insert_batch_size);
    // benchmark::RegisterBenchmark(
    //     "OZKSBatchInsertFlushLinkedVRFNoStorageAdditional",
    //     bind(OZKSBatchInsertFlushAdditional, _1, insert_batch_size));

    // // Stored node tests
    // // benchmark::RegisterBenchmark("OZKSInsertStored",
    // OZKSInsertStored)->Iterations(iterations);
    // // benchmark::RegisterBenchmark("OZKSInsertFlushStored", OZKSInsertFlushStored)
    // //     ->Iterations(iterations);
    // // benchmark::RegisterBenchmark("OZKSLookupStored", OZKSLookup);
    // // benchmark::RegisterBenchmark("OZKSInsertStoredVRF", bind(OZKSInsertStoredVRF, _1, 0))
    // //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushStoredVRF", bind(OZKSInsertFlushStoredVRF, _1, vrf_cache_size))
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupStoredVRF", bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));
    // // benchmark::RegisterBenchmark("OZKSInsertLinked",
    // OZKSInsertLinked)->Iterations(iterations);
    // // benchmark::RegisterBenchmark("OZKSInsertFlushLinked", OZKSInsertFlushLinked)
    // //     ->Iterations(iterations);
    // // benchmark::RegisterBenchmark("OZKSLookupLinked", bind(OZKSLookup, _1,
    // // vrf_cache_behavior::none));
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedVRF", bind(OZKSInsertFlushLinkedVRF, _1, vrf_cache_size))
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupLinkedVRF", bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));

    // // Linked node tests with no storage
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedVRFNoStorage", bind(OZKSInsertFlushLinkedVRFNoStorage, _1,
    //     vrf_cache_size))
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupLinkedVRFNoStorage", bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedVRFNoStorageAdditional1", OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedVRFNoStorageAdditional2", OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedVRFNoStorageAdditional3", OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupLinkedVRFNoStorageAdditional",
    //     bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));
    // benchmark::RegisterBenchmark("OZKSInsertFlushLinkedNoStorage",
    // OZKSInsertFlushLinkedNoStorage)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupLinkedNoStorage", bind(OZKSLookup, _1, vrf_cache_behavior::none));
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedNoStorageAdditional1", OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedNoStorageAdditional2", OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedNoStorageAdditional3", OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupLinkedNoStorageAdditional", bind(OZKSLookup, _1, vrf_cache_behavior::none));

    // // Linked node tests with storage
    // benchmark::RegisterBenchmark(
    //     "OZKSInsertFlushLinkedVRF", bind(OZKSInsertFlushLinkedVRF, _1, 1'000'000))
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupLinkedVRF", bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));
    // benchmark::RegisterBenchmark("OZKSInsertFlushLinkedVRFAdditional1",
    // OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark("OZKSInsertFlushLinkedVRFAdditional2",
    // OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark("OZKSInsertFlushLinkedVRFAdditional3",
    // OZKSInsertFlushAdditional)
    //     ->Iterations(iterations);
    // benchmark::RegisterBenchmark(
    //     "OZKSLookupLinkedVRFAdditional", bind(OZKSLookup, _1, vrf_cache_behavior::ensure_hit));

    // benchmark::RegisterBenchmark(
    //     "OZKSFailedLookup", bind(OZKSFailedLookup, _1, vrf_cache_behavior::ensure_hit));
    // benchmark::RegisterBenchmark("OZKSVerifySuccess", OZKSVerifySuccessfulQuery);
    // benchmark::RegisterBenchmark("OZKSVerifyFailure", OZKSVerifyFailedQuery);

    // benchmark::RegisterBenchmark("OZKSSave", OZKSSave)->Unit(benchmark::kMillisecond);
    // benchmark::RegisterBenchmark("OZKSLoad", OZKSLoad)->Unit(benchmark::kMillisecond);

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();

    // Delete the temporary file
    filesystem::remove(save_file_);

    return 0;
}
