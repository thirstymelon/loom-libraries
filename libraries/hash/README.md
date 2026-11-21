# Hash — Cryptographic & Fast Non-Cryptographic Hashing for Loom

`Hash` provides hardware-accelerated cryptographic digests, message authentication codes (HMAC), streaming file checksums, and high-performance non-cryptographic hashing for Loom.

## Installation

```bash
loom get hash
```

## Features

- **Cryptographic Hashes**: SHA-256, SHA-512, SHA-224, SHA-384, MD5, SHA-1.
- **Fast Non-Cryptographic Hash**: 64-bit FNV-1a (optimized for hash maps, routing tables, and caches).
- **HMAC (Message Authentication)**: HMAC-SHA256, HMAC-SHA512, HMAC-MD5, HMAC-SHA1.
- **Constant Memory File Hashing**: Stream multi-gigabyte files directly from disk without loading them entirely into RAM (`Hash.File_SHA256`).
- **Stateful Streaming Hasher**: Process chunks, network frames, or incoming packets iteratively (`Hasher_Init`, `Hasher_Update`, `Hasher_Final`).

## Quick Start

```loom
Import Hash
use Hash

Block Main
Declares
    # SHA-256 Digest
    Digest : Text = Hash.SHA256("Hello, Loom!")

    # Fast 64-bit non-cryptographic hash
    Key_Hash : Unsigned_64 = Hash.FNV1a("session_token_12345")

    # Streaming file hash (zero RAM overhead)
    Checksum : Text = Hash.File_SHA256("archive.tar.gz")

    # Keyed-Hash Message Authentication (HMAC)
    Tag : Text = Hash.HMAC_SHA256("secret_key", "authenticated_payload")
End Main
```

## API Reference

### One-Shot Cryptographic Hashes

- `Hash.SHA256(Input : Text) -> Text`
- `Hash.SHA512(Input : Text) -> Text`
- `Hash.SHA224(Input : Text) -> Text`
- `Hash.SHA384(Input : Text) -> Text`
- `Hash.MD5(Input : Text) -> Text`
- `Hash.SHA1(Input : Text) -> Text`

### Non-Cryptographic Hash

- `Hash.FNV1a(Input : Text) -> Unsigned_64`

### Keyed-Hash Message Authentication (HMAC)

- `Hash.HMAC_SHA256(Key : Text, Message : Text) -> Text`
- `Hash.HMAC_SHA512(Key : Text, Message : Text) -> Text`
- `Hash.HMAC_MD5(Key : Text, Message : Text) -> Text`
- `Hash.HMAC_SHA1(Key : Text, Message : Text) -> Text`

### File Checksums (Streaming)

- `Hash.File_SHA256(Path : Text) -> Text`
- `Hash.File_SHA512(Path : Text) -> Text`
- `Hash.File_MD5(Path : Text) -> Text`
- `Hash.File_SHA1(Path : Text) -> Text`

### Stateful Chunked Hasher

```loom
Block Hash_Chunks Takes Chunks : Vector[Text] Produces Text
Declares
    H : Hasher
    Idx : Signed_32
Implements
    H = Hash.Hasher_Init(Algorithm_SHA256)
    Loop Idx In 0 .. (Chunks.Length() - 1)
        Hash.Hasher_Update(H, Chunks[Idx])
    End Loop
    Give Hash.Hasher_Final(H)
End Hash_Chunks
```
