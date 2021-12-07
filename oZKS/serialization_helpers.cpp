// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

// STD
#include <cstring>
#include <sstream>

// OZKS
#include "oZKS/serialization_helpers.h"
#include "oZKS/utilities.h"

using namespace std;
using namespace ozks;

void StreamSerializationWriter::write(const void *bytes, size_t size)
{
    auto old_except_mask = stream_->exceptions();

    try {
        stream_->exceptions(ios_base::badbit | ios_base::failbit);
        stream_->write(reinterpret_cast<const char *>(bytes), size);
    } catch (ios_base::failure &ex) {
        stream_->exceptions(old_except_mask);

        stringstream ss;
        ss << "IO Error: " << ex.what();
        throw runtime_error(ss.str());
    } catch (...) {
        stream_->exceptions(old_except_mask);
        throw;
    }

    stream_->exceptions(old_except_mask);
}

template <class T>
void VectorSerializationWriter<T>::write(const void *bytes, size_t size)
{
    size_t original_size = vec_->size();
    vec_->resize(original_size + size);
    utils::copy_bytes(bytes, size, vec_->data() + original_size);
}

void StreamSerializationReader::read(void *destination, size_t size)
{
    auto old_except_mask = stream_->exceptions();

    try {
        stream_->exceptions(ios_base::badbit | ios_base::failbit);
        stream_->read(reinterpret_cast<char *>(destination), size);
    } catch (ios_base::failure &ex) {
        stream_->exceptions(old_except_mask);

        stringstream ss;
        ss << "IO Error: " << ex.what();
        throw runtime_error(ss.str());
    } catch (...) {
        stream_->exceptions(old_except_mask);
        throw;
    }

    stream_->exceptions(old_except_mask);
}

template <class T>
void VectorSerializationReader<T>::read(void *destination, size_t size)
{
    if (vec_->size() < (position_ + size)) {
        stringstream ss;
        ss << "Tried to read past the end of the vector. vec_size: " << vec_->size()
           << ", position: " << position_ << " size to read: " << size;
        throw runtime_error(ss.str());
    }

    utils::copy_bytes(vec_->data() + position_, size, destination);
    position_ += size;
}

// Explicit instantiations
template class ozks::VectorSerializationWriter<std::uint8_t>;
template class ozks::VectorSerializationWriter<std::byte>;
template class ozks::VectorSerializationReader<std::uint8_t>;
template class ozks::VectorSerializationReader<std::byte>;
