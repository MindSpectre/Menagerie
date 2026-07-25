# Crypto Library

The crypto library (`common/crypto/`) wraps OpenSSL for password/data hashing: an abstract hasher interface with
HMAC-SHA256 and PBKDF2-HMAC-SHA256 implementations, a cryptographically secure salt generator, and a couple of
small `menagerie::utilities` helpers (constant-time comparison, compile-time string concatenation) that ship
through the same umbrella. It depends on `menagerie::beavers` and links OpenSSL's `libssl` / `libcrypto`
privately. Everything is reached through `#include <menagerie/crypto>`
(`export/menagerie/crypto`).

## Key types

- **`HashInterface`** -- abstract base declaring
  `hash_function(data, salt)` and a `HashWithSalt{hash, salt}` return type; also provides
  `hash_with_generated_salt(password)`, which draws a 16-byte Base64 salt from `SaltGenerator` and hashes
  against it.
- **`SHA256Hash`** -- HMAC-SHA256 hasher; `set_key(...)` sets
  the HMAC key from any string-like type, `key()` reads it back, `hash_function(data, salt)` returns the hex
  digest.
- **`PBKDF2Hash`** -- PBKDF2-HMAC-SHA256 hasher;
  `hash_function(password, salt)` runs 100,000 iterations and returns a 256-bit hex digest.
- **`SaltGenerator`** -- static helpers backed by
  OpenSSL's `RAND_bytes`: `generate_bytes(size)`, `generate_hex(size)`, `generate_base64(size)`.
- **`menagerie::utilities::security::constant_time_compare(a, b)`**
  -- byte-by-byte string comparison that always walks the full length, avoiding early-exit timing leaks when
  checking a computed hash against a stored one.
- **`menagerie::utilities::compile_time::consteval_concat(...)`**
  -- variadic `consteval` concatenation of string literals into a fixed-size `std::array<char, N>`.

## Usage

```cpp
#include <menagerie/crypto>

using namespace menagerie::crypto;

SHA256Hash hasher;
hasher.set_key("server-side-hmac-key");

const std::string salt = SaltGenerator::generate_base64(16);
const std::string digest = hasher.hash_function("user-password", salt);

// later, verifying a login attempt against the stored (digest, salt) pair:
const std::string candidate_password = "user-password";
const bool ok = menagerie::utilities::security::constant_time_compare(
    digest, hasher.hash_function(candidate_password, salt));
```

## Design notes

`SHA256Hash` and `PBKDF2Hash` both write `class Derived final : HashInterface` with no `public`/`private` keyword
before the base name, which defaults to private inheritance for a `class` -- only the member each class
explicitly redeclares as `public` (`hash_function`) is reachable on the derived type from outside the class. `hash_with_generated_salt(...)`, defined once on
`HashInterface`, is therefore not callable through either concrete hasher; callers pair a hasher's
`hash_function` with `SaltGenerator` directly instead, as in the snippet above. The two `utilities/` headers live
under `common/crypto/` and ship through the crypto umbrella, but their symbols are declared in
`menagerie::utilities::security` and `menagerie::utilities::compile_time`, not `menagerie::crypto`.
