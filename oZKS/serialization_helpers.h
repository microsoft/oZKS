// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STD
#include <cstddef>
#include <iostream>
#include <vector>

namespace ozks {
    /**
    Base class of a generic serialization writer
    */
    class SerializationWriter {
    public:
        /**
        Write bytes to the serialized output
        */
        virtual void write(const void *bytes, std::size_t size) = 0;
    };

    /**
    Implementation of a serialization writer using an output stream
    */
    class StreamSerializationWriter : public SerializationWriter {
    public:
        StreamSerializationWriter(std::ostream *stream) : stream_(stream)
        {}

        /**
        Write bytes to the output stream
        */
        void write(const void *bytes, std::size_t size) override;

    private:
        std::ostream *stream_;
    };

    /**
    Implementation of a serialization writer using a byte vector
    */
    template <typename T>
    class VectorSerializationWriter : public SerializationWriter {
    public:
        VectorSerializationWriter(std::vector<T> *vec) : vec_(vec)
        {
            static_assert(sizeof(T) == 1);
        }

        /**
        Write bytes to the output vector
        */
        void write(const void *bytes, std::size_t size) override;

    private:
        std::vector<T> *vec_;
    };

    /**
    Base class of a generic serialization reader
    */
    class SerializationReader {
    public:
        /**
        Read bytes from the input
        */
        virtual void read(void *destination, std::size_t size) = 0;
    };

    /**
    Implementation of a serialization reader using an input stream
    */
    class StreamSerializationReader : public SerializationReader {
    public:
        StreamSerializationReader(std::istream *stream) : stream_(stream)
        {}

        /**
        Read bytes from the input stream
        */
        void read(void *destination, std::size_t size) override;

    private:
        std::istream *stream_;
    };

    /**
    Implementation of a serialization reader using a byte vector
    */
    template <typename T>
    class VectorSerializationReader : public SerializationReader {
    public:
        VectorSerializationReader(const std::vector<T> *vec, std::size_t position)
            : vec_(vec), position_(position)
        {
            static_assert(sizeof(T) == 1);
        }

        /**
        Read bytes from the byte vector
        */
        void read(void *destination, std::size_t size) override;

    private:
        const std::vector<T> *vec_;
        std::size_t position_;
    };
} // namespace ozks
