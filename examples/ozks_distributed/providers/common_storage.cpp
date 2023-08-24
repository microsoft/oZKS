// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// OZKS
#include "common_storage.h"
#include "oZKS/storage/memory_storage.h"

using namespace std;
using namespace ozks_distributed;
using namespace ozks_distributed::providers;

namespace {
    shared_ptr<ozks::storage::Storage> common_storage_ = make_shared<ozks::storage::MemoryStorage>();
}

shared_ptr<ozks::storage::Storage> ozks_distributed::providers::get_common_storage()
{
    return common_storage_;
}
