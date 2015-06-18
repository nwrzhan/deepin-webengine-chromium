// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>   // for min()

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

// Number of bits in a size_t.
static const int kSizeBits = 8 * sizeof(size_t);
// The maximum size of a size_t.
static const size_t kMaxSize = ~static_cast<size_t>(0);
// Maximum positive size of a size_t if it were signed.
static const size_t kMaxSignedSize = ((size_t(1) << (kSizeBits-1)) - 1);

namespace {

using std::min;

// Fill a buffer of the specified size with a predetermined pattern
static void Fill(unsigned char* buffer, int n) {
  for (int i = 0; i < n; i++) {
    buffer[i] = (i & 0xff);
  }
}

// Check that the specified buffer has the predetermined pattern
// generated by Fill()
static bool Valid(unsigned char* buffer, int n) {
  for (int i = 0; i < n; i++) {
    if (buffer[i] != (i & 0xff)) {
      return false;
    }
  }
  return true;
}

// Check that a buffer is completely zeroed.
static bool IsZeroed(unsigned char* buffer, int n) {
  for (int i = 0; i < n; i++) {
    if (buffer[i] != 0) {
      return false;
    }
  }
  return true;
}

// Check alignment
static void CheckAlignment(void* p, int align) {
  EXPECT_EQ(0, reinterpret_cast<uintptr_t>(p) & (align-1));
}

// Return the next interesting size/delta to check.  Returns -1 if no more.
static int NextSize(int size) {
  if (size < 100)
    return size+1;

  if (size < 100000) {
    // Find next power of two
    int power = 1;
    while (power < size)
      power <<= 1;

    // Yield (power-1, power, power+1)
    if (size < power-1)
      return power-1;

    if (size == power-1)
      return power;

    assert(size == power);
    return power+1;
  } else {
    return -1;
  }
}

static void TestCalloc(size_t n, size_t s, bool ok) {
  char* p = reinterpret_cast<char*>(calloc(n, s));
  if (!ok) {
    EXPECT_EQ(NULL, p) << "calloc(n, s) should not succeed";
  } else {
    EXPECT_NE(reinterpret_cast<void*>(NULL), p) <<
        "calloc(n, s) should succeed";
    for (size_t i = 0; i < n*s; i++) {
      EXPECT_EQ('\0', p[i]);
    }
    free(p);
  }
}

}  // namespace

//-----------------------------------------------------------------------------


TEST(Allocators, Malloc) {
  // Try allocating data with a bunch of alignments and sizes
  for (int size = 1; size < 1048576; size *= 2) {
    unsigned char* ptr = reinterpret_cast<unsigned char*>(malloc(size));
    CheckAlignment(ptr, 2);  // Should be 2 byte aligned
    Fill(ptr, size);
    EXPECT_TRUE(Valid(ptr, size));
    free(ptr);
  }
}

TEST(Allocators, Calloc) {
  TestCalloc(0, 0, true);
  TestCalloc(0, 1, true);
  TestCalloc(1, 1, true);
  TestCalloc(1<<10, 0, true);
  TestCalloc(1<<20, 0, true);
  TestCalloc(0, 1<<10, true);
  TestCalloc(0, 1<<20, true);
  TestCalloc(1<<20, 2, true);
  TestCalloc(2, 1<<20, true);
  TestCalloc(1000, 1000, true);

  TestCalloc(kMaxSize, 2, false);
  TestCalloc(2, kMaxSize, false);
  TestCalloc(kMaxSize, kMaxSize, false);

  TestCalloc(kMaxSignedSize, 3, false);
  TestCalloc(3, kMaxSignedSize, false);
  TestCalloc(kMaxSignedSize, kMaxSignedSize, false);
}

// This makes sure that reallocing a small number of bytes in either
// direction doesn't cause us to allocate new memory.
TEST(Allocators, Realloc1) {
  int start_sizes[] = { 100, 1000, 10000, 100000 };
  int deltas[] = { 1, -2, 4, -8, 16, -32, 64, -128 };

  for (int s = 0; s < sizeof(start_sizes)/sizeof(*start_sizes); ++s) {
    void* p = malloc(start_sizes[s]);
    ASSERT_TRUE(p);
    // The larger the start-size, the larger the non-reallocing delta.
    for (int d = 0; d < s*2; ++d) {
      void* new_p = realloc(p, start_sizes[s] + deltas[d]);
      ASSERT_EQ(p, new_p);  // realloc should not allocate new memory
    }
    // Test again, but this time reallocing smaller first.
    for (int d = 0; d < s*2; ++d) {
      void* new_p = realloc(p, start_sizes[s] - deltas[d]);
      ASSERT_EQ(p, new_p);  // realloc should not allocate new memory
    }
    free(p);
  }
}

