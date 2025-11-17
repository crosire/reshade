// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "reshade_api.hpp"

#if defined(_MSC_VER)
using ssize_t = intptr_t;
#endif

using rs_colorspace = reshade::api::color_space;

// Simplified version of the streaming input source from jxl/encode.h
// We only need this part to wrap the full image buffer in the standalone mode
// and this way we don't need to depend on the jxl headers.
struct JxlChunkedFrameInputSource {
  void *opaque;
  const void *(*get_color_channel_data_at)(void *opaque, size_t xpos,
                                           size_t ypos, size_t xsize,
                                           size_t ysize, size_t *row_offset);
  void (*release_buffer)(void *opaque, const void *buf);
};
// A FJxlParallelRunner must call fun(opaque, i) for all i from 0 to count. It
// may do so in parallel.
typedef void(FJxlParallelRunner)(void *runner_opaque, void *opaque,
                                 void fun(void *, size_t), size_t count);
struct JxlSimpleLosslessFrameState;

// Returned JxlSimpleLosslessFrameState must be freed by calling
// JxlSimpleLosslessFreeFrameState.
JxlSimpleLosslessFrameState *
JxlSimpleLosslessPrepareFrame(JxlChunkedFrameInputSource input, size_t width,
                              size_t height, size_t nb_chans, size_t bitdepth,
                              bool big_endian, int effort, rs_colorspace color_space, int oneshot);

bool JxlSimpleLosslessProcessFrame(JxlSimpleLosslessFrameState *frame_state,
                                   bool is_last, void *runner_opaque,
                                   FJxlParallelRunner runner);

// Prepare the (image/frame) header. You may encode animations by concatenating
// the output of multiple frames, of which the first one has add_image_header =
// 1 and subsequent ones have add_image_header = 0, and all frames but the last
// one have is_last = 0.
// (when FJXL_STANDALONE=0, add_image_header has to be 0)
void JxlSimpleLosslessPrepareHeader(JxlSimpleLosslessFrameState *frame,
                                    int add_image_header, int is_last);

// Upper bound on the required output size, including any padding that may be
// required by JxlSimpleLosslessWriteOutput. Cannot be called before
// JxlSimpleLosslessPrepareHeader.
size_t
JxlSimpleLosslessMaxRequiredOutput(const JxlSimpleLosslessFrameState *frame);

// Actual size of the frame once it is encoded. This is not identical to
// JxlSimpleLosslessMaxRequiredOutput because JxlSimpleLosslessWriteOutput may
// require extra padding.
size_t JxlSimpleLosslessOutputSize(const JxlSimpleLosslessFrameState *frame);

// Writes the frame to the given output buffer. Returns the number of bytes that
// were written, which is at least 1 unless the entire output has been written
// already. It is required that `output_size >= 32` when calling this function.
// This function must be called repeatedly until it returns 0.
size_t JxlSimpleLosslessWriteOutput(JxlSimpleLosslessFrameState *frame,
                                    unsigned char *output, size_t output_size);

// Frees the provided frame state.
void JxlSimpleLosslessFreeFrameState(JxlSimpleLosslessFrameState *frame);

#if defined(_MSC_VER) && !defined(__clang__)
#define FJXL_INLINE __forceinline
FJXL_INLINE uint32_t FloorLog2(uint32_t v) {
  unsigned long index;
  _BitScanReverse(&index, v);
  return index;
}
FJXL_INLINE uint32_t CtzNonZero(uint64_t v) {
  unsigned long index;
  _BitScanForward(&index, v);
  return index;
}
#else
#define FJXL_INLINE inline __attribute__((always_inline))
FJXL_INLINE uint32_t FloorLog2(uint32_t v) {
  return v ? 31 - __builtin_clz(v) : 0;
}
FJXL_INLINE uint32_t CtzNonZero(uint64_t v) { return __builtin_ctzll(v); }
#endif

// Compiles to a memcpy on little-endian systems.
FJXL_INLINE void StoreLE64(uint8_t *tgt, uint64_t data) {
#if (!defined(__BYTE_ORDER__) || (__BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__))
  for (int i = 0; i < 8; i++) {
    tgt[i] = (data >> (i * 8)) & 0xFF;
  }
#else
  memcpy(tgt, &data, 8);
#endif
}

FJXL_INLINE size_t AddBits(uint32_t count, uint64_t bits, uint8_t *data_buf,
                           size_t &bits_in_buffer, uint64_t &bit_buffer) {
  bit_buffer |= bits << bits_in_buffer;
  bits_in_buffer += count;
  StoreLE64(data_buf, bit_buffer);
  size_t bytes_in_buffer = bits_in_buffer / 8;
  bits_in_buffer -= bytes_in_buffer * 8;
  bit_buffer >>= bytes_in_buffer * 8;
  return bytes_in_buffer;
}

struct BitWriter {
  void Allocate(size_t maximum_bit_size) {
    assert(data == nullptr);
    // Leave some padding.
    data.reset(static_cast<uint8_t *>(malloc(maximum_bit_size / 8 + 64)));
  }

  void Write(uint32_t count, uint64_t bits) {
    bytes_written += AddBits(count, bits, data.get() + bytes_written,
                             bits_in_buffer, buffer);
  }

  void ZeroPadToByte() {
    if (bits_in_buffer != 0) {
      Write(8 - bits_in_buffer, 0);
    }
  }

  FJXL_INLINE void WriteMultiple(const uint64_t *nbits, const uint64_t *bits,
                                 size_t n) {
    // Necessary because Write() is only guaranteed to work with <=56 bits.
    // Trying to SIMD-fy this code results in lower speed (and definitely less
    // clarity).
    {
      for (size_t i = 0; i < n; i++) {
        this->buffer |= bits[i] << this->bits_in_buffer;
        memcpy(this->data.get() + this->bytes_written, &this->buffer, 8);
        uint64_t shift = 64 - this->bits_in_buffer;
        this->bits_in_buffer += nbits[i];
        // This `if` seems to be faster than using ternaries.
        if (this->bits_in_buffer >= 64) {
          uint64_t next_buffer = shift >= 64 ? 0 : bits[i] >> shift;
          this->buffer = next_buffer;
          this->bits_in_buffer -= 64;
          this->bytes_written += 8;
        }
      }
      memcpy(this->data.get() + this->bytes_written, &this->buffer, 8);
      size_t bytes_in_buffer = this->bits_in_buffer / 8;
      this->bits_in_buffer -= bytes_in_buffer * 8;
      this->buffer >>= bytes_in_buffer * 8;
      this->bytes_written += bytes_in_buffer;
    }
  }

  std::unique_ptr<uint8_t[], void (*)(void *)> data = {nullptr, free};
  size_t bytes_written = 0;
  size_t bits_in_buffer = 0;
  uint64_t buffer = 0;
};

size_t SectionSize(const std::array<BitWriter, 4> &group_data) {
  size_t sz = 0;
  for (size_t j = 0; j < 4; j++) {
    const auto &writer = group_data[j];
    sz += writer.bytes_written * 8 + writer.bits_in_buffer;
  }
  sz = (sz + 7) / 8;
  return sz;
}

constexpr size_t kMaxFrameHeaderSize = 5;

constexpr size_t kGroupSizeOffset[4] = {
    static_cast<size_t>(0),
    static_cast<size_t>(1024),
    static_cast<size_t>(17408),
    static_cast<size_t>(4211712),
};
constexpr size_t kTOCBits[4] = {12, 16, 24, 32};

size_t TOCBucket(size_t group_size) {
  size_t bucket = 0;
  while (bucket < 3 && group_size >= kGroupSizeOffset[bucket + 1])
    ++bucket;
  return bucket;
}

void ComputeAcGroupDataOffset(size_t dc_global_size, size_t num_dc_groups,
                              size_t num_ac_groups, size_t &min_dc_global_size,
                              size_t &ac_group_offset) {
  // Max AC group size is 768 kB, so max AC group TOC bits is 24.
  size_t ac_toc_max_bits = num_ac_groups * 24;
  size_t ac_toc_min_bits = num_ac_groups * 12;
  size_t max_padding = 1 + (ac_toc_max_bits - ac_toc_min_bits + 7) / 8;
  min_dc_global_size = dc_global_size;
  size_t dc_global_bucket = TOCBucket(min_dc_global_size);
  while (TOCBucket(min_dc_global_size + max_padding) > dc_global_bucket) {
    dc_global_bucket = TOCBucket(min_dc_global_size + max_padding);
    min_dc_global_size = kGroupSizeOffset[dc_global_bucket];
  }
  assert(TOCBucket(min_dc_global_size) == dc_global_bucket);
  assert(TOCBucket(min_dc_global_size + max_padding) == dc_global_bucket);
  size_t max_toc_bits =
      kTOCBits[dc_global_bucket] + 12 * (1 + num_dc_groups) + ac_toc_max_bits;
  size_t max_toc_size = (max_toc_bits + 7) / 8;
  ac_group_offset = kMaxFrameHeaderSize + max_toc_size + min_dc_global_size;
}

constexpr size_t kNumRawSymbols = 19;
constexpr size_t kNumLZ77 = 33;
constexpr size_t kLZ77CacheSize = 32;

constexpr size_t kLZ77Offset = 224;
constexpr size_t kLZ77MinLength = 7;

void EncodeHybridUintLZ77(uint32_t value, uint32_t *token, uint32_t *nbits,
                          uint32_t *bits) {
  // 400 config
  uint32_t n = FloorLog2(value);
  *token = value < 16 ? value : 16 + n - 4;
  *nbits = value < 16 ? 0 : n;
  *bits = value < 16 ? 0 : value - (1 << *nbits);
}

struct PrefixCode {
  uint8_t raw_nbits[kNumRawSymbols] = {};
  uint8_t raw_bits[kNumRawSymbols] = {};

  uint8_t lz77_nbits[kNumLZ77] = {};
  uint16_t lz77_bits[kNumLZ77] = {};

  uint64_t lz77_cache_bits[kLZ77CacheSize] = {};
  uint8_t lz77_cache_nbits[kLZ77CacheSize] = {};

  size_t numraw;

  static uint16_t BitReverse(size_t nbits, uint16_t bits) {
    constexpr uint16_t kNibbleLookup[16] = {
        0b0000, 0b1000, 0b0100, 0b1100, 0b0010, 0b1010, 0b0110, 0b1110,
        0b0001, 0b1001, 0b0101, 0b1101, 0b0011, 0b1011, 0b0111, 0b1111,
    };
    uint16_t rev16 = (kNibbleLookup[bits & 0xF] << 12) |
                     (kNibbleLookup[(bits >> 4) & 0xF] << 8) |
                     (kNibbleLookup[(bits >> 8) & 0xF] << 4) |
                     (kNibbleLookup[bits >> 12]);
    return rev16 >> (16 - nbits);
  }

