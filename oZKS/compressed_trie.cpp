// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

// OZKS
#include "oZKS/compressed_trie.h"
#include "oZKS/compressed_trie_generated.h"
#include "oZKS/ct_node_linked.h"
#include "oZKS/ct_node_stored.h"
#include "oZKS/storage/storage.h"
#include "oZKS/thread_pool.h"
#include "oZKS/utilities.h"
#include "oZKS/version.h"

using namespace std;
using namespace ozks;
using namespace ozks::storage;
using namespace ozks::utils;

CompressedTrie::CompressedTrie(
    shared_ptr<Storage> storage, TrieType trie_type, size_t thread_count)
    : epoch_(0), storage_(storage), thread_count_(thread_count), trie_type_(trie_type)
{
    init_random_id();
    init_empty_root();
}

CompressedTrie::CompressedTrie(
    trie_id_type trie_id, shared_ptr<Storage> storage, TrieType trie_type, size_t thread_count)
    : epoch_(0), id_(trie_id), storage_(storage), thread_count_(thread_count), trie_type_(trie_type)
{
    init_empty_root();
}

CompressedTrie::CompressedTrie()
    : epoch_(0), storage_(nullptr), thread_count_(0), trie_type_(TrieType::Stored)
{
    init_random_id();
}

void CompressedTrie::insert(
    const PartialLabel &label, const hash_type &payload_commit, append_proof_type &append_proof)
{
    append_proof.clear();

    epoch_++;

    root_->insert(label, payload_commit, epoch_);
    root_->update_hashes(label);

    // To get the append proof we need to lookup the item we just inserted after hashes have been
    // updated
    if (!lookup(label, append_proof, /* include_searched */ true)) {
        throw runtime_error("Should have been able to find the item we just inserted");
    }

    save_to_storage();
}

void CompressedTrie::insert(
    const partial_label_hash_batch_type &label_commit_batch, append_proof_batch_type &append_proofs)
{
    size_t thread_count = 1;
    if (root_->parallelizable()) {
        thread_count = utils::get_insertion_thread_limit(root_, thread_count_);
    }

    size_t bit_count = utils::get_log2(thread_count);
    ThreadPool tp(thread_count);
    vector<vector<pair<const PartialLabel &, const hash_type &>>> batches(thread_count);

    append_proofs.resize(label_commit_batch.size());
    epoch_++;

    // Reorder label_commit_batch entries according to utils::get_insertion_index
    // for multi-threaded processing.
    for (size_t li = 0; li < label_commit_batch.size(); li++) {
        const auto &label = label_commit_batch[li].first;
        const auto &payload = label_commit_batch[li].second;

        size_t ins_idx = utils::get_insertion_index(bit_count, label);
        batches[ins_idx].push_back({ label, payload });
    }

    vector<unordered_map<PartialLabel, shared_ptr<CTNode>>> updated_nodes(thread_count);

    auto insertion_lambda = [&batches, &updated_nodes, this](size_t i) {
        for (size_t bi = 0; bi < batches[i].size(); bi++) {
            unordered_map<PartialLabel, shared_ptr<CTNode>> *updated_nodes_ptr = nullptr;
            if (this->storage() != nullptr) {
                // Only save updated nodes if they are actually going to be saved to storage
                updated_nodes_ptr = &updated_nodes[i];
            }

            pair<PartialLabel, hash_type> entry = batches[i][bi];
            root_->insert(entry.first, entry.second, epoch_, updated_nodes_ptr);
        }
    };

    // Perform node insertion
    vector<future<void>> insert_results(batches.size());
    for (size_t idx = 0; idx < batches.size(); idx++) {
        insert_results[idx] = tp.enqueue(insertion_lambda, idx);
    }

    // Wait until insertion is done
    for (auto &ins_result : insert_results) {
        ins_result.get();
    }

    auto hash_update_lambda = [&batches, &bit_count, &updated_nodes, this](size_t i) {
        for (auto &entry : batches[i]) {
            unordered_map<PartialLabel, shared_ptr<CTNode>> *updated_nodes_ptr = nullptr;
            if (this->storage() != nullptr) {
                updated_nodes_ptr = &updated_nodes[i];
            }

            root_->update_hashes(entry.first, bit_count + 1, updated_nodes_ptr);
        }
    };

    // Update node hashes
    vector<future<void>> update_hashes_results(batches.size());
    for (size_t idx = 0; idx < batches.size(); idx++) {
        update_hashes_results[idx] = tp.enqueue(hash_update_lambda, idx);
    }

    // Wait until hash computation is done
    for (auto &hash_result : update_hashes_results) {
        hash_result.get();
    }

    // Now we need to update the top level hashes. Gather top leaves to update.
    vector<CTNode *> nodes_to_compute;
    size_t level_idx = 0;
    vector<CTNode *> curr_level;
    curr_level.push_back(root_.get());
    nodes_to_compute.push_back(root_.get());

    while (level_idx < bit_count) {
        vector<CTNode *> next_level;
        for (auto nodeptr : curr_level) {
            shared_ptr<CTNode> left = nodeptr->left();
            shared_ptr<CTNode> right = nodeptr->right();

            if (left == nullptr) {
                throw runtime_error("This node should exist");
            }
            next_level.push_back(left.get());
            nodes_to_compute.push_back(left.get());

            if (right == nullptr) {
                throw runtime_error("This node should exist");
            }
            next_level.push_back(right.get());
            nodes_to_compute.push_back(right.get());
        }

        curr_level.swap(next_level);
        level_idx++;
    }

    // Now we can update the top level hashes. We do it in reverse order because
    // top levels are at the beginning.
    unordered_map<PartialLabel, shared_ptr<CTNode>> *updated_nodes_ptr = nullptr;
    if (this->storage() != nullptr) {
        // Save top level nodes in the last updated nodes map, to ensure this is the version that
        // persists
        updated_nodes_ptr = &updated_nodes[updated_nodes.size() - 1];
    }

    for (size_t idx = nodes_to_compute.size(); idx != 0; idx--) {
        CTNode *node = nodes_to_compute[idx - 1];
        if (node->update_hash()) {
            node->save_to_storage(updated_nodes_ptr);
        }
    }

    // Save updated nodes to storage
    if (nullptr != storage_) {
        for (size_t idx = 0; idx < updated_nodes.size(); idx++) {
            for (auto &updated_node : updated_nodes[idx]) {
                storage_->save_ctnode(
                    id(), *(reinterpret_cast<CTNodeStored *>(updated_node.second.get())));
            }
        }
    }

    // To get the append proof we need to lookup the items we just inserted
    auto lookup_lambda = [&append_proofs, &label_commit_batch, this](size_t i, size_t stride) {
        for (size_t idx = i; idx < append_proofs.size(); idx += stride) {
            PartialLabel label = label_commit_batch[idx].first;
            append_proof_type append_proof;

            if (!lookup(label, append_proof, /* include_searched */ true)) {
                append_proofs[idx] = std::move(append_proof);
                return pair<bool, PartialLabel>(false, label);
            }

            append_proofs[idx] = std::move(append_proof);
        }

        return pair<bool, PartialLabel>(true, {});
    };

    vector<future<pair<bool, PartialLabel>>> lookup_results(thread_count);
    for (size_t idx = 0; idx < thread_count; idx++) {
        lookup_results[idx] = tp.enqueue(lookup_lambda, idx, thread_count);
    }

    for (auto &lookup_result : lookup_results) {
        if (!lookup_result.get().first) {
            throw runtime_error("Should have been able to find the item we just inserted");
        }
    }

    save_to_storage();
}

