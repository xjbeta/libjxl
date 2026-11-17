// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#ifndef LIB_JXL_FORK_PROGRESS_H_
#define LIB_JXL_FORK_PROGRESS_H_

// Small, self-contained progress reporter for the fork's cjxl. Keeping every
// detail here means enc_frame.cc only needs three calls and future upstream
// changes are unlikely to touch them.
//
// The first 15% covers reads, decoding and encoder setup. The remaining 85%
// is the AC group phase, which is the only part that dominates an encode.

#include <atomic>
#include <cstddef>
#include <cstdio>

namespace jxl {
namespace fork_progress {

inline std::atomic<size_t>& Completed() {
  static std::atomic<size_t> value{0};
  return value;
}

inline std::atomic<size_t>& Total() {
  static std::atomic<size_t> value{0};
  return value;
}

inline std::atomic<int>& LastPercent() {
  static std::atomic<int> value{-1};
  return value;
}

inline void Emit(int percent) {
  std::fprintf(stderr, "JXLQL_PROGRESS %d\n", percent);
  std::fflush(stderr);
}

// Called once per top-level frame. cjxl encodes one image, so a single stream
// of percentages is enough; the reset also makes this safe to reuse.
inline void Reset(size_t total_groups) {
  Total().store(total_groups, std::memory_order_relaxed);
  Completed().store(0, std::memory_order_relaxed);
  LastPercent().store(-1, std::memory_order_relaxed);
  Emit(5);
}

// Called after one AC group is fully written. Groups are encoded in several
// streaming sections; Completed() is global, so the percentage never goes
// backwards.
inline void GroupDone() {
  const size_t total = Total().load(std::memory_order_relaxed);
  if (total == 0) return;

  const size_t done =
      Completed().fetch_add(1, std::memory_order_relaxed) + 1;
  int percent = 15 + static_cast<int>(done * 85 / total);
  if (percent > 100) percent = 100;

  int last = LastPercent().load(std::memory_order_relaxed);
  if (percent >= last + 5 &&
      LastPercent().compare_exchange_strong(last, percent)) {
    Emit(percent);
  }
}

inline void Finish() {
  int last = LastPercent().load(std::memory_order_relaxed);
  if (last < 100 && LastPercent().compare_exchange_strong(last, 100)) {
    Emit(100);
  }
}

}  // namespace fork_progress
}  // namespace jxl

#endif  // LIB_JXL_FORK_PROGRESS_H_
