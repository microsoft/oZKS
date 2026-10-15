# Ordered Zero-Knowledge Set - oZKS

## Introduction

oZKS is a library that provides an implementation of an Ordered (and Append Only) Zero-knowledge Set. Zero-knowledge sets allow a prover to convince a verifier that a given value is a member of a specific set without revealing any information about the value or the set itself. oZKS implements a Zero-knowledge dictionary, where the user can add Key/Value pairs in a given epoch, obtain a proof of the correctness of the added Key/Value pair, and then retrieve a proof of whether a given Key is present in the dictionary.

## Zero-Knowledge Set definition

An Ordered (and Append-Only) Zero-Knowledge Set is a cryptographic primitive with the following properties:

### Data Structure
Let S be an Ordered Key-Value Dictionary where each element is of the form (label, value, e), where e is the epoch in which the elements got added to the dictionary.
New (label', value', e') pairs can be added to S (such that label' did not already exist). But once a (label, value, epoch) tuple is added, it cannot be modified or removed.

### Algorithms
An Ordered (and Append Only) Zero Knowledge Set lets a Prover (P) produce a short cryptographic commitment c for S such that it can later produce succinct proofs of membership and non-membership of a tuple with respect to c.
In more detail, the prover can produce proofs of membership of any label *l* &#x2208; *S* (and corresponding value) or non-membership of label *l* &#x2209; *S* corresponding to a certain epoch t with respect to c. These proofs reveal no extra information beyond the assertion (and some well-defined leakage).

The Prover can also update a commitment *c<sub>1</sub>* to *c<sub>2</sub>* if new elements get added to the underlying set *S<sub>1</sub>* to produce *S<sub>2</sub>* and produce a cryptographic proof that *S<sub>1</sub>* &#x2286; *S<sub>2</sub>* with respect to *c<sub>1</sub>*, *c<sub>2</sub>*.
This proof does not leak any extra information beyond the assertion (and some well-defined leakage) either, just like the membership and non-membership proofs.

