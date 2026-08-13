# C_Compressor

A C-based file compressor that scans input in 4-byte blocks, counts repeated values, builds two lookup tables, and emits a compact `.zz` file with a custom header plus 16-bit encoded tokens.

## What This Project Does

The compressor reads a file, breaks it into 32-bit chunks, counts how often each chunk appears, and then splits the discovered values into:

- repeated blocks: values that appear more than once
- remaining blocks: values that appear once

During encoding, every 4-byte input block is converted into a 16-bit value:

- repeated blocks are stored as a 12-bit dictionary index plus a 4-bit repeat count
- non-repeated blocks are stored as an index into the unique-value list

This means the current implementation is a custom block encoder focused on files where 4-byte patterns repeat often. It is a compressor only; decompression is not implemented yet.

## File Overview

### `zip.c`

This file contains the full program logic: file I/O, block counting, sorting, filtering, encoding, debug helpers, and the main compression flow.

#### Global state

- `int32 FILESZ;`
  Stores the size of the input file after `read_file()` succeeds. It is later used by `main3()` and `mk_header()`.

#### Memory and raw byte helpers

- `int8* empty(int32 filesz)`
  Allocates a large output buffer for the encoded result. The buffer size includes space for the header, both block tables, the encoded payload, and a small safety margin.

- `void zero(int8 *dst, int32 size)`
  Fills a memory region with zero bytes. This is used after allocations and before building output buffers.

- `void copy_bytes(int8* dst, int8* src, int32 size)`
  Copies `size` bytes one-by-one from `src` to `dst`. This is the project’s manual byte-copy helper.

#### File I/O

- `int8* read_file(int8* filename)`
  Opens the input file, checks its size with `fstat`, rejects files larger than `MAX_FILE_SIZE`, allocates a buffer, reads the file in 512-byte chunks, zero-pads the extra 4 bytes, stores the final size in `FILESZ`, and returns the in-memory contents.

- `bool write_file(int8* filename, int8* memspace, int32 size)`
  Creates the output file and writes the encoded buffer one byte at a time.

#### Header and metadata construction

- `header* mk_header(int32 filesz, int16 blocklist, int16 uniqlist)`
  Allocates and fills the archive header. It writes the `ZZ` magic, version number, original file size, repeated-block count, and unique-block count.

- `int32 copy_headers(int8* memspace, header* hdr, blocklist* repeated, blocklist* remaining)`
  Serializes the header followed by the repeated-block dictionary and then the non-repeated block dictionary into the output buffer. It returns the byte offset where encoded payload data should start.

#### Block counting and lookup

- `amtlist* make_amtlist(void)`
  Creates the dynamic frequency list used to store `{block, count}` pairs.

- `bool increase(amtlist **list)`
  Grows the capacity of an `amtlist` by `BlockSize` entries using `realloc`.

- `bool add_amt(amtlist **list, amtentry entry)`
  Appends a new `{block, amt}` record to the list, growing the list when needed.

- `amtentry *amtsearch(amtlist* haystack, int32 needle)`
  Performs a linear search for a block value in the frequency list and returns a pointer to the matching entry if found.

- `amtlist* read_blocks(int8* contents, int32 size_)`
  Walks through the file buffer 4 bytes at a time, interprets each chunk as an `int32`, and builds a frequency table of all discovered block values.

- `int16 block_search(blocklist* haystack, int32 needle)`
  Performs a linear search inside a serialized block dictionary and returns the block index or `NoMatch`.

#### Sorting and list utilities

- `void amtswap(amtentry* e1, amtentry* e2)`
  Swaps two frequency entries.

- `sortres amtsort(amtentry e1, amtentry e2)`
  Comparison function used for sorting. It orders entries so that higher frequencies come first.

- `amtlist* amtlistcopy(amtlist* list)`
  Creates a deep copy of an `amtlist`.

- `bool amtlisteq(amtlist* l1, amtlist* l2)`
  Compares two frequency lists entry-by-entry.

- `amtlist* zipsort(amtlist* old, sortfunc func)`
  Wrapper around `zipsort_()` that validates arguments and returns a sorted copy.

- `amtlist* zipsort_(amtlist* list_, sortfunc func)`
  Implements a bubble sort over a copied `amtlist`, using the provided comparator.

#### Filtering helpers

These functions are used with `filter_blocks()` to produce specialized block dictionaries.

- `bool head(int16 idx, amtentry* e, void* arg1, void* arg2)`
  Keeps entries whose index is less than or equal to the supplied max value.

- `bool tail(int16 idx, amtentry* e, void* arg1, void* arg2)`
  Keeps entries whose index is greater than or equal to the supplied min value.

- `bool repeated(int16 idx, amtentry* e, void* arg1, void* arg2)`
  Keeps blocks that appear more than once.

