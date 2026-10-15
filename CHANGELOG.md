# Changelog

## 2.0.0 - 2026-10-15

This release introduces serialization version 2.
It is incompatible with every earlier release: it changes the commitment-structure hashes, the wire and storage formats, and the public API.
See the README section "Serialization versioning and compatibility" for the policy and the migration checklist.

### Security fixes in client-side verification

- Label lengths are bound into leaf and internal-node hashes.
  Previously a responder could shorten a leaf label in a genuine membership proof and have the client accept it as a non-membership proof.
- Query verification is bound to the queried key's route through the trie.
  Previously a responder could answer a query for one key with another key's valid membership path and obtain a non-membership result, and could present the root commitment as a leaf hash to fabricate membership of an absent key.
- The returned payload and the leaf's insertion epoch are authenticated.
  Previously a responder could return any payload with a genuine proof.
- The VRF label mode comes only from the client's configuration.
  Previously serialized data could disable VRF verification.
- Verification can be bound to the requested key with `verify(commitment, expected_key)`.
  `is_member()` and successful deserialization are not authentication.
- Malformed query and append proofs are reported as invalid instead of making verification throw.
  For a truncated VRF proof that the VRF library reports as initialized, `verify_lookup_path` returns `NotValid` and `is_member()` returns `false` instead of throwing on its wrong-size VRF value.
- Append-proof verification folds the unary root before comparing with the commitment, so a supplied hash can never be accepted as the commitment directly.

### Format changes

- `ozks_serialization_version` is 2.
  Every loader (`Commitment`, `CompressedTrie`, `CTNodeStored`, `QueryResult`, `InsertResult`, `OZKSConfig`, and the example `OZKS` instances) rejects any other version with an "unsupported version" error.
  Version 1 objects, and FlatBuffers written before the field existed (read back as version 0), fail to load.
  `CompressedTrie::Load` previously accepted any version.
- Leaf and node hash inputs use a canonical encoding: label bytes in bit order, bit count as a 4-byte big-endian integer, epoch as an 8-byte big-endian integer.
  Leaf hash inputs are 76 bytes and node hash inputs 136 bytes, independent of `std::size_t`.
- The hash domains embed the serialization version (`leaf_hash_v2`, `node_hash_v2`).
  Version 1 used the unversioned `leaf_hash` and `node_hash`; `key_hash`, `randomness_hash` and `commitment_hash` are unchanged.
- Query results carry the serialization version and the queried leaf's insertion epoch.
- Stored nodes persisted through `CTNodeStored::save`/`Load`, insert results and the configuration blob carry the serialization version.
  A storage backend with its own node record format must version its records itself.
- Commitments state the VRF suite of their public key; `Commitment::Load` rejects a commitment whose stated suite is missing or does not match the key.

### API changes

- `QueryProvider::query(trie_id, label, lookup_path, std::size_t &leaf_epoch)` and `QueryProvider::query(trie_id, labels, found, lookup_paths, std::vector<std::size_t> &leaf_epochs)` gain trailing out-parameters for the leaf insertion epoch (zero when absent).
  Both are pure virtual; every provider must implement them.
- The `QueryResult` constructors take a trailing `std::size_t epoch`, the leaf's original insertion epoch, exposed by the new `QueryResult::epoch()`.
- New `QueryResult::verify(const Commitment &, const key_type &expected_key)`, the preferred verification call; it rejects a result for any key other than the one requested.
- New `CompressedTrie::lookup(const PartialLabel &, lookup_path_type &, std::size_t &leaf_epoch)` and `utils::compute_payload_commitment(payload, PayloadCommitmentType, randomness)`.
- `QueryResult::verify_lookup_path` and `InsertResult::verify` no longer throw on malformed proofs; `InsertResult::verify` throws only when called on an uninitialized result.
- `QueryResult::Load` throws on an unsupported version, a label mode that does not match the configuration, an out-of-range epoch, or a missing or invalid VRF proof in VRF mode.
  `QueryResult::save` throws for a VRF-mode result without an initialized proof.
- `QueryResult::Load` and `InsertResult::Load` reject a proof element count above `max_proof_length` (257) before allocating, so a hostile buffer cannot force a large allocation.
- `QueryResult::verify_vrf_proof` returns false when the proof's VRF suite differs from the key's.
- `Commitment::Load` throws when the stated VRF suite is missing or does not match the public key.

### Build and packaging

- The library version is 2.0.0; `find_package(OZKS 2.0)` is required, and 1.x requests no longer match.
- The installed CMake package directory is `lib/cmake/OZKS-2.0`, headers install under `include/OZKS-2.0`, and the library file is `ozks-2.0`.
- A little-endian host is required at compile time (`static_assert` in `oZKS/partial_label.cpp`).
  `PartialLabel` always assumed it; a big-endian build previously compiled and produced different commitments.

### Migration

1. Do not relabel old serialized objects with the new version.
2. Rebuild persisted tries from the authoritative insertion history, preserving each leaf's insertion epoch.
3. Publish and independently distribute the new trusted commitments before serving proofs under the new version.
4. Regenerate query and insertion proofs against the rebuilt tries.
5. Retain historical data under the old version separately if it is still needed.
