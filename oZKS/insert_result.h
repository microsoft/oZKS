// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license.

#pragma once

// STL
#include <iostream>
#include <memory>
#include <stdexcept>

// oZKS
#include "oZKS/defines.h"
#include "oZKS/partial_label.h"
#include "oZKS/serialization_helpers.h"

namespace ozks {
    /**
    Represents the result of an insertion into an oZKS instance
    */
    class InsertResult {
    public:
        /**
        Construct an instance of InsertResult
        */
        InsertResult() = default;

        /**
        Construct an instance of InsertResult
        */
        InsertResult(const commitment_type &commitment, const append_proof_type &append_proof);

        /**
        Commitment generated by the insertion
        */
        const commitment_type &commitment() const
        {
            if (nullptr == commitment_) {
                throw std::runtime_error("Commitment not initialized yet");
            }

            return *commitment_;
        }

        /**
        Append proof
        */
        const append_proof_type &append_proof() const
        {
            if (nullptr == append_proof_) {
                throw std::runtime_error("Append proof not initialized yet");
            }

            return *append_proof_;
        }

        /**
        Whether this InsertResult has been initialized
        */
        bool initialized() const
        {
            return !(nullptr == commitment_ || nullptr == append_proof_);
        }

        /**
        Initialize this InsertResult
        */
        void init_result(const commitment_type &commitment, const append_proof_type &append_proof)
        {
            commitment_ = std::make_unique<commitment_type>(commitment);
            append_proof_ = std::make_unique<append_proof_type>(append_proof);
        }

        /**
        Verify the append proof and commitment
        */
        bool verify() const;

        /**
        Save this insert result to a stream
        */
        std::size_t save(std::ostream &stream) const;

        /**
        Save this insert result to a byte vector
        */
        template <typename T>
        std::size_t save(std::vector<T> &vector) const;

        /**
        Load an insert result from a stream
        */
        static std::pair<InsertResult, std::size_t> Load(std::istream &stream);

        /**
        Load an insert result from a vector
        */
        template <typename T>
        static std::pair<InsertResult, std::size_t> Load(
            const std::vector<T> &vector, std::size_t position = 0);

    private:
        std::unique_ptr<commitment_type> commitment_;
        std::unique_ptr<append_proof_type> append_proof_;

        std::size_t save(SerializationWriter &writer) const;
        static std::pair<InsertResult, std::size_t> Load(SerializationReader &reader);
    };

    using InsertResultBatch = std::vector<std::shared_ptr<InsertResult>>;
} // namespace ozks
