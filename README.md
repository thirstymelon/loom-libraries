# Loom Libraries

Official packages and frameworks for the [Loom Programming Language](https://github.com/thirstymelon/Loom).

## Installation

Install packages using the `loom` CLI:

```bash
loom get <library>
```

Example:
```bash
loom get hash
```

## Packages

| Library | Version | Description |
|---|---|---|
| [`hash`](libraries/hash/) | `1.0.0` | Cryptographic digests (SHA-256, SHA-512, MD5, SHA-1), HMAC, 64-bit FNV-1a, and streaming file checksums. |

## License

[MIT](LICENSE)

## Toolchains

Install hardware toolchains using the `loom` CLI:

```bash
# Download and install RP2040 embedded toolchain
loom get toolchain rp2040

# Set active toolchain
loom set toolchain rp2040

# Switch back to native host toolchain
loom set toolchain native
```

| Toolchain | Version | Target | Description |
|---|---|---|---|
| [`rp2040`](toolchains/rp2040/) | `1.5.0` | `thumbv6m-none-eabi` | Loom RP2040 Embedded Bare-Metal Toolchain (ARM Cortex-M0+) |