  // Create the prefix codes given the code lengths.
  // Supports the code lengths being split into two halves.
  static void ComputeCanonicalCode(const uint8_t *first_chunk_nbits,
                                   uint8_t *first_chunk_bits,
                                   size_t first_chunk_size,
                                   const uint8_t *second_chunk_nbits,
                                   uint16_t *second_chunk_bits,
                                   size_t second_chunk_size) {
    constexpr size_t kMaxCodeLength = 15;
    uint8_t code_length_counts[kMaxCodeLength + 1] = {};
    for (size_t i = 0; i < first_chunk_size; i++) {
      code_length_counts[first_chunk_nbits[i]]++;
      assert(first_chunk_nbits[i] <= kMaxCodeLength);
      assert(first_chunk_nbits[i] <= 8);
      assert(first_chunk_nbits[i] > 0);
    }
    for (size_t i = 0; i < second_chunk_size; i++) {
      code_length_counts[second_chunk_nbits[i]]++;
      assert(second_chunk_nbits[i] <= kMaxCodeLength);
    }

    uint16_t next_code[kMaxCodeLength + 1] = {};

    uint16_t code = 0;
    for (size_t i = 1; i < kMaxCodeLength + 1; i++) {
      code = (code + code_length_counts[i - 1]) << 1;
      next_code[i] = code;
    }

    for (size_t i = 0; i < first_chunk_size; i++) {
      first_chunk_bits[i] =
          BitReverse(first_chunk_nbits[i], next_code[first_chunk_nbits[i]]++);
    }
    for (size_t i = 0; i < second_chunk_size; i++) {
      second_chunk_bits[i] =
          BitReverse(second_chunk_nbits[i], next_code[second_chunk_nbits[i]]++);
    }
  }

  template <typename T>
  static void ComputeCodeLengthsNonZeroImpl(const uint64_t *freqs, size_t n,
                                            size_t precision, T infty,
                                            const uint8_t *min_limit,
                                            const uint8_t *max_limit,
                                            uint8_t *nbits) {
    assert(precision < 15);
    assert(n <= kMaxNumSymbols);
    std::vector<T> dynp(((1U << precision) + 1) * (n + 1), infty);
    auto d = [&](size_t sym, size_t off) -> T & {
      return dynp[sym * ((1 << precision) + 1) + off];
    };
    d(0, 0) = 0;
    for (size_t sym = 0; sym < n; sym++) {
      for (T bits = min_limit[sym]; bits <= max_limit[sym]; bits++) {
        size_t off_delta = 1U << (precision - bits);
        for (size_t off = 0; off + off_delta <= (1U << precision); off++) {
          d(sym + 1, off + off_delta) =
              std::min(d(sym, off) + static_cast<T>(freqs[sym]) * bits,
                       d(sym + 1, off + off_delta));
        }
      }
    }

    size_t sym = n;
    size_t off = 1U << precision;

    assert(d(sym, off) != infty);

    while (sym-- > 0) {
      assert(off > 0);
      for (size_t bits = min_limit[sym]; bits <= max_limit[sym]; bits++) {
        size_t off_delta = 1U << (precision - bits);
        if (off_delta <= off &&
            d(sym + 1, off) == d(sym, off - off_delta) + freqs[sym] * bits) {
          off -= off_delta;
          nbits[sym] = bits;
          break;
        }
      }
    }
  }

  // Computes nbits[i] for i <= n, subject to min_limit[i] <= nbits[i] <=
  // max_limit[i] and sum 2**-nbits[i] == 1, so to minimize sum(nbits[i] *
  // freqs[i]).
  static void ComputeCodeLengthsNonZero(const uint64_t *freqs, size_t n,
                                        uint8_t *min_limit, uint8_t *max_limit,
                                        uint8_t *nbits) {
    size_t precision = 0;
    size_t shortest_length = 255;
    uint64_t freqsum = 0;
    for (size_t i = 0; i < n; i++) {
      assert(freqs[i] != 0);
      freqsum += freqs[i];
      if (min_limit[i] < 1)
        min_limit[i] = 1;
      assert(min_limit[i] <= max_limit[i]);
      precision = std::max<size_t>(max_limit[i], precision);
      shortest_length = std::min<size_t>(min_limit[i], shortest_length);
    }
    // If all the minimum limits are greater than 1, shift precision so that we
    // behave as if the shortest was 1.
    precision -= shortest_length - 1;
    uint64_t infty = freqsum * precision;
    if (infty < std::numeric_limits<uint32_t>::max() / 2) {
      ComputeCodeLengthsNonZeroImpl(freqs, n, precision,
                                    static_cast<uint32_t>(infty), min_limit,
                                    max_limit, nbits);
    } else {
      ComputeCodeLengthsNonZeroImpl(freqs, n, precision, infty, min_limit,
                                    max_limit, nbits);
    }
  }

  static constexpr size_t kMaxNumSymbols =
      kNumRawSymbols + 1 < kNumLZ77 ? kNumLZ77 : kNumRawSymbols + 1;
  static void ComputeCodeLengths(const uint64_t *freqs, size_t n,
                                 const uint8_t *min_limit_in,
                                 const uint8_t *max_limit_in, uint8_t *nbits) {
    assert(n <= kMaxNumSymbols);
    uint64_t compact_freqs[kMaxNumSymbols];
    uint8_t min_limit[kMaxNumSymbols];
    uint8_t max_limit[kMaxNumSymbols];
    size_t ni = 0;
    for (size_t i = 0; i < n; i++) {
      if (freqs[i]) {
        compact_freqs[ni] = freqs[i];
        min_limit[ni] = min_limit_in[i];
        max_limit[ni] = max_limit_in[i];
        ni++;
      }
    }
    for (size_t i = ni; i < kMaxNumSymbols; ++i) {
      compact_freqs[i] = 0;
      min_limit[i] = 0;
      max_limit[i] = 0;
    }
    uint8_t num_bits[kMaxNumSymbols] = {};
    ComputeCodeLengthsNonZero(compact_freqs, ni, min_limit, max_limit,
                              num_bits);
    ni = 0;
    for (size_t i = 0; i < n; i++) {
      nbits[i] = 0;
      if (freqs[i]) {
        nbits[i] = num_bits[ni++];
      }
    }
  }

  // Invalid code, used to construct arrays.
  PrefixCode() = default;

  template <typename BitDepth>
  PrefixCode(BitDepth /* bitdepth */, uint64_t *raw_counts,
             uint64_t *lz77_counts) {
    // "merge" together all the lz77 counts in a single symbol for the level 1
    // table (containing just the raw symbols, up to length 7).
    uint64_t level1_counts[kNumRawSymbols + 1];
    memcpy(level1_counts, raw_counts, kNumRawSymbols * sizeof(uint64_t));
    numraw = kNumRawSymbols;
    while (numraw > 0 && level1_counts[numraw - 1] == 0)
      numraw--;

    level1_counts[numraw] = 0;
    for (size_t i = 0; i < kNumLZ77; i++) {
      level1_counts[numraw] += lz77_counts[i];
    }
    uint8_t level1_nbits[kNumRawSymbols + 1] = {};
    ComputeCodeLengths(level1_counts, numraw + 1, BitDepth::kMinRawLength,
                       BitDepth::kMaxRawLength, level1_nbits);

    uint8_t level2_nbits[kNumLZ77] = {};
    uint8_t min_lengths[kNumLZ77] = {};
    uint8_t l = 15 - level1_nbits[numraw];
    uint8_t max_lengths[kNumLZ77];
    for (uint8_t &max_length : max_lengths) {
      max_length = l;
    }
    size_t num_lz77 = kNumLZ77;
    while (num_lz77 > 0 && lz77_counts[num_lz77 - 1] == 0)
      num_lz77--;
    ComputeCodeLengths(lz77_counts, num_lz77, min_lengths, max_lengths,
                       level2_nbits);
    for (size_t i = 0; i < numraw; i++) {
      raw_nbits[i] = level1_nbits[i];
    }
    for (size_t i = 0; i < num_lz77; i++) {
      lz77_nbits[i] =
          level2_nbits[i] ? level1_nbits[numraw] + level2_nbits[i] : 0;
    }

    ComputeCanonicalCode(raw_nbits, raw_bits, numraw, lz77_nbits, lz77_bits,
                         kNumLZ77);

    // Prepare lz77 cache
    for (size_t count = 0; count < kLZ77CacheSize; count++) {
      unsigned token, nbits, bits;
      EncodeHybridUintLZ77(count, &token, &nbits, &bits);
      lz77_cache_nbits[count] = lz77_nbits[token] + nbits + raw_nbits[0];
      lz77_cache_bits[count] =
          (((bits << lz77_nbits[token]) | lz77_bits[token]) << raw_nbits[0]) |
          raw_bits[0];
    }
  }

  // Max bits written: 2 + 72 + 95 + 24 + 165 = 286
  void WriteTo(BitWriter *writer) const {
    uint64_t code_length_counts[18] = {};
    code_length_counts[17] = 3 + 2 * (kNumLZ77 - 1);
    for (uint8_t raw_nbit : raw_nbits) {
      code_length_counts[raw_nbit]++;
    }
    for (uint8_t lz77_nbit : lz77_nbits) {
      code_length_counts[lz77_nbit]++;
    }
    uint8_t code_length_nbits[18] = {};
    uint8_t code_length_nbits_min[18] = {};
    uint8_t code_length_nbits_max[18] = {
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    };
    ComputeCodeLengths(code_length_counts, 18, code_length_nbits_min,
                       code_length_nbits_max, code_length_nbits);
    writer->Write(2, 0b00); // HSKIP = 0, i.e. don't skip code lengths.

    // As per Brotli RFC.
    uint8_t code_length_order[18] = {1, 2, 3, 4,  0,  5,  17, 6,  16,
                                     7, 8, 9, 10, 11, 12, 13, 14, 15};
    uint8_t code_length_length_nbits[] = {2, 4, 3, 2, 2, 4};
    uint8_t code_length_length_bits[] = {0, 7, 3, 2, 1, 15};

    // Encode lengths of code lengths.
    size_t num_code_lengths = 18;
    while (code_length_nbits[code_length_order[num_code_lengths - 1]] == 0) {
      num_code_lengths--;
    }
    // Max bits written in this loop: 18 * 4 = 72
    for (size_t i = 0; i < num_code_lengths; i++) {
      int symbol = code_length_nbits[code_length_order[i]];
      writer->Write(code_length_length_nbits[symbol],
                    code_length_length_bits[symbol]);
    }

    // Compute the canonical codes for the codes that represent the lengths of
    // the actual codes for data.
    uint16_t code_length_bits[18] = {};
    ComputeCanonicalCode(nullptr, nullptr, 0, code_length_nbits,
                         code_length_bits, 18);
    // Encode raw bit code lengths.
    // Max bits written in this loop: 19 * 5 = 95
    for (uint8_t raw_nbit : raw_nbits) {
      writer->Write(code_length_nbits[raw_nbit], code_length_bits[raw_nbit]);
    }
    size_t num_lz77 = kNumLZ77;
    while (lz77_nbits[num_lz77 - 1] == 0) {
      num_lz77--;
    }
    // Encode 0s until 224 (start of LZ77 symbols). This is in total 224-19 =
    // 205.
    static_assert(kLZ77Offset == 224, "kLZ77Offset should be 224");
    static_assert(kNumRawSymbols == 19, "kNumRawSymbols should be 19");
    {
      // Max bits in this block: 24
      writer->Write(code_length_nbits[17], code_length_bits[17]);
      writer->Write(3, 0b010); // 5
      writer->Write(code_length_nbits[17], code_length_bits[17]);
      writer->Write(3, 0b000); // (5-2)*8 + 3 = 27
      writer->Write(code_length_nbits[17], code_length_bits[17]);
      writer->Write(3, 0b010); // (27-2)*8 + 5 = 205
    }
    // Encode LZ77 symbols, with values 224+i.
    // Max bits written in this loop: 33 * 5 = 165
    for (size_t i = 0; i < num_lz77; i++) {
      writer->Write(code_length_nbits[lz77_nbits[i]],
                    code_length_bits[lz77_nbits[i]]);
    }
  }
};