bool CompressedTrie::lookup(const PartialLabel &label, lookup_path_type &path) const
{
    return lookup(label, path, /* include_searched */ true);
}

bool CompressedTrie::lookup(
    const PartialLabel &label, lookup_path_type &path, bool include_searched) const
{
    path.clear();
    root_->init(this);
    return CTNode::lookup(label, root_, path, include_searched);
}

string CompressedTrie::to_string() const
{
    return root_->to_string();
}

commitment_type CompressedTrie::get_commitment() const
{
    return root_->hash();
}

size_t CompressedTrie::save(SerializationWriter &writer) const
{
    flatbuffers::FlatBufferBuilder fbs_builder;

    fbs::CompressedTrieBuilder ct_builder(fbs_builder);
    ct_builder.add_epoch(epoch());
    ct_builder.add_version(ozks_serialization_version);
    ct_builder.add_id(id_);
    ct_builder.add_thread_count(static_cast<uint32_t>(thread_count_));
    ct_builder.add_trie_type(static_cast<uint8_t>(trie_type_));

    auto fbs_ct = ct_builder.Finish();
    fbs_builder.FinishSizePrefixed(fbs_ct);

    writer.write(fbs_builder.GetBufferPointer(), fbs_builder.GetSize());

    return (fbs_builder.GetSize());
}

size_t CompressedTrie::save(ostream &stream) const
{
    StreamSerializationWriter writer(&stream);
    return save(writer);
}

template <class T>
size_t CompressedTrie::save(vector<T> &vec) const
{
    VectorSerializationWriter writer(&vec);
    return save(writer);
}