For more details we refer the readers to [Section 5 of the SEEMless paper](https://eprint.iacr.org/2018/607.pdf).

## oZKS Implementation

The core data structure of oZKS is a Patricia Trie, where leafs contain the keys added to the dictionary. Non-leaf nodes contain prefixes that are common to the added leaves. Each node also contains a hash. Leaf hashes cover the leaf label, the payload commitment and the insertion epoch; internal-node hashes cover the labels and hashes of both children (see Serialization versioning and compatibility). The core trie structure supports add / query operations.

### Examples

The API of the core trie structure is not meant to be public. The public API would be an implementation that takes the core data structure and performs additional computations in order to serve as a useful protocol.

The oZKS library is meant to be flexible, and as such there is no single implementation of an actual oZKS class that provides a public API for users to consume. This is because the requirements of the public-facing class will entirely depend on what and how the system is supposed to work. The class will be different if the system is meant to run on a single machine, or it is meant to run in a distributed highly-scalable system, for example.

We have provided two different implementations of what a public API could look like in the [examples](examples/) directory.

The first example implements oZKS running in a single executable ([ozks_simple](examples/ozks_simple/)).

The second example implements oZKS as it might look running in a distributed system ([ozks_distributed](examples/ozks_distributed/)). In this example, Key/Value pairs are not directly inserted into the dictionary. They are kept in a "pending updates" structure. An update service runs every certain number of seconds, takes entries found in this structure and inserts them in the directory in a batch operation. The query functionality in this case is delegated to 4 different Querier objects, each one holding a copy of the directory in memory.

The two examples still run on a single machine. oZKS defines 'provider' interfaces that can be used to change the implementation to a distributed implementation. For example, the [Query provider](examples/ozks_distributed/providers/dist_query_provider.h) for the ozks_distributed example simply creates 4 instances of the [Querier](examples/ozks_distributed/providers/querier/querier.h) class and queries one of them at random. In a real distributed system, the Query provider could instead send a REST request to a backend that provides scalable query capabilities, such as a pool of machines that hold a copy of the trie. The same can be done with the [Update provider](examples/ozks_distributed/providers/dist_update_provider.h), the implementation could simply send a request to a backend whose only purpose is to handle trie insertions.

### Storage

The oZKS library provides a flexible way to store its data (that is, trie nodes, trie themselves, and user data). The concept of 'storage' is abstracted in the [Storage](oZKS/storage/storage.h) interface. Different implementations of this concept provide flexibility in the configuration of the system for different needs.

For example, a simple [memory storage](oZKS/storage/memory_storage.h) implementation is provided which holds all information in memory. This same interface could be implemented in a persistent database or file storage as well.

Abstracting the storage this way allows using different storage implementations in layers. For example, oZKS provides a [memory cache](oZKS/storage/memory_storage_cache.h) storage implementation that holds a given number of elements in memory, using an LRU policy to evict items when the capacity is exceeded. This storage implementation receives as parameter a backing storage, which is where it gets items from and where it saves updated items to. One could easily imagine using a memory cache storage with a database storage implementation as backing storage. This would provide the benefits of persistence, while also providing the benefits of quick access to the most accessed elements.

The abstract storage concept is also used to speed-up database operations. Imagine that you have a database storage implementation. Inserting values into a dictionary backed by a database storage would be very slow, as each node update would require a round-trip to the database. Updates to a database are more efficient when applied in a batch. oZKS provides a [batch insert](oZKS/storage/memory_storage_batch_inserter.h) storage implementation, which holds updated elements in memory until a 'flush' command is received. When the command is received, all updated elements are then sent to the backing storage.

### Elliptic Curve

oZKS uses a Verifiable Random Function (VRF) to map the actual key value to a random position in the trie, to provide zero-knowledge. The VRF is implemented using the [OpenSSL](https://github.com/openssl/openssl) NIST P-256 elliptic curve.

## Getting Started

The OZKS class (defined in [`examples/ozks_simple/ozks.h`](examples/ozks_simple/ozks.h)) provides an example of the main API for use of the library. It provides the following functionality:

### Insertion

Insert a key and payload, or a batch of keys and payloads.

```C++
std::shared_ptr<InsertResult> OZKS::insert(const key_type &key, const payload_type &payload);
InsertResultBatch OZKS::insert(const key_payload_batch_type &input);
```

Keys are not inserted immediately in the tree. Any key/payload that is inserted through these methods will become a pending insertion. Pending insertions are actually inserted in the set when calling the `flush` method:

```C++
void OZKS::flush();
```
After calling `flush`, pending keys/elements are inserted and its epoch will be increased.


### Querying

Query for the presence/non-presence of a given key.
```C++
QueryResult OZKS::query(const key_type &key) const;
```

### Verification

`InsertResult` objects returned from an `insert` operation and `QueryResult` objects returned from a `query` operation can be verified for correctness.

Verify that the `QueryResult` returned by a `query` has a correct proof and is the answer to the key that was requested:
```C++
QueryVerificationResult QueryResult::verify(
    const Commitment &commitment, const key_type &expected_key) const;
```

The `Commitment` received by the method as parameter should be built from the information that was published publicly, as specified in the SEEMless protocol.
The overload without `expected_key` performs the same verification but cannot know which key was requested; prefer the two-argument form, which returns `NotValid` for a result that answers a different key.

Verify that the `InsertResult` from an `insert` operation provides a correct append proof:

```C++
bool InsertResult::verify() const;
```

`QueryResult::verify` authenticates the proof against the trusted `Commitment` passed to it.
`InsertResult::verify` checks the append proof against the commitment carried in the result (`InsertResult::commitment()`); applications must compare that commitment with the published one.
Both verifiers report a malformed proof as invalid rather than throwing; `InsertResult::verify` throws only when called on an uninitialized result.
`is_member()` and successful deserialization are not authentication.
Applications using the single-argument `verify` must check themselves that the key in a returned `QueryResult` is the key they asked for.

## Serialization versioning and compatibility

### Policy

oZKS has a single serialization version, `ozks_serialization_version` in `oZKS/version.h`.
It is written into, and checked on load for, every format the library defines: commitments, compressed tries, stored nodes (`CTNodeStored::save`/`Load`), query results, insert results, the configuration blob, and the example `OZKS` instances.
Every loader rejects any other version.
A FlatBuffers buffer that predates the field reads back as version 0; a legacy configuration blob reads its leading type bytes as a version value that can never equal a supported version. Both are rejected like any other mismatch.

The version also fixes the commitment-structure hashes: it is embedded in the leaf and node hash domains (`leaf_hash_v2`, `node_hash_v2`) and the hash input encoding is defined per version.
Bumping the version therefore invalidates all persisted data and every published commitment.
Treat each bump as a full migration (see below). Wire-only or storage-only changes must still bump the version; batch such changes together, and never add an unversioned field or format.

### Version 2

Version 2 is the first version whose hashes and formats are fully specified.
Version 2 has not shipped; until it does, its formats may still change without a further bump. Once shipped, any change requires version 3.
Relative to version 1:

- Leaf and node hashes cover the label's bit count as well as its bytes. Labels are hashed as their 32 bytes in bit order (bits past the bit count are zero) followed by the bit count as a 4-byte big-endian integer. A leaf hashes the encoded label, the payload commitment hash and the insertion epoch as an 8-byte big-endian integer; a node hashes the encoded label and hash of each child. The encoding does not depend on the width of `std::size_t`; the library requires a little-endian host, enforced at compile time.
- The hash domains embed the version (`leaf_hash_v2`, `node_hash_v2`; version 1 used the unversioned `leaf_hash` and `node_hash`), so hashes from different versions are domain-separated.
- Nonempty-tree commitments change. The empty-tree commitment (all zeros) is unchanged.
- Query results carry the version and the queried leaf's original insertion epoch. Membership verification recomputes the leaf hash from the returned payload, its opening randomness (committed-payload mode only) and that epoch, under the caller's configured payload commitment mode.
- Query results must match the caller's configured label mode; serialized data cannot enable or disable VRF verification. VRF-mode results require a proof and a trusted commitment that carries the VRF public key.
- Commitments state the VRF suite of their public key, and the stated suite must match the key on load.
- Stored nodes, insert results and the configuration blob carry the version.

Stored nodes persisted through `CTNodeStored::save`/`Load` are checked; a storage backend that keeps node fields in its own record format must version its records itself.
The in-memory backends hold node objects and never serialize them.

### Integrator changes in version 2

- Query providers must return the leaf insertion epoch for single and batched lookups; use zero for absent keys.
- Pass that epoch to the `QueryResult` constructors, not the current root epoch.
- Unversioned query results, insert results, stored nodes and configuration blobs are rejected.

### Migration checklist for any version bump

1. Do not relabel old serialized objects with the new version.
2. Rebuild persisted tries from the authoritative insertion history, preserving each leaf's insertion epoch.
3. Publish and independently distribute the new trusted commitments before serving proofs under the new version.
4. Regenerate query and insertion proofs against the rebuilt tries.
5. Retain historical data under the old version separately if it is still needed.

## Building and Installing oZKS

oZKS has multiple external dependencies that must be pre-installed.
By far the easiest and recommended way to do this is using [vcpkg](https://github.com/microsoft/vcpkg).
Each package's name in vcpkg is listed below.

The CMake build system can then automatically find these pre-installed packages, if the following arguments are passed to CMake configuration:
- `-DCMAKE_TOOLCHAIN_FILE=${vcpkg_root_dir}/scripts/buildsystems/vcpkg.cmake`

| Dependency                                                | vcpkg name                                           |
|-----------------------------------------------------------|------------------------------------------------------|
| [FlatBuffers](https://github.com/google/flatbuffers)      | `flatbuffers`                                        |
| [GSL](https://github.com/microsoft/GSL/)                  | `ms-gsl`                                             |
| [OpenSSL](https://github.com/openssl/openssl)             | `openssl`                                            |
| [C++ VRF](https://github.com/microsoft/cppvrf)            | `cppvrf`                                             |
| [POCO](https://github.com/pocoproject/poco)               | `poco`                                               |
| [Google Test](https://github.com/google/googletest)       | `gtest` (needed only for building tests)             |
| [Google Benchmark](https://github.com/google/benchmark)   | `benchmark` (needed only for building benchmarks)    |

To build the examples, set the CMake options `OZKS_BUILD_EXAMPLES` to `ON`.

To build the unit tests, set the CMake option `OZKS_BUILD_TESTS` to `ON`.

To build the performance benchmarks, set the CMake option `OZKS_BUILD_BENCH` to `ON`.

### Building dependencies automatically
A file called `vcpkg.json` is provided in the root directory of this repository. This file is used by `vcpkg` to automatically build the dependencies that are needed by oZKS. When you configure the oZKS project using CMake, part of the initial configuration process will be to build the dependencies specified in the file. If the dependencies were pre-installed then they will not be compiled, they will simply be used.

## Contribute

For contributing to oZKS, please see [CONTRIBUTING.md](CONTRIBUTING.md).