struct JxlSimpleLosslessFrameState {
  JxlChunkedFrameInputSource input;
  size_t width;
  size_t height;
  size_t num_groups_x;
  size_t num_groups_y;
  size_t num_dc_groups_x;
  size_t num_dc_groups_y;
  size_t nb_chans;
  size_t bitdepth;
  int big_endian;
  int effort;
  bool collided;
  PrefixCode hcode[4];
  std::vector<int16_t> lookup;
  BitWriter header;
  std::vector<std::array<BitWriter, 4>> group_data;
  std::vector<size_t> group_sizes;
  size_t ac_group_data_offset = 0;
  size_t min_dc_global_size = 0;
  size_t current_bit_writer = 0;
  size_t bit_writer_byte_pos = 0;
  size_t bits_in_buffer = 0;
  uint64_t bit_buffer = 0;
  bool process_done = false;
  rs_colorspace color_space = rs_colorspace::srgb_nonlinear;
};

size_t JxlSimpleLosslessOutputSize(const JxlSimpleLosslessFrameState *frame) {
  size_t total_size_groups = 0;
  for (const auto &section : frame->group_data) {
    total_size_groups += SectionSize(section);
  }
  return frame->header.bytes_written + total_size_groups;
}

size_t
JxlSimpleLosslessMaxRequiredOutput(const JxlSimpleLosslessFrameState *frame) {
  return JxlSimpleLosslessOutputSize(frame) + 32;
}

void JxlSimpleLosslessPrepareHeader(JxlSimpleLosslessFrameState *frame,
                                    int add_image_header, int is_last) {
  BitWriter *output = &frame->header;
  output->Allocate(1000 + frame->group_sizes.size() * 32);

  bool have_alpha = (frame->nb_chans == 2 || frame->nb_chans == 4);

  if (add_image_header) {
    // Signature
    output->Write(16, 0x0AFF);

    // Size header, hand-crafted.
    // Not small
    output->Write(1, 0);

    auto wsz = [output](size_t size) {
      if (size - 1 < (1 << 9)) {
        output->Write(2, 0b00);
        output->Write(9, size - 1);
      } else if (size - 1 < (1 << 13)) {
        output->Write(2, 0b01);
        output->Write(13, size - 1);
      } else if (size - 1 < (1 << 18)) {
        output->Write(2, 0b10);
        output->Write(18, size - 1);
      } else {
        output->Write(2, 0b11);
        output->Write(30, size - 1);
      }
    };

    wsz(frame->height);

    // No special ratio.
    output->Write(3, 0);

    wsz(frame->width);

    // Hand-crafted ImageMetadata.
    output->Write(1, 0); // all_default
    output->Write(1, 0); // extra_fields
	if (frame->color_space == rs_colorspace::extended_srgb_linear) {
	  output->Write(1, 1); // bit_depth.floating_point_sample
	  output->Write(2, 0b01); // bit_depth.bits_per_sample = 16
	  output->Write(4, 4); // bit_depth.exp_bits = 5
	} else {
	  output->Write(1, 0); // !bit_depth.floating_point_sample
	  if (frame->bitdepth == 8) {
		output->Write(2, 0b00); // bit_depth.bits_per_sample = 8
      } else if (frame->bitdepth == 10) {
        output->Write(2, 0b01); // bit_depth.bits_per_sample = 10
      } else if (frame->bitdepth == 12) {
        output->Write(2, 0b10); // bit_depth.bits_per_sample = 12
      } else {
        output->Write(2, 0b11); // 1 + u(6)
        output->Write(6, frame->bitdepth - 1);
      }
	}
    if (frame->bitdepth <= 14) {
      output->Write(1, 1); // 16-bit-buffer sufficient
    } else {
      output->Write(1, 0); // 16-bit-buffer NOT sufficient
    }
    if (have_alpha) {
      output->Write(2, 0b01); // One extra channel
      if (frame->bitdepth == 8) {
        output->Write(1, 1); // ... all_default (ie. 8-bit alpha)
      } else {
        output->Write(1, 0); // not d_alpha
        output->Write(2, 0); // type = kAlpha
		if (frame->color_space == rs_colorspace::extended_srgb_linear) {
		  output->Write(1, 1); // float
		  output->Write(2, 0b01); // bit_depth.bits_per_sample = 16
		  output->Write(4, 4); // bit_depth.exp_bits = 5
		} else {
		  output->Write(1, 0); // not float
		  if (frame->bitdepth == 10) {
			output->Write(2, 0b01); // bit_depth.bits_per_sample = 10
		  } else if (frame->bitdepth == 12) {
			output->Write(2, 0b10); // bit_depth.bits_per_sample = 12
		  } else {
			output->Write(2, 0b11); // 1 + u(6)
			output->Write(6, frame->bitdepth - 1);
		  }
		}
        output->Write(2, 0); // dim_shift = 0
        output->Write(2, 0); // name_len = 0
        output->Write(1, 0); // alpha_associated = 0
      }
    } else {
      output->Write(2, 0b00); // No extra channel
    }
    output->Write(1, 0); // Not XYB

	if (frame->color_space == rs_colorspace::hdr10_st2084) {
	  output->Write(1, 0);    // color_encoding.all_default false
	  output->Write(1, 0);    // color_encoding.want_icc false
	  output->Write(2, 0);    // RGB
	  output->Write(2, 1);    // D65
	  output->Write(2, 0b10); // primaries: 2 + u(4)
	  output->Write(4, 7);    // 2100
	  output->Write(1, 0);    // no gamma transfer function
	  output->Write(2, 0b10); // tf: 2 + u(4)
	  output->Write(4, 14);   // tf of PQ
	  output->Write(2, 1);    // relative rendering intent
	} else if (frame->color_space == rs_colorspace::extended_srgb_linear) {
	  output->Write(1, 0);    // color_encoding.all_default false
	  output->Write(1, 0);    // color_encoding.want_icc false
	  output->Write(2, 0);    // RGB
	  output->Write(2, 1);    // D65
	  output->Write(2, 1);    // sRGB
	  output->Write(1, 0);    // no gamma transfer function
	  output->Write(2, 0b10); // tf: 2 + u(4)
	  output->Write(4, 6);    // tf of linear
	  output->Write(2, 1);    // relative rendering intent
	} else if (frame->nb_chans == 1 || frame->nb_chans == 2) { // grayscale
      output->Write(1, 0);    // color_encoding.all_default false
      output->Write(1, 0);    // color_encoding.want_icc false
      output->Write(2, 1);    // grayscale
      output->Write(2, 1);    // D65
      output->Write(1, 0);    // no gamma transfer function
      output->Write(2, 0b10); // tf: 2 + u(4)
      output->Write(4, 11);   // tf of sRGB
      output->Write(2, 1);    // relative rendering intent
	} else { // anything else goes into sRGB
	  output->Write(1, 1); // color_encoding.all_default (sRGB)
	}
    output->Write(2, 0b00); // No extensions.

    output->Write(1, 1); // all_default transform data

    // No ICC, no preview. Frame should start at byte boundary.
    output->ZeroPadToByte();
  }

  // Handcrafted frame header.
  output->Write(1, 0);    // all_default
  output->Write(2, 0b00); // regular frame
  output->Write(1, 1);    // modular
  output->Write(2, 0b00); // default flags
  output->Write(1, 0);    // not YCbCr
  output->Write(2, 0b00); // no upsampling
  if (have_alpha) {
    output->Write(2, 0b00); // no alpha upsampling
  }
  output->Write(2, 0b01); // default group size
  output->Write(2, 0b00); // exactly one pass
  output->Write(1, 0);    // no custom size or origin
  output->Write(2, 0b00); // kReplace blending mode
  if (have_alpha) {
    output->Write(2, 0b00); // kReplace blending mode for alpha channel
  }
  output->Write(1, is_last); // is_last
  if (!is_last) {
    output->Write(2, 0b00); // can not be saved as reference
  }
  output->Write(2, 0b00); // a frame has no name
  output->Write(1, 0);    // loop filter is not all_default
  output->Write(1, 0);    // no gaborish
  output->Write(2, 0);    // 0 EPF iters
  output->Write(2, 0b00); // No LF extensions
  output->Write(2, 0b00); // No FH extensions

  output->Write(1, 0);     // No TOC permutation
  output->ZeroPadToByte(); // TOC is byte-aligned.
  assert(add_image_header || output->bytes_written <= kMaxFrameHeaderSize);
  for (size_t group_size : frame->group_sizes) {
    size_t bucket = TOCBucket(group_size);
    output->Write(2, bucket);
    output->Write(kTOCBits[bucket] - 2, group_size - kGroupSizeOffset[bucket]);
  }
  output->ZeroPadToByte(); // Groups are byte-aligned.
}

