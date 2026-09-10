#include "spsc_queue.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <thread>

namespace {

TEST(SpscQueue, ReportsEmptyAndFullStates) {
    utils::spsc_queue<int, 4> queue;
    int value{};

    EXPECT_FALSE(queue.pop(value));
    EXPECT_TRUE(queue.push(1));
    EXPECT_TRUE(queue.push(2));
    EXPECT_TRUE(queue.push(3));
    EXPECT_FALSE(queue.push(4));

    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 2);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 3);
    EXPECT_FALSE(queue.pop(value));
}

TEST(SpscQueue, PreservesOrderAcrossIndexWraparound) {
    utils::spsc_queue<int, 4> queue;
    int value{};

    ASSERT_TRUE(queue.push(1));
    ASSERT_TRUE(queue.push(2));
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 1);
    ASSERT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 2);

    ASSERT_TRUE(queue.push(3));
    ASSERT_TRUE(queue.push(4));
    ASSERT_TRUE(queue.push(5));
    EXPECT_FALSE(queue.push(6));

    for (const int expected : {3, 4, 5}) {
        ASSERT_TRUE(queue.pop(value));
        EXPECT_EQ(value, expected);
    }
    EXPECT_FALSE(queue.pop(value));
}

TEST(SpscQueue, TransfersValuesBetweenProducerAndConsumer) {
    constexpr int value_count = 100'000;
    utils::spsc_queue<int, 1024> queue;

    std::jthread producer{[&queue] {
        for (int value = 0; value < value_count; ++value) {
            while (!queue.push(value)) {
                std::this_thread::yield();
            }
        }
    }};

    for (int expected = 0; expected < value_count; ++expected) {
        int value{};
        while (!queue.pop(value)) {
            std::this_thread::yield();
        }
        EXPECT_EQ(value, expected);
    }
}

} // namespace