- `bool nonrepeated(int16 idx, amtentry* e, void* arg1, void* arg2)`
  Keeps blocks that appear once or not more than once.

- `blocklist* filter_blocks(amtlist* list, filterfunc func, void* arg1, void* arg2)`
  Builds a compact `blocklist` from an `amtlist` by applying a predicate function to each entry.

#### Encoding logic

- `int32 parse_file(int8* dst, int8* src, header* hdr, blocklist* repeated, blocklist* remaining)`
  Encodes the source file into the destination buffer.

  Encoding behavior:

- If a 4-byte block is in the repeated dictionary, the encoder writes a 16-bit token:
  `bits 0-11 = dictionary index`
  `bits 12-15 = run length of repeated consecutive occurrences`
- The repeat count is capped at 15 because only 4 bits are reserved.
- If a block is not in the repeated dictionary, the encoder looks it up in the remaining dictionary and writes that index as a 16-bit token.
- The function returns the number of encoded output bytes written.

This is the core compression routine.

#### Debug and developer helpers

- `void showamtlist(const char *identifier, amtlist *list)`
  Prints summary information for a frequency list.

- `void show_blocklist(const char *identifier, blocklist *list)`
  Prints summary information for a block dictionary.

- `void showamtentry(const char *identifier, amtentry e)`
  Prints one `{block, count}` entry.

#### Entry points

- `int main1(void)`
  A test/demo routine for populating, mutating, and sorting an `amtlist`.

- `int main2(void)`
  A test/demo routine for searching within an `amtlist`.

- `int main3(int8* file, int8* out_file)`
  The real compression pipeline:

1. reads the source file
2. builds the frequency table
3. sorts blocks by frequency
4. splits them into repeated and non-repeated lists
5. creates the output header
6. allocates output memory
7. serializes the header and block tables
8. encodes the payload
9. shrinks the final output buffer
10. writes the `.zz` file

- `int main(int argc, char **argv)`
  Validates the command-line argument, creates the output filename by appending `.zz`, and invokes `main3()`.

### `zipzip.h`

This header defines the full public interface and most of the project’s type system.

#### Basic typedefs

- `int8`, `int16`, `int32`, `int64`
  Custom unsigned integer aliases used throughout the project.

#### Core data structures

- `struct s_amtentry`
  Stores one 4-byte block and how many times it occurs.

- `struct s_amtlist`
  Dynamic array of `amtentry` records. It tracks `capacity`, `length`, and the flexible array `data[]`.

- `struct s_blocklist`
  Compact dictionary of raw block values chosen after filtering. It stores `length` plus a flexible array `blocks[]`.

- `struct s_header`
  File header written to the compressed output. It contains:
  `magic[3]`, `version`, `filesz`, `blocklist`, and `uniqlist`.

#### Enums and callbacks

- `enum e_sortres`
  Represents comparison results: `MoreThan`, `Equal`, `LessThan`.

- `filterfunc`
  Callback type used by `filter_blocks()`.

- `sortfunc`
  Callback type used by the sorting functions.

#### Important macros

- `NoMatch`
  Sentinel value returned by `block_search()` when a block is not found.

- `Version`
  Output format version, currently `1`.

- `BlockSize`
  Initial and incremental growth unit for `amtlist`, set to `0xffff`.

- `MaxCap`
  Maximum capacity allowed for the dynamic frequency list.

- `MAX_FILE_SIZE`
  Upper limit for input file size. Currently `33556` bytes.

- Cast helper macros: `$c`, `$i`, `$v`, `$1`, `$2`, `$4`, `$8`
  Short aliases used to reduce explicit casts in the source.

- Allocation helpers: `alloc`, `destroy`, `ralloc`
  Thin wrappers over `malloc`, `free`, and `realloc`.

- Generic helper macros: `show`, `swap`, `copy`, `listeq`
  Use C11/C2x `_Generic` dispatch to call the correct helper based on argument type.

#### Function declarations

The header also declares all constructors, utility functions, encoding helpers, debug helpers, and program entry points used by `zip.c`.

### `MakeFile`

The build file is minimal and straightforward.

- `flags := -Wall -I. -std=c2x`
  Enables warnings, includes the current directory, and compiles with the C2x standard.

- `all: zz`
  Default target that builds the executable `zz`.

- `zz: zip.o`
  Links the final executable from `zip.o`.

- `zip.o: zip.c`
  Compiles the source file into an object file.

- `clean`
  Removes the executable and object files.

One thing to note is that the file is named `MakeFile`, while many environments expect `Makefile` or `makefile` by default.

## Compression Format Summary

The generated `.zz` file is laid out like this:

1. `struct s_header`
2. repeated block dictionary as raw `int32` values
3. non-repeated block dictionary as raw `int32` values
4. encoded payload as 16-bit tokens

The design assumes that every 4-byte chunk from the input can be represented through one of the two block dictionaries.

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
