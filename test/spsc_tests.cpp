#include "../Limit_Order_Book/spsc_queue.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

struct Pod {
    int x = 0;
    int y = 0;
};

}  // namespace

TEST(SPSCQueueTests, SingleProducerSingleConsumer_BasicEnqueueDequeue) {
    SPSCQueue<int, 8> q;

    EXPECT_TRUE(q.empty());
    EXPECT_FALSE(q.full());

    EXPECT_TRUE(q.enqueue(1));
    EXPECT_TRUE(q.enqueue(2));
    EXPECT_FALSE(q.empty());

    int v = 0;
    EXPECT_TRUE(q.dequeue(v));
    EXPECT_EQ(v, 1);
    EXPECT_TRUE(q.dequeue(v));
    EXPECT_EQ(v, 2);
    EXPECT_TRUE(q.empty());
}

TEST(SPSCQueueTests, RingWrapAndFullBehavior) {
    constexpr std::size_t N = 4;
    SPSCQueue<int, N> q;

    // Queue with capacity N uses N-1 actual elements.
    EXPECT_TRUE(q.enqueue(1));
    EXPECT_TRUE(q.enqueue(2));
    EXPECT_TRUE(q.enqueue(3));
    EXPECT_TRUE(q.full());
    EXPECT_FALSE(q.enqueue(4));

    int v = 0;
    EXPECT_TRUE(q.dequeue(v));
    EXPECT_EQ(v, 1);
    EXPECT_FALSE(q.full());

    EXPECT_TRUE(q.enqueue(4));  // wrap-around should work
    EXPECT_TRUE(q.full());
}

TEST(SPSCQueueTests, PodType_MoveAndCopy) {
    SPSCQueue<Pod, 8> q;
    Pod p1{1, 2};
    Pod p2{3, 4};

    EXPECT_TRUE(q.enqueue(p1));
    EXPECT_TRUE(q.enqueue(Pod{5, 6}));
    EXPECT_TRUE(q.enqueue(p2));

    Pod out{};
    EXPECT_TRUE(q.dequeue(out));
    EXPECT_EQ(out.x, 1);
    EXPECT_EQ(out.y, 2);

    EXPECT_TRUE(q.dequeue(out));
    EXPECT_EQ(out.x, 5);
    EXPECT_EQ(out.y, 6);

    EXPECT_TRUE(q.dequeue(out));
    EXPECT_EQ(out.x, 3);
    EXPECT_EQ(out.y, 4);
}

TEST(SPSCQueueTests, EmptyPopDoesNotModifyOutput) {
    SPSCQueue<int, 8> q;
    int v = 42;
    EXPECT_FALSE(q.dequeue(v));
    EXPECT_EQ(v, 42);
}

// Concurrent stress: start producer/consumer threads and push many items through.
TEST(SPSCQueueTests, ConcurrentStress) {
    constexpr std::size_t N = 1024;
    constexpr std::size_t kTotal = 2'000'000;

    SPSCQueue<std::uint64_t, N> q;
    std::atomic<bool> start{false};
    std::atomic<std::size_t> produced{0};
    std::atomic<std::size_t> consumed{0};
    std::atomic<bool> done{false};

    std::thread producer([&] {
        // wait for start signal
        while (!start.load(std::memory_order_acquire)) {
        }
        for (std::size_t i = 0; i < kTotal; ++i) {
            while (!q.enqueue(static_cast<std::uint64_t>(i))) {
                // spin until space is available
            }
            produced.fetch_add(1, std::memory_order_release);
        }
        done.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
        while (!start.load(std::memory_order_acquire)) {
        }

        std::uint64_t last = 0;
        bool first = true;
        while (!done.load(std::memory_order_acquire) || !q.empty()) {
            std::uint64_t v{};
            if (q.dequeue(v)) {
                if (!first) {
                    // sequence should be non-decreasing; strong guarantee of FIFO.
                    EXPECT_GE(v, last);
                }
                first = false;
                last = v;
                consumed.fetch_add(1, std::memory_order_release);
            } else {
                std::this_thread::yield();
            }
        }
    });

    start.store(true, std::memory_order_release);
    producer.join();
    consumer.join();

    EXPECT_EQ(produced.load(std::memory_order_acquire), kTotal);
    EXPECT_EQ(consumed.load(std::memory_order_acquire), kTotal);
}