size_t JxlSimpleLosslessWriteOutput(JxlSimpleLosslessFrameState *frame,
                                    unsigned char *output, size_t output_size) {
  assert(output_size >= 32);
  unsigned char *initial_output = output;
  size_t (*append_bytes_with_bit_offset)(const uint8_t *, size_t, size_t,
                                         unsigned char *, uint64_t &) = nullptr;

  while (true) {
    size_t &cur = frame->current_bit_writer;
    size_t &bw_pos = frame->bit_writer_byte_pos;
    if (cur >= 1 + frame->group_data.size() * frame->nb_chans) {
      return output - initial_output;
    }
    if (output_size <= 9) {
      return output - initial_output;
    }
    size_t nbc = frame->nb_chans;
    const BitWriter &writer =
        cur == 0 ? frame->header
                 : frame->group_data[(cur - 1) / nbc][(cur - 1) % nbc];
    size_t full_byte_count =
        std::min(output_size - 9, writer.bytes_written - bw_pos);
    if (frame->bits_in_buffer == 0) {
      memcpy(output, writer.data.get() + bw_pos, full_byte_count);
    } else {
      size_t i = 0;
      if (append_bytes_with_bit_offset) {
        i += append_bytes_with_bit_offset(
            writer.data.get() + bw_pos, full_byte_count, frame->bits_in_buffer,
            output, frame->bit_buffer);
      }
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
      // Copy 8 bytes at a time until we reach the border.
      for (; i + 8 < full_byte_count; i += 8) {
        uint64_t chunk;
        memcpy(&chunk, writer.data.get() + bw_pos + i, 8);
        uint64_t out = frame->bit_buffer | (chunk << frame->bits_in_buffer);
        memcpy(output + i, &out, 8);
        frame->bit_buffer = chunk >> (64 - frame->bits_in_buffer);
      }
#endif
      for (; i < full_byte_count; i++) {
        AddBits(8, writer.data.get()[bw_pos + i], output + i,
                frame->bits_in_buffer, frame->bit_buffer);
      }
    }
    output += full_byte_count;
    output_size -= full_byte_count;
    bw_pos += full_byte_count;
    if (bw_pos == writer.bytes_written) {
      auto write = [&](size_t num, uint64_t bits) {
        size_t n = AddBits(num, bits, output, frame->bits_in_buffer,
                           frame->bit_buffer);
        output += n;
        output_size -= n;
      };
      if (writer.bits_in_buffer) {
        write(writer.bits_in_buffer, writer.buffer);
      }
      bw_pos = 0;
      cur++;
      if ((cur - 1) % nbc == 0 && frame->bits_in_buffer != 0) {
        write(8 - frame->bits_in_buffer, 0);
      }
    }
  }
}

void JxlSimpleLosslessFreeFrameState(JxlSimpleLosslessFrameState *frame) {
  delete frame;
}

void EncodeHybridUint000(uint32_t value, uint32_t *token, uint32_t *nbits,
                         uint32_t *bits) {
  uint32_t n = FloorLog2(value);
  *token = value ? n + 1 : 0;
  *nbits = value ? n : 0;
  *bits = value ? value - (1 << n) : 0;
}

constexpr static size_t kLogChunkSize = 3;
constexpr static size_t kChunkSize = 1 << kLogChunkSize;

template <typename Residual>
void GenericEncodeChunk(const Residual *residuals, size_t n, size_t skip,
                        const PrefixCode &code, BitWriter &output) {
  for (size_t ix = skip; ix < n; ix++) {
    unsigned token, nbits, bits;
    EncodeHybridUint000(residuals[ix], &token, &nbits, &bits);
    output.Write(code.raw_nbits[token] + nbits,
                 code.raw_bits[token] | bits << code.raw_nbits[token]);
  }
}

struct UpTo8Bits {
  size_t bitdepth;
  explicit UpTo8Bits(size_t bitdepth) : bitdepth(bitdepth) {
    assert(bitdepth <= 8);
  }
  // Here we can fit up to 9 extra bits + 7 Huffman bits in a u16; for all other
  // symbols, we could actually go up to 8 Huffman bits as we have at most 8
  // extra bits; however, the SIMD bit merging logic for AVX2 assumes that no
  // Huffman length is 8 or more, so we cap at 8 anyway. Last symbol is used for
  // LZ77 lengths and has no limitations except allowing to represent 32 symbols
  // in total.
  static constexpr uint8_t kMinRawLength[12] = {};
  static constexpr uint8_t kMaxRawLength[12] = {
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 10,
  };
  static size_t MaxEncodedBitsPerSample() { return 16; }
  static constexpr size_t kInputBytes = 1;
  using pixel_t = int16_t;
  using upixel_t = uint16_t;

  size_t NumSymbols(bool doing_ycocg_or_large_palette) const {
    // values gain 1 bit for YCoCg, 1 bit for prediction.
    // Maximum symbol is 1 + effective bit depth of residuals.
    if (doing_ycocg_or_large_palette) {
      return bitdepth + 3;
    } else {
      return bitdepth + 2;
    }
  }
};
constexpr uint8_t UpTo8Bits::kMinRawLength[];
constexpr uint8_t UpTo8Bits::kMaxRawLength[];

struct From9To13Bits {
  size_t bitdepth;
  explicit From9To13Bits(size_t bitdepth) : bitdepth(bitdepth) {
    assert(bitdepth <= 13 && bitdepth >= 9);
  }
  // Last symbol is used for LZ77 lengths and has no limitations except allowing
  // to represent 32 symbols in total.
  // We cannot fit all the bits in a u16, so do not even try and use up to 8
  // bits per raw symbol.
  // There are at most 16 raw symbols, so Huffman coding can be SIMDfied without
  // any special tricks.
  static constexpr uint8_t kMinRawLength[17] = {};
  static constexpr uint8_t kMaxRawLength[17] = {
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 10,
  };
  static size_t MaxEncodedBitsPerSample() { return 21; }
  static constexpr size_t kInputBytes = 2;
  using pixel_t = int16_t;
  using upixel_t = uint16_t;

  size_t NumSymbols(bool doing_ycocg_or_large_palette) const {
    // values gain 1 bit for YCoCg, 1 bit for prediction.
    // Maximum symbol is 1 + effective bit depth of residuals.
    if (doing_ycocg_or_large_palette) {
      return bitdepth + 3;
    } else {
      return bitdepth + 2;
    }
  }
};
constexpr uint8_t From9To13Bits::kMinRawLength[];
constexpr uint8_t From9To13Bits::kMaxRawLength[];

struct Exactly14Bits {
  explicit Exactly14Bits(size_t bitdepth_) { assert(bitdepth_ == 14); }
  // Force LZ77 symbols to have at least 8 bits, and raw symbols 15 and 16 to
  // have exactly 8, and no other symbol to have 8 or more. This ensures that
  // the representation for 15 and 16 is identical up to one bit.
  static constexpr uint8_t kMinRawLength[18] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 7,
  };
  static constexpr uint8_t kMaxRawLength[18] = {
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 10,
  };
  static constexpr size_t bitdepth = 14;
  static size_t MaxEncodedBitsPerSample() { return 22; }
  static constexpr size_t kInputBytes = 2;
  using pixel_t = int16_t;
  using upixel_t = uint16_t;

  size_t NumSymbols(bool) const { return 17; }
};
constexpr uint8_t Exactly14Bits::kMinRawLength[];
constexpr uint8_t Exactly14Bits::kMaxRawLength[];

struct MoreThan14Bits {
  size_t bitdepth;
  explicit MoreThan14Bits(size_t bitdepth) : bitdepth(bitdepth) {
    assert(bitdepth > 14);
    assert(bitdepth <= 16);
  }
  // Force LZ77 symbols to have at least 8 bits, and raw symbols 13 to 18 to
  // have exactly 8, and no other symbol to have 8 or more. This ensures that
  // the representation for (13, 14), (15, 16), (17, 18) is identical up to one
  // bit.
  static constexpr uint8_t kMinRawLength[20] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 7,
  };
  static constexpr uint8_t kMaxRawLength[20] = {
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 10,
  };
  static size_t MaxEncodedBitsPerSample() { return 24; }
  static constexpr size_t kInputBytes = 2;
  using pixel_t = int32_t;
  using upixel_t = uint32_t;

  size_t NumSymbols(bool) const { return 19; }
};
constexpr uint8_t MoreThan14Bits::kMinRawLength[];
constexpr uint8_t MoreThan14Bits::kMaxRawLength[];

void PrepareDCGlobalCommon(bool is_single_group, size_t width, size_t height,
                           const PrefixCode code[4], BitWriter *output) {
  output->Allocate(100000 + (is_single_group ? width * height * 16 : 0));
  // No patches, spline or noise.
  output->Write(1, 1); // default DC dequantization factors (?)
  output->Write(1, 1); // use global tree / histograms
  output->Write(1, 0); // no lz77 for the tree

  output->Write(1, 1);        // simple code for the tree's context map
  output->Write(2, 0);        // all contexts clustered together
  output->Write(1, 1);        // use prefix code for tree
  output->Write(4, 0);        // 000 hybrid uint
  output->Write(6, 0b100011); // Alphabet size is 4 (var16)
  output->Write(2, 1);        // simple prefix code
  output->Write(2, 3);        // with 4 symbols
  output->Write(2, 0);
  output->Write(2, 1);
  output->Write(2, 2);
  output->Write(2, 3);
  output->Write(1, 0); // First tree encoding option

  // Huffman table + extra bits for the tree.
  uint8_t symbol_bits[6] = {0b00, 0b10, 0b001, 0b101, 0b0011, 0b0111};
  uint8_t symbol_nbits[6] = {2, 2, 3, 3, 4, 4};
  // Write a tree with a leaf per channel, and gradient predictor for every
  // leaf.
  for (auto v : {1, 2, 1, 4, 1, 0, 0, 5, 0, 0, 0, 0, 5,
                 0, 0, 0, 0, 5, 0, 0, 0, 0, 5, 0, 0, 0}) {
    output->Write(symbol_nbits[v], symbol_bits[v]);
  }

  output->Write(1, 1);    // Enable lz77 for the main bitstream
  output->Write(2, 0b00); // lz77 offset 224
  static_assert(kLZ77Offset == 224, "kLZ77Offset should be 224");
  output->Write(4, 0b1010); // lz77 min length 7
  // 400 hybrid uint config for lz77
  output->Write(4, 4);
  output->Write(3, 0);
  output->Write(3, 0);

  output->Write(1, 1); // simple code for the context map
  output->Write(2, 3); // 3 bits per entry
  output->Write(3, 4); // channel 3
  output->Write(3, 3); // channel 2
  output->Write(3, 2); // channel 1
  output->Write(3, 1); // channel 0
  output->Write(3, 0); // distance histogram first

  output->Write(1, 1); // use prefix codes
  output->Write(4, 0); // 000 hybrid uint config for distances (only need 0)
  for (size_t i = 0; i < 4; i++) {
    output->Write(4, 0); // 000 hybrid uint config for symbols (only <= 10)
  }

  // Distance alphabet size:
  output->Write(5, 0b00001); // 2: just need 1 for RLE (i.e. distance 1)
  // Symbol + LZ77 alphabet size:
  for (size_t i = 0; i < 4; i++) {
    output->Write(1, 1);   // > 1
    output->Write(4, 8);   // <= 512
    output->Write(8, 256); // == 512
  }

  // Distance histogram:
  output->Write(2, 1); // simple prefix code
  output->Write(2, 0); // with one symbol
  output->Write(1, 1); // 1

  // Symbol + lz77 histogram:
  for (size_t i = 0; i < 4; i++) {
    code[i].WriteTo(output);
  }

  // Group header for global modular image.
  output->Write(1, 1); // Global tree
  output->Write(1, 1); // All default wp
}