TEST(Allocators, Realloc2) {
  for (int src_size = 0; src_size >= 0; src_size = NextSize(src_size)) {
    for (int dst_size = 0; dst_size >= 0; dst_size = NextSize(dst_size)) {
      unsigned char* src = reinterpret_cast<unsigned char*>(malloc(src_size));
      Fill(src, src_size);
      unsigned char* dst =
          reinterpret_cast<unsigned char*>(realloc(src, dst_size));
      EXPECT_TRUE(Valid(dst, min(src_size, dst_size)));
      Fill(dst, dst_size);
      EXPECT_TRUE(Valid(dst, dst_size));
      if (dst != NULL) free(dst);
    }
  }

  // Now make sure realloc works correctly even when we overflow the
  // packed cache, so some entries are evicted from the cache.
  // The cache has 2^12 entries, keyed by page number.
  const int kNumEntries = 1 << 14;
  int** p = reinterpret_cast<int**>(malloc(sizeof(*p) * kNumEntries));
  int sum = 0;
  for (int i = 0; i < kNumEntries; i++) {
    // no page size is likely to be bigger than 8192?
    p[i] = reinterpret_cast<int*>(malloc(8192));
    p[i][1000] = i;              // use memory deep in the heart of p
  }
  for (int i = 0; i < kNumEntries; i++) {
    p[i] = reinterpret_cast<int*>(realloc(p[i], 9000));
  }
  for (int i = 0; i < kNumEntries; i++) {
    sum += p[i][1000];
    free(p[i]);
  }
  EXPECT_EQ(kNumEntries/2 * (kNumEntries - 1), sum);  // assume kNE is even
  free(p);
}

// Test recalloc
TEST(Allocators, Recalloc) {
  for (int src_size = 0; src_size >= 0; src_size = NextSize(src_size)) {
    for (int dst_size = 0; dst_size >= 0; dst_size = NextSize(dst_size)) {
      unsigned char* src =
          reinterpret_cast<unsigned char*>(_recalloc(NULL, 1, src_size));
      EXPECT_TRUE(IsZeroed(src, src_size));
      Fill(src, src_size);
      unsigned char* dst =
          reinterpret_cast<unsigned char*>(_recalloc(src, 1, dst_size));
      EXPECT_TRUE(Valid(dst, min(src_size, dst_size)));
      Fill(dst, dst_size);
      EXPECT_TRUE(Valid(dst, dst_size));
      if (dst != NULL)
        free(dst);
    }
  }
}

// Test windows specific _aligned_malloc() and _aligned_free() methods.
TEST(Allocators, AlignedMalloc) {
  // Try allocating data with a bunch of alignments and sizes
  static const int kTestAlignments[] = {8, 16, 256, 4096, 8192, 16384};
  for (int size = 1; size > 0; size = NextSize(size)) {
    for (int i = 0; i < arraysize(kTestAlignments); ++i) {
      unsigned char* ptr = static_cast<unsigned char*>(
          _aligned_malloc(size, kTestAlignments[i]));
      CheckAlignment(ptr, kTestAlignments[i]);
      Fill(ptr, size);
      EXPECT_TRUE(Valid(ptr, size));

      // Make a second allocation of the same size and alignment to prevent
      // allocators from passing this test by accident.  Per jar, tcmalloc
      // provides allocations for new (never before seen) sizes out of a thread
      // local heap of a given "size class."  Each time the test requests a new
      // size, it will usually get the first element of a span, which is a
      // 4K aligned allocation.
      unsigned char* ptr2 = static_cast<unsigned char*>(
          _aligned_malloc(size, kTestAlignments[i]));
      CheckAlignment(ptr2, kTestAlignments[i]);
      Fill(ptr2, size);
      EXPECT_TRUE(Valid(ptr2, size));

      // Should never happen, but sanity check just in case.
      ASSERT_NE(ptr, ptr2);
      _aligned_free(ptr);
      _aligned_free(ptr2);
    }
  }
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}