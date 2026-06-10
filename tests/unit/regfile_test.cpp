#include <gtest/gtest.h>
#include "cpusim/regfile.h"

using cpusim::RegFile;

TEST(RegFile, InitializesToZero) {
    RegFile rf;
    for (std::size_t i = 0; i < 32; ++i) {
        EXPECT_EQ(rf.read(i), 0u) << "x" << i << " not zero at init";
    }
}

TEST(RegFile, WriteThenRead) {
    RegFile rf;
    rf.write(5, 0xDEADBEEF);
    EXPECT_EQ(rf.read(5), 0xDEADBEEFu);
}

TEST(RegFile, X0IgnoresWrites) {
    RegFile rf;
    rf.write(0, 0x12345678);
    EXPECT_EQ(rf.read(0), 0u);
}

TEST(RegFile, X0StaysZeroAcrossManyWrites) {
    RegFile rf;
    for (std::uint32_t v : {1u, 0xFFFFFFFFu, 0x80000000u, 42u}) {
        rf.write(0, v);
        EXPECT_EQ(rf.read(0), 0u);
    }
}

TEST(RegFile, RegistersAreIndependent) {
    RegFile rf;
    rf.write(1, 100);
    rf.write(2, 200);
    rf.write(31, 300);
    EXPECT_EQ(rf.read(1), 100u);
    EXPECT_EQ(rf.read(2), 200u);
    EXPECT_EQ(rf.read(31), 300u);
}

TEST(RegFile, OverwriteUpdatesValue) {
    RegFile rf;
    rf.write(7, 1);
    rf.write(7, 2);
    EXPECT_EQ(rf.read(7), 2u);
}

TEST(RegFile, ResetClearsAllRegisters) {
    RegFile rf;
    for (std::size_t i = 1; i < 32; ++i) rf.write(i, i * 0x10);
    rf.reset();
    for (std::size_t i = 0; i < 32; ++i) EXPECT_EQ(rf.read(i), 0u);
}

TEST(RegFile, EqualityHoldsForFreshFiles) {
    RegFile a, b;
    EXPECT_EQ(a, b);
}

TEST(RegFile, EqualityDetectsDifference) {
    RegFile a, b;
    a.write(3, 42);
    EXPECT_NE(a, b);
    b.write(3, 42);
    EXPECT_EQ(a, b);
}

TEST(RegFile, X0WritesDontBreakEquality) {
    RegFile a, b;
    a.write(0, 0xFFFFFFFF);
    EXPECT_EQ(a, b);  // x0 stayed 0
}

TEST(RegFile, AbiNamesAreCorrect) {
    EXPECT_STREQ(cpusim::abi_name(0), "zero");
    EXPECT_STREQ(cpusim::abi_name(1), "ra");
    EXPECT_STREQ(cpusim::abi_name(2), "sp");
    EXPECT_STREQ(cpusim::abi_name(10), "a0");
    EXPECT_STREQ(cpusim::abi_name(15), "a5");
    EXPECT_STREQ(cpusim::abi_name(31), "t6");
}