void PrepareDCGlobal(bool is_single_group, size_t width, size_t height,
                     size_t nb_chans, const PrefixCode code[4],
                     BitWriter *output) {
  PrepareDCGlobalCommon(is_single_group, width, height, code, output);
  if (nb_chans > 2) {
    output->Write(2, 0b01);    // 1 transform
    output->Write(2, 0b00);    // RCT
    output->Write(5, 0b00000); // Starting from ch 0
    output->Write(2, 0b00);    // YCoCg
  } else {
    output->Write(2, 0b00); // no transforms
  }
  if (!is_single_group) {
    output->ZeroPadToByte();
  }
}

template <typename BitDepth> struct ChunkEncoder {
  FJXL_INLINE static void EncodeRle(size_t count, const PrefixCode &code,
                                    BitWriter &output) {
    if (count == 0)
      return;
    count -= kLZ77MinLength + 1;
    if (count < kLZ77CacheSize) {
      output.Write(code.lz77_cache_nbits[count], code.lz77_cache_bits[count]);
    } else {
      unsigned token, nbits, bits;
      EncodeHybridUintLZ77(count, &token, &nbits, &bits);
      uint64_t wbits = bits;
      wbits = (wbits << code.lz77_nbits[token]) | code.lz77_bits[token];
      wbits = (wbits << code.raw_nbits[0]) | code.raw_bits[0];
      output.Write(code.lz77_nbits[token] + nbits + code.raw_nbits[0], wbits);
    }
  }

  FJXL_INLINE void Chunk(size_t run, typename BitDepth::upixel_t *residuals,
                         size_t skip, size_t n) {
    EncodeRle(run, *code, *output);
    GenericEncodeChunk(residuals, n, skip, *code, *output);
  }

  inline void Finalize(size_t run) { EncodeRle(run, *code, *output); }

  const PrefixCode *code;
  BitWriter *output;
  alignas(64) uint8_t raw_nbits_simd[16] = {};
  alignas(64) uint8_t raw_bits_simd[16] = {};
};

template <typename BitDepth> struct ChunkSampleCollector {
  FJXL_INLINE void Rle(size_t count, uint64_t *lz77_counts_) {
    if (count == 0)
      return;
    raw_counts[0] += 1;
    count -= kLZ77MinLength + 1;
    unsigned token, nbits, bits;
    EncodeHybridUintLZ77(count, &token, &nbits, &bits);
    lz77_counts_[token]++;
  }

  FJXL_INLINE void Chunk(size_t run, typename BitDepth::upixel_t *residuals,
                         size_t skip, size_t n) {
    // Run is broken. Encode the run and encode the individual vector.
    Rle(run, lz77_counts);
    for (size_t ix = skip; ix < n; ix++) {
      unsigned token, nbits, bits;
      EncodeHybridUint000(residuals[ix], &token, &nbits, &bits);
      raw_counts[token]++;
    }
  }

  // don't count final run since we don't know how long it really is
  void Finalize(size_t run) {}

  uint64_t *raw_counts;
  uint64_t *lz77_counts;
};

constexpr uint32_t PackSigned(int32_t value) {
  return (static_cast<uint32_t>(value) << 1) ^
         ((static_cast<uint32_t>(~value) >> 31) - 1);
}

template <typename T, typename BitDepth> struct ChannelRowProcessor {
  using upixel_t = typename BitDepth::upixel_t;
  using pixel_t = typename BitDepth::pixel_t;
  T *t;
  void ProcessChunk(const pixel_t *row, const pixel_t *row_left,
                    const pixel_t *row_top, const pixel_t *row_topleft,
                    size_t n) {
    alignas(64) upixel_t residuals[kChunkSize] = {};
    size_t prefix_size = 0;
    size_t required_prefix_size = 0;
    for (size_t ix = 0; ix < kChunkSize; ix++) {
      pixel_t px = row[ix];
      pixel_t left = row_left[ix];
      pixel_t top = row_top[ix];
      pixel_t topleft = row_topleft[ix];
      pixel_t ac = left - topleft;
      pixel_t ab = left - top;
      pixel_t bc = top - topleft;
      pixel_t grad = static_cast<pixel_t>(static_cast<upixel_t>(ac) +
                                          static_cast<upixel_t>(top));
      pixel_t d = ab ^ bc;
      pixel_t clamp = d < 0 ? top : left;
      pixel_t s = ac ^ bc;
      pixel_t pred = s < 0 ? grad : clamp;
      residuals[ix] = PackSigned(px - pred);
      prefix_size = prefix_size == required_prefix_size
                        ? prefix_size + (residuals[ix] == 0)
                        : prefix_size;
      required_prefix_size += 1;
    }

    prefix_size = std::min(n, prefix_size);
    if (prefix_size == n && (run > 0 || prefix_size > kLZ77MinLength)) {
      // Run continues, nothing to do.
      run += prefix_size;
    } else if (prefix_size + run > kLZ77MinLength) {
      // Run is broken. Encode the run and encode the individual vector.
      t->Chunk(run + prefix_size, residuals, prefix_size, n);
      run = 0;
    } else {
      // There was no run to begin with.
      t->Chunk(0, residuals, 0, n);
    }
  }

  void ProcessRow(const pixel_t *row, const pixel_t *row_left,
                  const pixel_t *row_top, const pixel_t *row_topleft,
                  size_t xs) {
    for (size_t x = 0; x < xs; x += kChunkSize) {
      ProcessChunk(row + x, row_left + x, row_top + x, row_topleft + x,
                   std::min(kChunkSize, xs - x));
    }
  }

  void Finalize() { t->Finalize(run); }
  // Invariant: run == 0 or run > kLZ77MinLength.
  size_t run = 0;
};

uint16_t LoadLE16(const unsigned char *ptr) {
  return uint16_t{ptr[0]} | (uint16_t{ptr[1]} << 8);
}

uint16_t SwapEndian(uint16_t in) { return (in >> 8) | (in << 8); }

template <typename pixel_t>
void FillRowG8(const unsigned char *rgba, size_t oxs, pixel_t *luma) {
  size_t x = 0;
  for (; x < oxs; x++) {
    luma[x] = rgba[x];
  }
}

template <bool big_endian, typename pixel_t>
void FillRowG16(const unsigned char *rgba, size_t oxs, pixel_t *luma) {
  size_t x = 0;
  for (; x < oxs; x++) {
    uint16_t val = LoadLE16(rgba + 2 * x);
    if (big_endian) {
      val = SwapEndian(val);
    }
    luma[x] = val;
  }
}

template <typename pixel_t>
void FillRowGA8(const unsigned char *rgba, size_t oxs, pixel_t *luma,
                pixel_t *alpha) {
  size_t x = 0;
  for (; x < oxs; x++) {
    luma[x] = rgba[2 * x];
    alpha[x] = rgba[2 * x + 1];
  }
}

template <bool big_endian, typename pixel_t>
void FillRowGA16(const unsigned char *rgba, size_t oxs, pixel_t *luma,
                 pixel_t *alpha) {
  size_t x = 0;
  for (; x < oxs; x++) {
    uint16_t l = LoadLE16(rgba + 4 * x);
    uint16_t a = LoadLE16(rgba + 4 * x + 2);
    if (big_endian) {
      l = SwapEndian(l);
      a = SwapEndian(a);
    }
    luma[x] = l;
    alpha[x] = a;
  }
}

template <typename pixel_t>
void StoreYCoCg(pixel_t r, pixel_t g, pixel_t b, pixel_t *y, pixel_t *co,
                pixel_t *cg) {
  *co = r - b;
  pixel_t tmp = b + (*co >> 1);
  *cg = g - tmp;
  *y = tmp + (*cg >> 1);
}

template <typename pixel_t>
void FillRowRGB8(const unsigned char *rgba, size_t oxs, pixel_t *y, pixel_t *co,
                 pixel_t *cg) {
  size_t x = 0;
  for (; x < oxs; x++) {
    uint16_t r = rgba[3 * x];
    uint16_t g = rgba[3 * x + 1];
    uint16_t b = rgba[3 * x + 2];
    StoreYCoCg<pixel_t>(r, g, b, y + x, co + x, cg + x);
  }
}

template <bool big_endian, typename pixel_t>
void FillRowRGB16(const unsigned char *rgba, size_t oxs, pixel_t *y,
                  pixel_t *co, pixel_t *cg) {
  size_t x = 0;
  for (; x < oxs; x++) {
    uint16_t r = LoadLE16(rgba + 6 * x);
    uint16_t g = LoadLE16(rgba + 6 * x + 2);
    uint16_t b = LoadLE16(rgba + 6 * x + 4);
    if (big_endian) {
      r = SwapEndian(r);
      g = SwapEndian(g);
      b = SwapEndian(b);
    }
    StoreYCoCg<pixel_t>(r, g, b, y + x, co + x, cg + x);
  }
}

template <typename pixel_t>
void FillRowRGBA8(const unsigned char *rgba, size_t oxs, pixel_t *y,
                  pixel_t *co, pixel_t *cg, pixel_t *alpha) {
  size_t x = 0;
  for (; x < oxs; x++) {
    uint16_t r = rgba[4 * x];
    uint16_t g = rgba[4 * x + 1];
    uint16_t b = rgba[4 * x + 2];
    uint16_t a = rgba[4 * x + 3];
    StoreYCoCg<pixel_t>(r, g, b, y + x, co + x, cg + x);
    alpha[x] = a;
  }
}

template <bool big_endian, typename pixel_t>
void FillRowRGBA16(const unsigned char *rgba, size_t oxs, pixel_t *y,
                   pixel_t *co, pixel_t *cg, pixel_t *alpha) {
  size_t x = 0;
  for (; x < oxs; x++) {
    uint16_t r = LoadLE16(rgba + 8 * x);
    uint16_t g = LoadLE16(rgba + 8 * x + 2);
    uint16_t b = LoadLE16(rgba + 8 * x + 4);
    uint16_t a = LoadLE16(rgba + 8 * x + 6);
    if (big_endian) {
      r = SwapEndian(r);
      g = SwapEndian(g);
      b = SwapEndian(b);
      a = SwapEndian(a);
    }
    StoreYCoCg<pixel_t>(r, g, b, y + x, co + x, cg + x);
    alpha[x] = a;
  }
}

