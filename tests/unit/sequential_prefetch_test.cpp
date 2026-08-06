#include <gtest/gtest.h>
#include "cpusim/memory/sequential_prefetch.h"

using namespace cpusim;

TEST(SequentialPrefetch, NextIsOneLineAhead) {
    SequentialPrefetch p(32);
    EXPECT_EQ(p.next(0x1000), 0x1020u);
}

TEST(SequentialPrefetch, AddsLineBytesRegardlessOfAlignment) {
    SequentialPrefetch p(16);
    EXPECT_EQ(p.next(0x2004), 0x2014u);
}
