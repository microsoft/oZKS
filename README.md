# Ordered Zero Knowledge Set - oZKS
## Introduction 
oZKS is a library that provides an implementation of an Ordered (and Append Only) Zero Knowledge Set. An Ordered (and Append Only) Zero Knowledge Set is a cryptographic primitive with the following properties:

Data Structure: Let S be an Ordered Key-Value Dictionary where each element is of the form (label,value,e), where e is the epoch in which the elements got added to the dictionary. 
New (label',value',e') pairs can be added to S (such that label' did not already exist). But once a (label,value,epoch) tuple is added, it cannot be modified or removed.

Algorithms: An Ordered (and Append Only) Zero Knowledge Set lets a Prover (P) produce a short cryptographic commitment c for S such that it can later produce succinct proofs of membership and non-membership of a tuple with respect to c. 
In more detail, the prover can produce proofs of membership of any label *l* &#x2208; *S* (and corresponding value) or non-membership of label *l* &#x2209; *S* corresponding to a certain epoch t with respect to c. These proofs reveal no extra information beyond the assertion (and some well-defined leakage).

The Prover can also update a commitment *c<sub>1</sub>* to *c<sub>2</sub>* if new elements get added to the underlying set *S<sub>1</sub>* to produce *S<sub>2</sub>* and produce a cryptographic proof that *S<sub>1</sub>* &#x2286; *S<sub>2</sub>* with respect to *c<sub>1</sub>*, *c<sub>2</sub>*.
This proof does not leak any extra information beyond the assertion (and some well-defined leakage) either, just like the membership and non-membership proofs.

For more details we refer the readers to [Section 5 of the SEEMless paper](https://eprint.iacr.org/2018/607.pdf).


## Getting Started
The OZKS class (defined in `ozks.h`) provides the main API for use of the library. It provides the following functionality:

### Insertion
Insert a key and payload, or a batch of keys and payloads.

```C++
InsertResult OZKS::insert(const key_type &key, const payload_type &payload);
InsertResultBatch OZKS::insert(const key_payload_batch_type &input);
```

### Querying
Query for the presence/non-presence of a given key.
```C++
QueryResult OZKS::query(const key_type &key) const;
```

### Verification
Verify that the `QueryResult` returned by a query has a correct proof.
```C++
bool QueryResult::verify(const key_type &key, const Commitment &commitment) const;
```

## Building and Installing oZKS

oZKS has multiple external dependencies that must be pre-installed.
By far the easiest and recommended way to do this is using [vcpkg](https://github.com/microsoft/vcpkg).
Each package's name in vcpkg is listed below.

**Note:** On Windows, the vcpkg triplet `x64-windows-static-md` should be specified.
For example, to install FlatBuffers on Windows, you must use `.\vcpkg install flatbuffers:x64-windows-static-md`, while on other systems use simply `./vcpkg install flatbuffers`.

The CMake build system can then automatically find these pre-installed packages, if the following arguments are passed to CMake configuration:
- `-DCMAKE_TOOLCHAIN_FILE=${vcpkg_root_dir}/scripts/buildsystems/vcpkg.cmake`, and
- `-DVCPKG_TARGET_TRIPLET="x64-windows-static-md"` on Windows only.

| Dependency                                                | vcpkg name                                           |
|-----------------------------------------------------------|------------------------------------------------------|
| [FlatBuffers](https://github.com/google/flatbuffers)      | `flatbuffers`                                        |
| [GSL](https://github.com/microsoft/GSL/)                  | `ms-gsl`                                             |
| [Google Test](https://github.com/google/googletest)       | `gtest` (needed only for building tests)             |

To build the unit tests, set the CMake option `OZKS_BUILD_TESTS` to `ON`.

## Contribute
For contributing to oZKS, please see [CONTRIBUTING.md](CONTRIBUTING.md).