template <typename Processor, typename BitDepth>
void ProcessImageArea(const unsigned char *rgba, size_t x0, size_t y0,
                      size_t xs, size_t yskip, size_t ys, size_t row_stride,
                      BitDepth bitdepth, size_t nb_chans, bool big_endian,
                      Processor *processors) {
  constexpr size_t kPadding = 32;

  using pixel_t = typename BitDepth::pixel_t;

  constexpr size_t kAlign = 64;
  constexpr size_t kAlignPixels = kAlign / sizeof(pixel_t);

  auto align = [=](pixel_t *ptr) {
    size_t offset = reinterpret_cast<uintptr_t>(ptr) % kAlign;
    if (offset) {
      ptr += offset / sizeof(pixel_t);
    }
    return ptr;
  };

  constexpr size_t kNumPx =
      (256 + kPadding * 2 + kAlignPixels + kAlignPixels - 1) / kAlignPixels *
      kAlignPixels;

  std::vector<std::array<std::array<pixel_t, kNumPx>, 2>> group_data(nb_chans);

  for (size_t y = 0; y < ys; y++) {
    const auto rgba_row =
        rgba + row_stride * (y0 + y) + x0 * nb_chans * BitDepth::kInputBytes;
    pixel_t *crow[4] = {};
    pixel_t *prow[4] = {};
    for (size_t i = 0; i < nb_chans; i++) {
      crow[i] = align(&group_data[i][y & 1][kPadding]);
      prow[i] = align(&group_data[i][(y - 1) & 1][kPadding]);
    }

    // Pre-fill rows with YCoCg converted pixels.
    if (nb_chans == 1) {
      if (BitDepth::kInputBytes == 1) {
        FillRowG8(rgba_row, xs, crow[0]);
      } else if (big_endian) {
        FillRowG16</*big_endian=*/true>(rgba_row, xs, crow[0]);
      } else {
        FillRowG16</*big_endian=*/false>(rgba_row, xs, crow[0]);
      }
    } else if (nb_chans == 2) {
      if (BitDepth::kInputBytes == 1) {
        FillRowGA8(rgba_row, xs, crow[0], crow[1]);
      } else if (big_endian) {
        FillRowGA16</*big_endian=*/true>(rgba_row, xs, crow[0], crow[1]);
      } else {
        FillRowGA16</*big_endian=*/false>(rgba_row, xs, crow[0], crow[1]);
      }
    } else if (nb_chans == 3) {
      if (BitDepth::kInputBytes == 1) {
        FillRowRGB8(rgba_row, xs, crow[0], crow[1], crow[2]);
      } else if (big_endian) {
        FillRowRGB16</*big_endian=*/true>(rgba_row, xs, crow[0], crow[1],
                                          crow[2]);
      } else {
        FillRowRGB16</*big_endian=*/false>(rgba_row, xs, crow[0], crow[1],
                                           crow[2]);
      }
    } else {
      if (BitDepth::kInputBytes == 1) {
        FillRowRGBA8(rgba_row, xs, crow[0], crow[1], crow[2], crow[3]);
      } else if (big_endian) {
        FillRowRGBA16</*big_endian=*/true>(rgba_row, xs, crow[0], crow[1],
                                           crow[2], crow[3]);
      } else {
        FillRowRGBA16</*big_endian=*/false>(rgba_row, xs, crow[0], crow[1],
                                            crow[2], crow[3]);
      }
    }
    // Deal with x == 0.
    for (size_t c = 0; c < nb_chans; c++) {
      *(crow[c] - 1) = y > 0 ? *(prow[c]) : 0;
      // Fix topleft.
      *(prow[c] - 1) = y > 0 ? *(prow[c]) : 0;
    }
    if (y < yskip)
      continue;
    for (size_t c = 0; c < nb_chans; c++) {
      // Get pointers to px/left/top/topleft data to speedup loop.
      const pixel_t *row = crow[c];
      const pixel_t *row_left = crow[c] - 1;
      const pixel_t *row_top = y == 0 ? row_left : prow[c];
      const pixel_t *row_topleft = y == 0 ? row_left : prow[c] - 1;

      processors[c].ProcessRow(row, row_left, row_top, row_topleft, xs);
    }
  }
  for (size_t c = 0; c < nb_chans; c++) {
    processors[c].Finalize();
  }
}

template <typename BitDepth>
void WriteACSection(const unsigned char *rgba, size_t x0, size_t y0, size_t xs,
                    size_t ys, size_t row_stride, bool is_single_group,
                    BitDepth bitdepth, size_t nb_chans, bool big_endian,
                    const PrefixCode code[4],
                    std::array<BitWriter, 4> &output) {
  for (size_t i = 0; i < nb_chans; i++) {
    if (is_single_group && i == 0)
      continue;
    output[i].Allocate(xs * ys * bitdepth.MaxEncodedBitsPerSample() + 4);
  }
  if (!is_single_group) {
    // Group header for modular image.
    // When the image is single-group, the global modular image is the one
    // that contains the pixel data, and there is no group header.
    output[0].Write(1, 1);    // Global tree
    output[0].Write(1, 1);    // All default wp
    output[0].Write(2, 0b00); // 0 transforms
  }

  ChunkEncoder<BitDepth> encoders[4];
  ChannelRowProcessor<ChunkEncoder<BitDepth>, BitDepth> row_encoders[4];
  for (size_t c = 0; c < nb_chans; c++) {
    row_encoders[c].t = &encoders[c];
    encoders[c].output = &output[c];
    encoders[c].code = &code[c];
  }
  ProcessImageArea<ChannelRowProcessor<ChunkEncoder<BitDepth>, BitDepth>>(
      rgba, x0, y0, xs, 0, ys, row_stride, bitdepth, nb_chans, big_endian,
      row_encoders);
}

constexpr int kHashExp = 16;
constexpr uint32_t kHashSize = 1 << kHashExp;
constexpr uint32_t kHashMultiplier = 2654435761;
constexpr int kMaxColors = 512;

// can be any function that returns a value in 0 .. kHashSize-1
// has to map 0 to 0
inline uint32_t pixel_hash(uint32_t p) {
  return (p * kHashMultiplier) >> (32 - kHashExp);
}

template <size_t nb_chans>
void FillRowPalette(const unsigned char *inrow, size_t xs,
                    const int16_t *lookup, int16_t *out) {
  for (size_t x = 0; x < xs; x++) {
    uint32_t p = 0;
    for (size_t i = 0; i < nb_chans; ++i) {
      p |= inrow[x * nb_chans + i] << (8 * i);
    }
    out[x] = lookup[pixel_hash(p)];
  }
}

template <typename Processor>
void ProcessImageAreaPalette(const unsigned char *rgba, size_t x0, size_t y0,
                             size_t xs, size_t yskip, size_t ys,
                             size_t row_stride, const int16_t *lookup,
                             size_t nb_chans, Processor *processors) {
  constexpr size_t kPadding = 32;

  std::vector<std::array<int16_t, 256 + kPadding * 2>> group_data(2);
  Processor &row_encoder = processors[0];

  for (size_t y = 0; y < ys; y++) {
    // Pre-fill rows with palette converted pixels.
    const unsigned char *inrow = rgba + row_stride * (y0 + y) + x0 * nb_chans;
    int16_t *outrow = &group_data[y & 1][kPadding];
    if (nb_chans == 1) {
      FillRowPalette<1>(inrow, xs, lookup, outrow);
    } else if (nb_chans == 2) {
      FillRowPalette<2>(inrow, xs, lookup, outrow);
    } else if (nb_chans == 3) {
      FillRowPalette<3>(inrow, xs, lookup, outrow);
    } else if (nb_chans == 4) {
      FillRowPalette<4>(inrow, xs, lookup, outrow);
    }
    // Deal with x == 0.
    group_data[y & 1][kPadding - 1] =
        y > 0 ? group_data[(y - 1) & 1][kPadding] : 0;
    // Fix topleft.
    group_data[(y - 1) & 1][kPadding - 1] =
        y > 0 ? group_data[(y - 1) & 1][kPadding] : 0;
    // Get pointers to px/left/top/topleft data to speedup loop.
    const int16_t *row = &group_data[y & 1][kPadding];
    const int16_t *row_left = &group_data[y & 1][kPadding - 1];
    const int16_t *row_top =
        y == 0 ? row_left : &group_data[(y - 1) & 1][kPadding];
    const int16_t *row_topleft =
        y == 0 ? row_left : &group_data[(y - 1) & 1][kPadding - 1];

    row_encoder.ProcessRow(row, row_left, row_top, row_topleft, xs);
  }
  row_encoder.Finalize();
}

void WriteACSectionPalette(const unsigned char *rgba, size_t x0, size_t y0,
                           size_t xs, size_t ys, size_t row_stride,
                           bool is_single_group, const PrefixCode code[4],
                           const int16_t *lookup, size_t nb_chans,
                           BitWriter &output) {
  if (!is_single_group) {
    output.Allocate(16 * xs * ys + 4);
    // Group header for modular image.
    // When the image is single-group, the global modular image is the one
    // that contains the pixel data, and there is no group header.
    output.Write(1, 1);    // Global tree
    output.Write(1, 1);    // All default wp
    output.Write(2, 0b00); // 0 transforms
  }

  ChunkEncoder<UpTo8Bits> encoder;
  ChannelRowProcessor<ChunkEncoder<UpTo8Bits>, UpTo8Bits> row_encoder;

  row_encoder.t = &encoder;
  encoder.output = &output;
  encoder.code = &code[is_single_group ? 1 : 0];
  ProcessImageAreaPalette<
      ChannelRowProcessor<ChunkEncoder<UpTo8Bits>, UpTo8Bits>>(
      rgba, x0, y0, xs, 0, ys, row_stride, lookup, nb_chans, &row_encoder);
}

template <typename BitDepth>
void CollectSamples(const unsigned char *rgba, size_t x0, size_t y0, size_t xs,
                    size_t row_stride, size_t row_count,
                    uint64_t raw_counts[4][kNumRawSymbols],
                    uint64_t lz77_counts[4][kNumLZ77], bool is_single_group,
                    bool palette, BitDepth bitdepth, size_t nb_chans,
                    bool big_endian, const int16_t *lookup) {
  if (palette) {
    ChunkSampleCollector<UpTo8Bits> sample_collectors[4];
    ChannelRowProcessor<ChunkSampleCollector<UpTo8Bits>, UpTo8Bits>
        row_sample_collectors[4];
    for (size_t c = 0; c < nb_chans; c++) {
      row_sample_collectors[c].t = &sample_collectors[c];
      sample_collectors[c].raw_counts = raw_counts[is_single_group ? 1 : 0];
      sample_collectors[c].lz77_counts = lz77_counts[is_single_group ? 1 : 0];
    }
    ProcessImageAreaPalette<
        ChannelRowProcessor<ChunkSampleCollector<UpTo8Bits>, UpTo8Bits>>(
        rgba, x0, y0, xs, 1, 1 + row_count, row_stride, lookup, nb_chans,
        row_sample_collectors);
  } else {
    ChunkSampleCollector<BitDepth> sample_collectors[4];
    ChannelRowProcessor<ChunkSampleCollector<BitDepth>, BitDepth>
        row_sample_collectors[4];
    for (size_t c = 0; c < nb_chans; c++) {
      row_sample_collectors[c].t = &sample_collectors[c];
      sample_collectors[c].raw_counts = raw_counts[c];
      sample_collectors[c].lz77_counts = lz77_counts[c];
    }
    ProcessImageArea<
        ChannelRowProcessor<ChunkSampleCollector<BitDepth>, BitDepth>>(
        rgba, x0, y0, xs, 1, 1 + row_count, row_stride, bitdepth, nb_chans,
        big_endian, row_sample_collectors);
  }
}

