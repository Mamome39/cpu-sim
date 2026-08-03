#include <gtest/gtest.h>
#include "cpusim/memory/tree_plru.h"

using namespace cpusim;

// One way: the only way is always the victim, touch is a no-op.
TEST(TreePlru, SingleWayAlwaysVictimZero) {
    TreePlru p(/*sets=*/4, /*ways=*/1);
    EXPECT_EQ(p.victim(0), 0u);
    p.touch(0, 0);
    EXPECT_EQ(p.victim(0), 0u);
}

// Two way: touching a way steers the victim to the other one.
TEST(TreePlru, TwoWayPointsAwayFromTouched) {
    TreePlru p(4, 2);
    p.touch(1, 0);              // way 0 just used
    EXPECT_EQ(p.victim(1), 1u); // evict the other
    p.touch(1, 1);              // now way 1 just used
    EXPECT_EQ(p.victim(1), 0u);
}

// Four way: after using ways in order 0,1,2,3 then 1,2,3, way 0 is the
// pseudo-oldest and must be the victim.
TEST(TreePlru, FourWayEvictsPseudoOldest) {
    TreePlru p(2, 4);
    for (unsigned w : {0u, 1u, 2u, 3u}) p.touch(0, w);
    for (unsigned w : {1u, 2u, 3u})     p.touch(0, w);
    EXPECT_EQ(p.victim(0), 0u);

    // Touching the victim moves it off the victim path.
    p.touch(0, 0);
    EXPECT_NE(p.victim(0), 0u);
}

// Sets are independent — touching one set never changes another.
TEST(TreePlru, SetsAreIndependent) {
    TreePlru p(4, 2);
    unsigned before = p.victim(2);
    p.touch(0, 0);
    p.touch(1, 1);
    p.touch(3, 0);
    EXPECT_EQ(p.victim(2), before);
}