pair<shared_ptr<CompressedTrie>, size_t> CompressedTrie::Load(
    SerializationReader &reader, shared_ptr<storage::Storage> storage)
{
    vector<unsigned char> in_data(utils::read_from_serialization_reader(reader));

    auto verifier =
        flatbuffers::Verifier(reinterpret_cast<uint8_t *>(in_data.data()), in_data.size());
    bool safe = fbs::VerifySizePrefixedCompressedTrieBuffer(verifier);

    if (!safe) {
        throw runtime_error("Failed to load Compressed Trie: invalid buffer");
    }

    auto fbs_ct = fbs::GetSizePrefixedCompressedTrie(in_data.data());
    auto ct = make_shared<CompressedTrie>();
    ct->epoch_ = fbs_ct->epoch();
    ct->thread_count_ = fbs_ct->thread_count();
    ct->id_ = fbs_ct->id();
    ct->trie_type_ = static_cast<TrieType>(fbs_ct->trie_type());
    ct->storage_ = storage;

    CTNodeStored root(ct.get());
    if (ct->storage() != nullptr) {
        if (!ct->storage()->load_ctnode(ct->id(), {}, nullptr, root)) {
            throw runtime_error("Failed to load root");
        }
        root.init(ct.get());
        ct->init(make_shared<CTNodeStored>(root));
    }

    return { ct, in_data.size() };
}

pair<shared_ptr<CompressedTrie>, size_t> CompressedTrie::Load(
    istream &stream, shared_ptr<storage::Storage> storage)
{
    StreamSerializationReader reader(&stream);
    return Load(reader, storage);
}

template <class T>
pair<shared_ptr<CompressedTrie>, size_t> CompressedTrie::Load(
    const vector<T> &vec, shared_ptr<storage::Storage> storage, size_t position)
{
    VectorSerializationReader reader(&vec, position);
    return Load(reader, storage);
}

void CompressedTrie::save_to_storage() const
{
    if (nullptr != storage_) {
        storage_->save_compressed_trie(*this);
    }
}

pair<shared_ptr<CompressedTrie>, bool> CompressedTrie::LoadFromStorage(
    trie_id_type trie_id, shared_ptr<storage::Storage> storage)
{
    if (nullptr == storage) {
        throw invalid_argument("storage is null");
    }

    auto trie = make_shared<CompressedTrie>();
    bool loaded = storage->load_compressed_trie(trie_id, *trie);
    if (!loaded) {
        return { nullptr, loaded };
    }
    trie->init(storage);

    // Now load the root
    auto root = make_shared<CTNodeStored>();
    if (!storage->load_ctnode(trie_id, {}, storage, *root)) {
        throw runtime_error("Could not load root");
    }
    root->init(trie.get());
    trie->init(root);
    trie->trie_type_ = TrieType::Stored;

    return { trie, loaded };
}

pair<shared_ptr<CompressedTrie>, bool> CompressedTrie::LoadFromStorageWithChildren(
    trie_id_type trie_id, shared_ptr<storage::Storage> storage)
{
    if (nullptr == storage) {
        throw invalid_argument("storage is null");
    }

    auto trie = make_shared<CompressedTrie>();
    bool loaded = storage->load_compressed_trie(trie_id, *trie);
    if (!loaded) {
        return { nullptr, false };
    }
    trie->init(storage);

    // Now load the children
    CTNodeStored snode;
    if (!storage->load_ctnode(trie_id, {}, storage, snode)) {
        throw runtime_error("Could not load root");
    }
    auto root = make_shared<CTNodeLinked>(trie.get(), snode.label(), snode.hash());
    trie->init(root);
    trie->trie_type_ = TrieType::Linked;

    trie->root_->load_from_storage(storage, snode.left_label(), snode.right_label());

    return { trie, true };
}

void CompressedTrie::init(shared_ptr<storage::Storage> storage)
{
    storage_ = std::move(storage);
}

void CompressedTrie::init_random_id()
{
    utils::random_bytes(reinterpret_cast<byte *>(&id_), sizeof(trie_id_type));
}

void CompressedTrie::init(shared_ptr<CTNode> root)
{
    root_ = std::move(root);
}

void CompressedTrie::init_empty_root()
{
    switch (trie_type_) {
    case TrieType::Linked:
    case TrieType::LinkedNoStorage:
        root_ = make_shared<CTNodeLinked>(this);
        break;
    case TrieType::Stored:
        root_ = make_shared<CTNodeStored>(this);
        break;
    default:
        throw invalid_argument("Invalid trie type");
    }

    root_->save_to_storage();
}

// Explicit instantiations
template size_t CompressedTrie::save(vector<uint8_t> &vec) const;
template size_t CompressedTrie::save(vector<byte> &vec) const;
template pair<shared_ptr<CompressedTrie>, size_t> CompressedTrie::Load(
    const vector<uint8_t> &vec, shared_ptr<storage::Storage> storage, size_t position);
template pair<shared_ptr<CompressedTrie>, size_t> CompressedTrie::Load(
    const vector<byte> &vec, shared_ptr<storage::Storage> storage, size_t position);