void PrepareDCGlobalPalette(bool is_single_group, size_t width, size_t height,
                            size_t nb_chans, const PrefixCode code[4],
                            const std::vector<uint32_t> &palette,
                            size_t pcolors, BitWriter *output) {
  PrepareDCGlobalCommon(is_single_group, width, height, code, output);
  output->Write(2, 0b01);    // 1 transform
  output->Write(2, 0b01);    // Palette
  output->Write(5, 0b00000); // Starting from ch 0
  if (nb_chans == 1) {
    output->Write(2, 0b00); // 1-channel palette (Gray)
  } else if (nb_chans == 3) {
    output->Write(2, 0b01); // 3-channel palette (RGB)
  } else if (nb_chans == 4) {
    output->Write(2, 0b10); // 4-channel palette (RGBA)
  } else {
    output->Write(2, 0b11);
    output->Write(13, nb_chans - 1);
  }
  // pcolors <= kMaxColors + kChunkSize - 1
  static_assert(kMaxColors + kChunkSize < 1281,
                "add code to signal larger palette sizes");
  if (pcolors < 256) {
    output->Write(2, 0b00);
    output->Write(8, pcolors);
  } else {
    output->Write(2, 0b01);
    output->Write(10, pcolors - 256);
  }

  output->Write(2, 0b00); // nb_deltas == 0
  output->Write(4, 0);    // Zero predictor for delta palette
  // Encode palette
  ChunkEncoder<UpTo8Bits> encoder;
  ChannelRowProcessor<ChunkEncoder<UpTo8Bits>, UpTo8Bits> row_encoder;
  row_encoder.t = &encoder;
  encoder.output = output;
  encoder.code = &code[0];
  std::vector<std::array<int16_t, 32 + 1024>> p(4);
  size_t i = 0;
  size_t have_zero = 1;
  for (; i < pcolors; i++) {
    p[0][16 + i + have_zero] = palette[i] & 0xFF;
    p[1][16 + i + have_zero] = (palette[i] >> 8) & 0xFF;
    p[2][16 + i + have_zero] = (palette[i] >> 16) & 0xFF;
    p[3][16 + i + have_zero] = (palette[i] >> 24) & 0xFF;
  }
  p[0][15] = 0;
  row_encoder.ProcessRow(p[0].data() + 16, p[0].data() + 15, p[0].data() + 15,
                         p[0].data() + 15, pcolors);
  p[1][15] = p[0][16];
  p[0][15] = p[0][16];
  if (nb_chans > 1) {
    row_encoder.ProcessRow(p[1].data() + 16, p[1].data() + 15, p[0].data() + 16,
                           p[0].data() + 15, pcolors);
  }
  p[2][15] = p[1][16];
  p[1][15] = p[1][16];
  if (nb_chans > 2) {
    row_encoder.ProcessRow(p[2].data() + 16, p[2].data() + 15, p[1].data() + 16,
                           p[1].data() + 15, pcolors);
  }
  p[3][15] = p[2][16];
  p[2][15] = p[2][16];
  if (nb_chans > 3) {
    row_encoder.ProcessRow(p[3].data() + 16, p[3].data() + 15, p[2].data() + 16,
                           p[2].data() + 15, pcolors);
  }
  row_encoder.Finalize();

  if (!is_single_group) {
    output->ZeroPadToByte();
  }
}

template <size_t nb_chans>
bool detect_palette(const unsigned char *r, size_t width,
                    std::vector<uint32_t> &palette) {
  size_t x = 0;
  bool collided = false;
  for (; x < width; x++) {
    uint32_t p = 0;
    for (size_t i = 0; i < nb_chans; ++i) {
      p |= r[x * nb_chans + i] << (8 * i);
    }
    uint32_t index = pixel_hash(p);
    collided |= (palette[index] != 0 && p != palette[index]);
    palette[index] = p;
  }
  return collided;
}

template <typename BitDepth>
JxlSimpleLosslessFrameState *
LLPrepare(JxlChunkedFrameInputSource input, size_t width, size_t height,
          BitDepth bitdepth, size_t nb_chans, bool big_endian, int effort,
		  rs_colorspace color_space, int oneshot) {
  assert(width != 0);
  assert(height != 0);

  // Count colors to try palette
  std::vector<uint32_t> palette(kHashSize);
  std::vector<int16_t> lookup(kHashSize);
  lookup[0] = 0;
  int pcolors = 0;
  bool collided = effort < 2 || bitdepth.bitdepth != 8 || !oneshot;
  for (size_t y0 = 0; y0 < height && !collided; y0 += 256) {
    size_t ys = std::min<size_t>(height - y0, 256);
    for (size_t x0 = 0; x0 < width && !collided; x0 += 256) {
      size_t xs = std::min<size_t>(width - x0, 256);
      size_t stride;
      // TODO(szabadka): Add RAII wrapper around this.
      const void *buffer = input.get_color_channel_data_at(input.opaque, x0, y0,
                                                           xs, ys, &stride);
      auto rgba = reinterpret_cast<const unsigned char *>(buffer);
      for (size_t y = 0; y < ys && !collided; y++) {
        const unsigned char *r = rgba + stride * y;
        if (nb_chans == 1)
          collided = detect_palette<1>(r, xs, palette);
        if (nb_chans == 2)
          collided = detect_palette<2>(r, xs, palette);
        if (nb_chans == 3)
          collided = detect_palette<3>(r, xs, palette);
        if (nb_chans == 4)
          collided = detect_palette<4>(r, xs, palette);
      }
      input.release_buffer(input.opaque, buffer);
    }
  }
  int nb_entries = 0;
  if (!collided) {
    pcolors = 1; // always have all-zero as a palette color
    bool have_color = false;
    uint8_t minG = 255, maxG = 0;
    for (uint32_t k = 0; k < kHashSize; k++) {
      if (palette[k] == 0)
        continue;
      uint8_t p[4];
      for (int i = 0; i < 4; ++i) {
        p[i] = (palette[k] >> (8 * i)) & 0xFF;
      }
      // move entries to front so sort has less work
      palette[nb_entries] = palette[k];
      if (p[0] != p[1] || p[0] != p[2])
        have_color = true;
      if (p[1] < minG)
        minG = p[1];
      if (p[1] > maxG)
        maxG = p[1];
      nb_entries++;
      // don't do palette if too many colors are needed
      if (nb_entries + pcolors > kMaxColors) {
        collided = true;
        break;
      }
    }
    if (!have_color) {
      // don't do palette if it's just grayscale without many holes
      if (maxG - minG < nb_entries * 1.4f)
        collided = true;
    }
  }
  if (!collided) {
    std::sort(
        palette.begin(), palette.begin() + nb_entries,
        [&nb_chans](uint32_t ap, uint32_t bp) {
          if (ap == 0)
            return false;
          if (bp == 0)
            return true;
          uint8_t a[4], b[4];
          for (int i = 0; i < 4; ++i) {
            a[i] = (ap >> (8 * i)) & 0xFF;
            b[i] = (bp >> (8 * i)) & 0xFF;
          }
          float ay, by;
          if (nb_chans == 4) {
            ay = (0.299f * a[0] + 0.587f * a[1] + 0.114f * a[2] + 0.01f) * a[3];
            by = (0.299f * b[0] + 0.587f * b[1] + 0.114f * b[2] + 0.01f) * b[3];
          } else {
            ay = (0.299f * a[0] + 0.587f * a[1] + 0.114f * a[2] + 0.01f);
            by = (0.299f * b[0] + 0.587f * b[1] + 0.114f * b[2] + 0.01f);
          }
          return ay < by; // sort on alpha*luma
        });
    for (int k = 0; k < nb_entries; k++) {
      if (palette[k] == 0)
        break;
      lookup[pixel_hash(palette[k])] = pcolors++;
    }
  }

  size_t num_groups_x = (width + 255) / 256;
  size_t num_groups_y = (height + 255) / 256;
  size_t num_dc_groups_x = (width + 2047) / 2048;
  size_t num_dc_groups_y = (height + 2047) / 2048;

  uint64_t raw_counts[4][kNumRawSymbols] = {};
  uint64_t lz77_counts[4][kNumLZ77] = {};

  bool onegroup = num_groups_x == 1 && num_groups_y == 1;

  auto sample_rows = [&](size_t xg, size_t yg, size_t num_rows) {
    size_t y0 = yg * 256;
    size_t x0 = xg * 256;
    size_t ys = std::min<size_t>(height - y0, 256);
    size_t xs = std::min<size_t>(width - x0, 256);
    size_t stride;
    const void *buffer =
        input.get_color_channel_data_at(input.opaque, x0, y0, xs, ys, &stride);
    auto rgba = reinterpret_cast<const unsigned char *>(buffer);
    int y_begin_group =
        std::max<ssize_t>(0, static_cast<ssize_t>(ys) -
                                 static_cast<ssize_t>(num_rows)) /
        2;
    int y_count = std::min<int>(num_rows, ys - y_begin_group);
    int x_max = xs / kChunkSize * kChunkSize;
    CollectSamples(rgba, 0, y_begin_group, x_max, stride, y_count, raw_counts,
                   lz77_counts, onegroup, !collided, bitdepth, nb_chans,
                   big_endian, lookup.data());
    input.release_buffer(input.opaque, buffer);
  };

  // TODO(veluca): that `64` is an arbitrary constant, meant to correspond to
  // the point where the number of processed rows is large enough that loading
  // the entire image is cost-effective.
  if (oneshot || effort >= 64) {
    for (size_t g = 0; g < num_groups_y * num_groups_x; g++) {
      size_t xg = g % num_groups_x;
      size_t yg = g / num_groups_x;
      size_t y0 = yg * 256;
      size_t ys = std::min<size_t>(height - y0, 256);
      size_t num_rows = 2 * effort * ys / 256;
      sample_rows(xg, yg, num_rows);
    }
  } else {
    // sample the middle (effort * 2 * num_groups) rows of the center group
    // (possibly all of them).
    sample_rows((num_groups_x - 1) / 2, (num_groups_y - 1) / 2,
                2 * effort * num_groups_x * num_groups_y);
  }

  // TODO(veluca): can probably improve this and make it bitdepth-dependent.
  uint64_t base_raw_counts[kNumRawSymbols] = {
      3843, 852, 1270, 1214, 1014, 727, 481, 300, 159, 51,
      5,    1,   1,    1,    1,    1,   1,   1,   1};

  bool doing_ycocg = nb_chans > 2 && collided;
  bool large_palette = !collided || pcolors >= 256;
  for (size_t i = bitdepth.NumSymbols(doing_ycocg || large_palette);
       i < kNumRawSymbols; i++) {
    base_raw_counts[i] = 0;
  }

  for (size_t c = 0; c < 4; c++) {
    for (size_t i = 0; i < kNumRawSymbols; i++) {
      raw_counts[c][i] = (raw_counts[c][i] << 8) + base_raw_counts[i];
    }
  }

  if (!collided) {
    unsigned token, nbits, bits;
    EncodeHybridUint000(PackSigned(pcolors - 1), &token, &nbits, &bits);
    // ensure all palette indices can actually be encoded
    for (size_t i = 0; i < token + 1; i++)
      raw_counts[0][i] = std::max<uint64_t>(raw_counts[0][i], 1);
    // these tokens are only used for the palette itself so they can get a bad
    // code
    for (size_t i = token + 1; i < 10; i++)
      raw_counts[0][i] = 1;
  }

  uint64_t base_lz77_counts[kNumLZ77] = {
      29, 27, 25,  23, 21, 21, 19, 18, 21, 17, 16, 15, 15, 14,
      13, 13, 137, 98, 61, 34, 1,  1,  1,  1,  1,  1,  1,  1,
  };

  for (size_t c = 0; c < 4; c++) {
    for (size_t i = 0; i < kNumLZ77; i++) {
      lz77_counts[c][i] = (lz77_counts[c][i] << 8) + base_lz77_counts[i];
    }
  }

  JxlSimpleLosslessFrameState *frame_state = new JxlSimpleLosslessFrameState();
  for (size_t i = 0; i < 4; i++) {
    frame_state->hcode[i] = PrefixCode(bitdepth, raw_counts[i], lz77_counts[i]);
  }

  size_t num_dc_groups = num_dc_groups_x * num_dc_groups_y;
  size_t num_ac_groups = num_groups_x * num_groups_y;
  size_t num_groups = onegroup ? 1 : (2 + num_dc_groups + num_ac_groups);
  frame_state->input = input;
  frame_state->width = width;
  frame_state->height = height;
  frame_state->num_groups_x = num_groups_x;
  frame_state->num_groups_y = num_groups_y;
  frame_state->num_dc_groups_x = num_dc_groups_x;
  frame_state->num_dc_groups_y = num_dc_groups_y;
  frame_state->nb_chans = nb_chans;
  frame_state->bitdepth = bitdepth.bitdepth;
  frame_state->big_endian = big_endian;
  frame_state->effort = effort;
  frame_state->collided = collided;
  frame_state->lookup = lookup;
  frame_state->color_space = color_space;

  frame_state->group_data = std::vector<std::array<BitWriter, 4>>(num_groups);
  frame_state->group_sizes.resize(num_groups);
  if (collided) {
    PrepareDCGlobal(onegroup, width, height, nb_chans, frame_state->hcode,
                    &frame_state->group_data[0][0]);
  } else {
    PrepareDCGlobalPalette(onegroup, width, height, nb_chans,
                           frame_state->hcode, palette, pcolors,
                           &frame_state->group_data[0][0]);
  }
  frame_state->group_sizes[0] = SectionSize(frame_state->group_data[0]);
  if (!onegroup) {
    ComputeAcGroupDataOffset(frame_state->group_sizes[0], num_dc_groups,
                             num_ac_groups, frame_state->min_dc_global_size,
                             frame_state->ac_group_data_offset);
  }

  return frame_state;
}

