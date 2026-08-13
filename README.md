# C_Compressor

A simple compressor written entirely in C.

## Implementation Summary

This project reads an input file, processes it in 4-byte blocks, counts how often each block appears, and builds a compact `.zz` output format.

The implementation mainly does four things:

- reads the source file into memory
- groups and counts 4-byte blocks
- separates repeated blocks from non-repeated blocks
- writes a compressed output containing a small header, block tables, and encoded 16-bit tokens

`zip.c` contains the full compression flow, including file handling, block analysis, sorting/filtering, and final encoding.

`zipzip.h` defines the shared types, macros, structures, and function declarations used by the compressor.

`MakeFile` provides a minimal build setup to compile `zip.c` into the `zz` executable.

## Current Design Characteristics

### Strengths

- Entire implementation is in C with direct control over memory and file layout.
- The format is simple enough to trace and extend.
- Repeated adjacent 4-byte values can benefit from small run-length packing.
- The dictionary split between frequent and non-frequent blocks makes the encoding logic easy to follow.

### Current limitations

- No decompressor exists yet.
- Compression works on 4-byte aligned chunks only; leftover bytes are not modeled explicitly.
- Block lookup uses linear search, which becomes expensive as dictionaries grow.
- `write_file()` writes one byte at a time, which is slow.
- `MAX_FILE_SIZE` is very small for real-world use.
- The format depends on raw integer interpretation, so endianness and cross-platform decoding need care.
- The repeated-block index must fit within 12 bits because the upper 4 bits of the token store run length.
- There is little separation between library code, test code, and the main executable path.

## Future Scope

- Implement a full decompressor for the `.zz` format.
- Store and reconstruct trailing bytes when input size is not divisible by 4.
- Replace linear dictionary searches with hash tables or sorted lookups.
- Write output in larger buffered chunks instead of byte-by-byte.
- Increase supported file sizes and validate all integer-overflow boundaries.
- Document the binary format formally so other tools can read and write it.
- Add portability handling for endianness and compiler/platform differences.
- Split the code into modules such as `io`, `format`, `encoder`, and `tests`.
- Add unit tests for block counting, sorting, filtering, and token encoding.
- Rename `MakeFile` to `Makefile` for smoother tool compatibility.
- Add CLI options such as custom output path, stats, verbose mode, and decompression mode.
- Improve compression effectiveness by using smarter dictionary selection and better token packing.
