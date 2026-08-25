# baseConversion — Sequence Diagrams

> **Source files read**: `base16.h`, `base16.cpp`, `_base64.h`, `_base64.cpp`
> **Confidence**: 100% — all files read in full

---

## Module Overview

Provides low-level encoding/decoding utilities:
- **base16** (hex): `base16_Encode()`, `base16_Decode()`
- **base64**: `base64_Encode()`, `base64_Decode()`

Both modules are stateless, pure C-style functions using `malloc` for output buffers (caller must `free`).

---

## 1. Base16 Encode

```mermaid
sequenceDiagram
    participant Caller
    participant base16_Encode
    participant malloc

    Caller->>base16_Encode: src (binary data), len
    base16_Encode->>malloc: allocate (len*2 + 1) bytes
    malloc-->>base16_Encode: outData pointer
    loop For each byte in src
        base16_Encode->>base16_Encode: WRITE_HASCII(dst, byte)
    end
    base16_Encode->>base16_Encode: null-terminate
    base16_Encode-->>Caller: hex-encoded cstring (or NULL on OOM)
```

---

## 2. Base16 Decode

```mermaid
sequenceDiagram
    participant Caller
    participant base16_Decode
    participant malloc

    Caller->>base16_Decode: srcPtr (hex string), srcLen, &len
    base16_Decode->>base16_Decode: numChars = srcLen / 2
    base16_Decode->>malloc: allocate numChars bytes
    malloc-->>base16_Decode: outData pointer
    loop For each pair of hex chars
        base16_Decode->>base16_Decode: lookup mBase16CharToIndex[high] << 4
        base16_Decode->>base16_Decode: lookup mBase16CharToIndex[low] | combine
        base16_Decode->>base16_Decode: write byte to dst
    end
    base16_Decode->>base16_Decode: *len = numChars
    base16_Decode-->>Caller: binary data pointer (or NULL on OOM, *len=0)
```

---

## 3. Base64 Encode

```mermaid
sequenceDiagram
    participant Caller
    participant base64_Encode
    participant malloc

    Caller->>base64_Encode: src (binary data), len
    base64_Encode->>malloc: allocate ((len+2)/3)*4 + 1 bytes
    malloc-->>base64_Encode: rc pointer
    loop Process 3 bytes at a time
        base64_Encode->>base64_Encode: Pack 3 bytes into 24-bit temp
        base64_Encode->>base64_Encode: Extract 4x 6-bit indices
        base64_Encode->>base64_Encode: Map to base64 alphabet (A-Z,a-z,0-9,+,/)
        alt Padding needed (< 3 bytes remaining)
            base64_Encode->>base64_Encode: Emit '=' for missing bytes
        end
    end
    base64_Encode->>base64_Encode: null-terminate
    base64_Encode-->>Caller: base64-encoded cstring (or NULL on OOM)
```

---

## 4. Base64 Decode

```mermaid
sequenceDiagram
    participant Caller
    participant base64_Decode
    participant malloc

    Caller->>base64_Decode: src (base64 string), &outLen, srcLen
    base64_Decode->>malloc: allocate srcLen*3/4 bytes
    malloc-->>base64_Decode: rc pointer
    base64_Decode->>base64_Decode: Strip trailing '=' padding from srcLen
    loop Process 4 chars at a time
        base64_Decode->>base64_Decode: Lookup decode[char] for each (up to 4)
        alt Invalid character found (decode < 0)
            base64_Decode->>base64_Decode: free(rc)
            base64_Decode-->>Caller: NULL
        end
        base64_Decode->>base64_Decode: Reconstruct 1-3 bytes from 24-bit buffer
    end
    base64_Decode->>base64_Decode: *outLen = dst - rc
    base64_Decode-->>Caller: binary data pointer (or NULL)
```

---

## Dependencies

| Function | Depends On |
|----------|-----------|
| `base16_Encode` | `PlayerUtils.h` (WRITE_HASCII macro), `stdlib.h` |
| `base16_Decode` | `stdlib.h` |
| `base64_Encode` | `stdlib.h` |
| `base64_Decode` | `stdlib.h` |
