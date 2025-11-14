// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#pragma once

#include "jxl_simple_lossless.hpp"
#include "reshade_api.hpp"

namespace simple_jxl {
using format = reshade::api::format;
using color_space = reshade::api::color_space;

bool writer(const std::vector<uint8_t> &pixel_data,
            std::vector<uint8_t> &output_data, size_t j_width, size_t j_height,
            int c_num, const format fmt = format::r8g8b8a8_unorm,
            const color_space cs = color_space::srgb_nonlinear) {
  size_t num_threads = 0;
  int bitdepth = 8;

  auto parallel_runner = [](void *num_threads_ptr, void *opaque,
                            void fun(void *, size_t), size_t count) {
    size_t num_threads = *static_cast<size_t *>(num_threads_ptr);
    if (num_threads == 0) {
      num_threads = std::thread::hardware_concurrency();
    }
    if (num_threads > count) {
      num_threads = count;
    }
    if (num_threads == 1) {
      for (size_t i = 0; i < count; i++) {
        fun(opaque, i);
      }
    } else {
      std::atomic<uint32_t> task{0};
      std::vector<std::thread> threads;
      for (size_t i = 0; i < num_threads; i++) {
        threads.push_back(std::thread([count, opaque, fun, &task]() {
          while (true) {
            uint32_t t = task++;
            if (t >= count)
              break;
            fun(opaque, t);
          }
        }));
      }
      for (auto &t : threads)
        t.join();
    }
  };

  if (pixel_data.size() < j_width * j_height * c_num) {
    // Assert so that it won't crash the whole app
    reshade::log::message(reshade::log::level::error, "Pixel size mismatch");
    return false;
  }

  const unsigned char *pix_data =
      reinterpret_cast<const unsigned char *>(pixel_data.data());
  std::vector<uint16_t> converted_pix;

  if (fmt == format::r10g10b10a2_unorm || fmt == format::b10g10r10a2_unorm) {
    converted_pix.resize(j_height * j_width * 3, 0x0);

    uint16_t *cpix_ptr = converted_pix.data();
    const uint32_t *src_pixels =
        reinterpret_cast<const uint32_t *>(pixel_data.data());

    if (converted_pix.size() != (pixel_data.size() / 4 * 3)) {
      // Final check
      reshade::log::message(reshade::log::level::error, "Pixel size mismatch");
      return false;
    }

    for (size_t i = 0; i < j_height * j_width; i++) {
      const uint32_t pix = *src_pixels;
      const uint16_t pixels[] = {(pix & 0x000003FFU) & 0xFFFFU,
                                 ((pix & 0x000FFC00U) >> 10U) & 0xFFFFU,
                                 ((pix & 0x3FF00000U) >> 20U) & 0xFFFFU};

      memcpy(cpix_ptr, pixels, sizeof(pixels));

	  src_pixels++;
	  cpix_ptr += 3;
    }
    pix_data = reinterpret_cast<const unsigned char *>(converted_pix.data());
    c_num = 3;
    bitdepth = 10;
  } else if (fmt == format::r16g16b16a16_float) {
    bitdepth = 16;
  }

  size_t encoded_size = 0;
  unsigned char *encoded = nullptr;
  size_t stride = j_width * c_num * (bitdepth > 8 ? 2 : 1);

  encoded_size = JxlSimpleLosslessEncode(
      pix_data, j_width, stride, j_height, c_num, bitdepth,
      /*big_endian=*/false, 1, cs, &encoded, &num_threads, +parallel_runner);

  if (encoded_size == 0) {
    free(encoded);
    reshade::log::message(reshade::log::level::error, "JXL Encode error");
    return false;
  }

  output_data.resize(encoded_size, 0x0);
  memcpy(output_data.data(), encoded, encoded_size);
  free(encoded);

  return true;
}
} // namespace simple_jxl