template <typename BitDepth>
bool LLProcess(JxlSimpleLosslessFrameState *frame_state, bool is_last,
               BitDepth bitdepth, void *runner_opaque,
               FJxlParallelRunner runner) {
  // The maximum number of groups that we process concurrently here.
  // TODO(szabadka) Use the number of threads or some outside parameter for the
  // maximum memory usage instead.
  constexpr size_t kMaxLocalGroups = 16;
  bool onegroup = frame_state->group_sizes.size() == 1;

  size_t total_groups = frame_state->num_groups_x * frame_state->num_groups_y;
  JxlSimpleLosslessFrameState local_frame_state;
  auto run_one = [&](size_t g) {
    size_t xg = g % frame_state->num_groups_x;
    size_t yg = g / frame_state->num_groups_x;
    size_t num_dc_groups =
        frame_state->num_dc_groups_x * frame_state->num_dc_groups_y;
    size_t group_id = onegroup ? 0 : (2 + num_dc_groups + g);
    size_t xs = std::min<size_t>(frame_state->width - xg * 256, 256);
    size_t ys = std::min<size_t>(frame_state->height - yg * 256, 256);
    size_t x0 = xg * 256;
    size_t y0 = yg * 256;
    size_t stride;
    JxlChunkedFrameInputSource input = frame_state->input;
    const void *buffer =
        input.get_color_channel_data_at(input.opaque, x0, y0, xs, ys, &stride);
    const unsigned char *rgba = reinterpret_cast<const unsigned char *>(buffer);

    auto &gd = frame_state->group_data[group_id];
    if (frame_state->collided) {
      WriteACSection(rgba, 0, 0, xs, ys, stride, onegroup, bitdepth,
                     frame_state->nb_chans, frame_state->big_endian,
                     frame_state->hcode, gd);
    } else {
      WriteACSectionPalette(rgba, 0, 0, xs, ys, stride, onegroup,
                            frame_state->hcode, frame_state->lookup.data(),
                            frame_state->nb_chans, gd[0]);
    }
    frame_state->group_sizes[group_id] = SectionSize(gd);
    input.release_buffer(input.opaque, buffer);
  };
  runner(
      runner_opaque, &run_one,
      +[](void *r, size_t i) { (*reinterpret_cast<decltype(&run_one)>(r))(i); },
      total_groups);

  return true;
}

JxlSimpleLosslessFrameState *
JxlSimpleLosslessPrepareImpl(JxlChunkedFrameInputSource input, size_t width,
                             size_t height, size_t nb_chans, size_t bitdepth,
                             bool big_endian, int effort, rs_colorspace color_space, int oneshot) {
  assert(bitdepth > 0);
  assert(nb_chans <= 4);
  assert(nb_chans != 0);
  if (bitdepth <= 8) {
    return LLPrepare(input, width, height, UpTo8Bits(bitdepth), nb_chans,
                     big_endian, effort, color_space, oneshot);
  }
  if (bitdepth <= 13) {
    return LLPrepare(input, width, height, From9To13Bits(bitdepth), nb_chans,
                     big_endian, effort, color_space, oneshot);
  }
  if (bitdepth == 14) {
    return LLPrepare(input, width, height, Exactly14Bits(bitdepth), nb_chans,
                     big_endian, effort, color_space, oneshot);
  }
  return LLPrepare(input, width, height, MoreThan14Bits(bitdepth), nb_chans,
                   big_endian, effort, color_space, oneshot);
}

bool JxlSimpleLosslessProcessFrameImpl(JxlSimpleLosslessFrameState *frame_state,
                                       bool is_last, void *runner_opaque,
                                       FJxlParallelRunner runner) {
  const size_t bitdepth = frame_state->bitdepth;
  if (bitdepth <= 8) {
    return LLProcess(frame_state, is_last, UpTo8Bits(bitdepth), runner_opaque,
                     runner);
  } else if (bitdepth <= 13) {
    return LLProcess(frame_state, is_last, From9To13Bits(bitdepth),
                     runner_opaque, runner);
  } else if (bitdepth == 14) {
    return LLProcess(frame_state, is_last, Exactly14Bits(bitdepth),
                     runner_opaque, runner);
  } else {
    return LLProcess(frame_state, is_last, MoreThan14Bits(bitdepth),
                     runner_opaque, runner);
  }
  return true;
}

class FJxlFrameInput {
public:
  FJxlFrameInput(const unsigned char *rgba, size_t row_stride, size_t nb_chans,
                 size_t bitdepth)
      : rgba_(rgba), row_stride_(row_stride),
        bytes_per_pixel_(bitdepth <= 8 ? nb_chans : 2 * nb_chans) {}

  JxlChunkedFrameInputSource GetInputSource() {
    return JxlChunkedFrameInputSource{this, GetDataAt,
                                      [](void *, const void *) {}};
  }

private:
  static const void *GetDataAt(void *opaque, size_t xpos, size_t ypos,
                               size_t xsize, size_t ysize, size_t *row_offset) {
    FJxlFrameInput *self = static_cast<FJxlFrameInput *>(opaque);
    *row_offset = self->row_stride_;
    return self->rgba_ + ypos * (*row_offset) + xpos * self->bytes_per_pixel_;
  }

  const uint8_t *rgba_;
  size_t row_stride_;
  size_t bytes_per_pixel_;
};

size_t JxlSimpleLosslessEncode(const unsigned char *rgba, size_t width,
                               size_t row_stride, size_t height,
                               size_t nb_chans, size_t bitdepth,
                               bool big_endian, int effort, rs_colorspace color_space,
                               unsigned char **output, void *runner_opaque,
                               FJxlParallelRunner runner) {
  FJxlFrameInput input(rgba, row_stride, nb_chans, bitdepth);
  auto *frame_state = JxlSimpleLosslessPrepareFrame(
      input.GetInputSource(), width, height, nb_chans, bitdepth, big_endian,
      effort, color_space, /*oneshot=*/true);
  if (!JxlSimpleLosslessProcessFrame(frame_state, /*is_last=*/true,
                                     runner_opaque, runner)) {
    return 0;
  }
  JxlSimpleLosslessPrepareHeader(frame_state, /*add_image_header=*/1,
                                 /*is_last=*/1);
  size_t output_size = JxlSimpleLosslessMaxRequiredOutput(frame_state);
  *output = (unsigned char *)malloc(output_size);
  size_t written = 0;
  size_t total = 0;
  while ((written = JxlSimpleLosslessWriteOutput(frame_state, *output + total,
                                                 output_size - total)) != 0) {
    total += written;
  }
  JxlSimpleLosslessFreeFrameState(frame_state);
  return total;
}

JxlSimpleLosslessFrameState *
JxlSimpleLosslessPrepareFrame(JxlChunkedFrameInputSource input, size_t width,
                              size_t height, size_t nb_chans, size_t bitdepth,
                              bool big_endian, int effort, rs_colorspace color_space, int oneshot) {
  return JxlSimpleLosslessPrepareImpl(input, width, height, nb_chans, bitdepth,
                                      big_endian, effort, color_space, oneshot);
}

bool JxlSimpleLosslessProcessFrame(JxlSimpleLosslessFrameState *frame_state,
                                   bool is_last, void *runner_opaque,
                                   FJxlParallelRunner runner) {
  auto trivial_runner =
      +[](void *, void *opaque, void fun(void *, size_t), size_t count) {
        for (size_t i = 0; i < count; i++) {
          fun(opaque, i);
        }
      };

  if (runner == nullptr) {
    runner = trivial_runner;
  }

  return JxlSimpleLosslessProcessFrameImpl(frame_state, is_last, runner_opaque,
                                           runner);
}
