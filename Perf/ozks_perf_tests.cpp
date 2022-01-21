// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <chrono>
#include <fstream>
#include <string>

// OZKS
#include "oZKS/fourq/random.h"
#include "oZKS/ozks.h"
#include "mem_utils.h"

// Google Benchmark
#include <benchmark/benchmark.h>

using namespace std;
using namespace ozks;

namespace {
    OZKSConfig ozks_config_ = { false, false };
    OZKS ozks_(ozks_config_); // The OZKS instance
    vector<key_type> keys_;

    string file_name_;

    /**
    Get random bytes and throw if unsuccessful
    */
    void get_random_bytes(unsigned char *dest, size_t size)
    {
        if (!random_bytes(dest, static_cast<unsigned int>(size)))
            throw runtime_error("Failed to get random bytes");
    }
} // namespace

static void OZKSInsert(benchmark::State &state)
{
    key_type key(40);
    payload_type payload(40);
    keys_.clear();

    // Measure total duration
    auto start = chrono::high_resolution_clock::now();

    // Insertion
    for (auto _ : state) {
        state.PauseTiming();
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());
        state.ResumeTiming();

        auto insert_result = ozks_.insert(key, payload);

        state.PauseTiming();
        // Max 1 million keys
        if (keys_.size() < 1'000'000) {
            keys_.push_back(key);
        }
        state.ResumeTiming();
    }

    // We don't want to include flush in this scenario
    state.PauseTiming();
    ozks_.flush();
    state.ResumeTiming();

    // Report how long it took to perform all repetitions
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;
    auto total_ms = chrono::duration_cast<chrono::milliseconds>(diff);

    auto mem_use = get_current_mem();
    state.counters["WorkingSet"] = benchmark::Counter(
        (double)mem_use.first, benchmark::Counter::kDefaults, benchmark::Counter::OneK::kIs1024);
    state.counters["PrivateSet"] = benchmark::Counter(
        (double)mem_use.second, benchmark::Counter::kDefaults, benchmark::Counter::OneK::kIs1024);
    state.counters["Elapsed"] = benchmark::Counter((double)total_ms.count());
}

static void OZKSInsertFlush(benchmark::State& state)
{
    key_type key(40);
    payload_type payload(40);
    OZKS ozks(ozks_config_);
    
    // Measure total duration
    auto start = chrono::high_resolution_clock::now();

    // Insertion
    for (auto _ : state) {
        state.PauseTiming();
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        get_random_bytes(reinterpret_cast<unsigned char *>(payload.data()), payload.size());
        state.ResumeTiming();
        
        ozks.insert(key, payload);
    }

    auto flush_start = chrono::high_resolution_clock::now();
    ozks.flush();
    auto flush_end = chrono::high_resolution_clock::now();

    // Report how long it took to perform all repetitions
    auto end = chrono::high_resolution_clock::now();
    chrono::duration<double> diff = end - start;
    auto total_ms = chrono::duration_cast<chrono::milliseconds>(diff);
    chrono::duration<double> flush_diff = flush_end - flush_start;
    auto flush_total_ms = chrono::duration_cast<chrono::milliseconds>(flush_diff);

    state.counters["Elapsed"] = benchmark::Counter((double)total_ms.count());
    state.counters["FlushElapsed"] = benchmark::Counter((double)flush_total_ms.count());
}

static void OZKSLookup(benchmark::State &state)
{
    size_t accumulated_depth = 0;
    size_t lookup_count = 0;
    size_t idx = 0;

    for (auto _ : state) {
        if (idx >= keys_.size()) {
            idx = 0;
        }

        auto query_result = ozks_.query(keys_[idx++]);
        lookup_count++;
        accumulated_depth += query_result.lookup_proof().size() - 1;

        if (!query_result.is_member()) {
            state.SkipWithError("query should have been found");
        }
    }

    double avg_depth = (double)accumulated_depth / lookup_count;
    state.counters["Avg depth"] = benchmark::Counter(avg_depth);
}

