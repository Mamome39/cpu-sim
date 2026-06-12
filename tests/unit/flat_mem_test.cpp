#include <gtest/gtest.h>
#include "cpusim/memory/flat_mem.h"

using namespace cpusim;

static constexpr uint32_t BASE = 0x80000000;
static constexpr size_t   SZ   = 0x1000;

TEST(FlatMem, LoadWordRoundTrip) {
    FlatMem m(BASE, SZ);
    m.store_word(BASE, 0xDEADBEEFu);
    EXPECT_EQ(m.load_word(BASE), 0xDEADBEEFu);
}

TEST(FlatMem, LittleEndianLayout) {
    FlatMem m(BASE, SZ);
    m.store_word(BASE, 0x04030201u);
    EXPECT_EQ(m.load_byte(BASE + 0), 0x01u);
    EXPECT_EQ(m.load_byte(BASE + 1), 0x02u);
    EXPECT_EQ(m.load_byte(BASE + 2), 0x03u);
    EXPECT_EQ(m.load_byte(BASE + 3), 0x04u);
}

TEST(FlatMem, LoadHalfRoundTrip) {
    FlatMem m(BASE, SZ);
    m.store_half(BASE, 0xABCDu);
    EXPECT_EQ(m.load_half(BASE), 0xABCDu);
}

TEST(FlatMem, StoreByteLoadByte) {
    FlatMem m(BASE, SZ);
    m.store_byte(BASE + 7, 0xFF);
    EXPECT_EQ(m.load_byte(BASE + 7), 0xFFu);
}

TEST(FlatMem, InitialisedToZero) {
    FlatMem m(BASE, SZ);
    EXPECT_EQ(m.load_word(BASE), 0u);
}

TEST(FlatMem, WriteBytes) {
    FlatMem m(BASE, SZ);
    uint8_t prog[] = {0x13, 0x00, 0x00, 0x00,  // nop
                      0x93, 0x00, 0x10, 0x00};  // addi x1,x0,1
    m.write_bytes(BASE, prog, sizeof(prog));
    EXPECT_EQ(m.load_word(BASE),     0x00000013u);
    EXPECT_EQ(m.load_word(BASE + 4), 0x00100093u);
}

TEST(FlatMem, IndependentAddresses) {
    FlatMem m(BASE, SZ);
    m.store_word(BASE,      0x11111111u);
    m.store_word(BASE + 4,  0x22222222u);
    m.store_word(BASE + 8,  0x33333333u);
    EXPECT_EQ(m.load_word(BASE),     0x11111111u);
    EXPECT_EQ(m.load_word(BASE + 4), 0x22222222u);
    EXPECT_EQ(m.load_word(BASE + 8), 0x33333333u);
}