static void OZKSFailedLookup(benchmark::State &state)
{
    size_t accumulated_depth = 0;
    size_t lookup_count = 0;
    size_t idx = 0;
    key_type key(40);

    for (auto _ : state) {
        state.PauseTiming();
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        state.ResumeTiming();

        auto query_result = ozks_.query(key);
        lookup_count++;
        if (query_result.lookup_proof().size() > 2) {
            accumulated_depth += query_result.lookup_proof().size() - 2;
        }

        if (query_result.is_member()) {
            state.SkipWithError("query should not have been found");
        }
    }

    double avg_depth = (double)accumulated_depth / lookup_count;
    state.counters["Avg depth"] = benchmark::Counter(avg_depth);
}

static void OZKSVerifySuccessfulQuery(benchmark::State &state)
{
    size_t idx = 0;
    for (auto _ : state) {
        if (idx >= keys_.size()) {
            idx = 0;
        }

        state.PauseTiming();
        auto query_result = ozks_.query(keys_[idx]);
        state.ResumeTiming();

        if (!query_result.is_member()) {
            state.SkipWithError("query should have been found");
        }

        bool verification_result = query_result.verify(keys_[idx++], ozks_.get_commitment());

        if (!verification_result) {
            state.SkipWithError("Verification should have succeeded");
        }
    }
}

static void OZKSVerifyFailedQuery(benchmark::State &state)
{
    size_t idx = 0;
    key_type key(40);

    for (auto _ : state) {
        state.PauseTiming();
        get_random_bytes(reinterpret_cast<unsigned char *>(key.data()), key.size());
        auto query_result = ozks_.query(key);
        state.ResumeTiming();

        if (query_result.is_member()) {
            state.SkipWithError("query should not have been found");
        }

        bool verification_result = query_result.verify(key, ozks_.get_commitment());

        if (!verification_result) {
            state.SkipWithError("Verification should have succeeded");
        }
    }
}

static void OZKSSave(benchmark::State &state)
{
    for (auto _ : state) {
        ofstream output_file;
        output_file.open(file_name_, ios::out | ios::binary | ios::trunc);
        ozks_.save(output_file);
        output_file.close();
    }
}

static void OZKSLoad(benchmark::State &state)
{
    ozks_.clear();

    for (auto _ : state) {
        OZKS ozks;
        ifstream input_file;
        input_file.open(file_name_, ios::in | ios::binary);
        OZKS::load(ozks, input_file);
        input_file.close();
    }
}

static void OZKSBytesToBools(benchmark::State &state)
{
    vector<byte> bytes(40);
    for (auto _ : state) {
        state.PauseTiming();
        get_random_bytes(reinterpret_cast<unsigned char *>(bytes.data()), bytes.size());
        state.ResumeTiming();

        auto bools = utils::bytes_to_bools(bytes.data(), bytes.size());
    }
}

int main(int argc, char **argv)
{
    // The only valid parameter is the number of iterations
    size_t iterations = 1000;
    if (argc > 1) {
        iterations = static_cast<size_t>(stoi(argv[1]));
    }

    // If provided, next argument is file to use to perform save/load tests
    string save_file;
    if (argc > 2) {
        file_name_ = string(argv[2]);
    }

    benchmark::RegisterBenchmark("OZKSInsert", OZKSInsert)->Iterations(iterations);
    benchmark::RegisterBenchmark("OZKSInsertFlush", OZKSInsertFlush)->Iterations(iterations);
    benchmark::RegisterBenchmark("OZKSLookup", OZKSLookup);
    benchmark::RegisterBenchmark("OZKSFailedLookup", OZKSFailedLookup);
    benchmark::RegisterBenchmark("OZKSVerifySuccess", OZKSVerifySuccessfulQuery);
    benchmark::RegisterBenchmark("OZKSVerifyFailure", OZKSVerifyFailedQuery);
    benchmark::RegisterBenchmark("OZKSBytesToBools", OZKSBytesToBools);

    if (!file_name_.empty()) {
        benchmark::RegisterBenchmark("OZKSSave", OZKSSave)->Unit(benchmark::kMillisecond);
        benchmark::RegisterBenchmark("OZKSLoad", OZKSLoad)->Unit(benchmark::kMillisecond);
    }

    benchmark::Initialize(&argc, argv);
    benchmark::RunSpecifiedBenchmarks();

    return 0;
}
