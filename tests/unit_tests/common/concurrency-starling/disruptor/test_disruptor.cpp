#include <algorithm>
#include <array>
#include <barrier>
#include <chrono>
#include <future>
#include <latch>
#include <limits>
#include <memory>
#include <menagerie/starling>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace menagerie::starling;

namespace {
    /**
     * @brief Test fixture for static Disruptor tests
     */
    class DisruptorTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Common setup if needed
        }

        void TearDown() override {
            // Common cleanup if needed
        }
    };
}  // namespace

namespace {
    /**
     * @brief Test fixture for dynamic Disruptor tests
     */
    class DynamicDisruptorTest : public ::testing::Test {
    protected:
        void SetUp() override {
            // Common setup if needed
        }

        void TearDown() override {
            // Common cleanup if needed
        }
    };
}  // namespace

/* *==============================================================================
 * SEQUENCE TESTS - Cache-aligned atomic counters
 * *============================================================================*/

TEST_F(DisruptorTest, SequenceInitialization) {
    const Sequence seq{42};
    EXPECT_EQ(seq.get(), 42);

    const Sequence default_seq{};
    EXPECT_EQ(default_seq.get(), -1);  // Default is -1 (nothing published)
}

TEST_F(DisruptorTest, SequenceSetAndGet) {
    Sequence seq{0};
    seq.set(100);
    EXPECT_EQ(seq.get(), 100);

    seq.set(1000000);
    EXPECT_EQ(seq.get(), 1000000);
}

TEST_F(DisruptorTest, SequenceIncrementAndGet) {
    Sequence seq{0};

    EXPECT_EQ(seq.increment_and_get(), 1);
    EXPECT_EQ(seq.increment_and_get(), 2);
    EXPECT_EQ(seq.increment_and_get(), 3);
    EXPECT_EQ(seq.get(), 3);
}

TEST_F(DisruptorTest, SequenceAddAndGet) {
    Sequence seq{10};

    EXPECT_EQ(seq.add_and_get(5), 15);
    EXPECT_EQ(seq.add_and_get(10), 25);
    EXPECT_EQ(seq.get(), 25);
}

TEST_F(DisruptorTest, SequenceCompareAndSet) {
    Sequence seq{100};

    std::int64_t expected = 100;
    EXPECT_TRUE(seq.compare_and_set(expected, 200));
    EXPECT_EQ(seq.get(), 200);

    // CAS should fail if expected doesn't match
    expected = 100;  // Wrong value
    EXPECT_FALSE(seq.compare_and_set(expected, 300));
    EXPECT_EQ(expected, 200);   // Updated with actual value
    EXPECT_EQ(seq.get(), 200);  // Value unchanged
}

TEST_F(DisruptorTest, SequenceCacheLineAlignment) {
    // Verify cache-line alignment to prevent false sharing
    EXPECT_EQ(sizeof(Sequence), std::hardware_destructive_interference_size);
    EXPECT_EQ(alignof(Sequence), std::hardware_destructive_interference_size);

    // Verify multiple sequences don't share cache lines
    Sequence seq1{};
    Sequence seq2{};

    const auto addr1 = reinterpret_cast<uintptr_t>(&seq1);
    const auto addr2 = reinterpret_cast<uintptr_t>(&seq2);

    // They should be at least one destructive-interference span apart
    EXPECT_GE(std::abs(static_cast<long long>(addr2 - addr1)),
              static_cast<long long>(std::hardware_destructive_interference_size));
}

/*==============================================================================
 * RING BUFFER TESTS - Power-of-2 circular buffer
 *============================================================================*/

TEST_F(DisruptorTest, StaticRingBufferPowerOf2Sizing) {
    // These should compile (power of 2)
    StaticRingBuffer<int, 4> tiny_buffer;
    StaticRingBuffer<int, 1024> buffer;
    StaticRingBuffer<int, 16384> large_buffer;

    EXPECT_EQ(tiny_buffer.capacity(), 4);
    EXPECT_EQ(buffer.capacity(), 1024);
    EXPECT_EQ(large_buffer.capacity(), 16384);

    // This would fail to compile (not power of 2):
    // StaticRingBuffer<int, 1000> bad_buffer;  // Compile error!
}

TEST_F(DisruptorTest, StaticRingBufferIndexWrapping) {
    constexpr size_t SIZE = 8;
    StaticRingBuffer<int, SIZE> buffer;

    // Test wrapping behavior
    for (std::int64_t seq = 0; seq < 100; ++seq) {
        buffer[seq] = static_cast<int>(seq);
    }

    // Verify wrap-around: sequences 0, 8, 16, 24... map to index 0
    EXPECT_EQ(buffer[0], 96);  // Overwritten by seq 96 (96 % 8 = 0)
    EXPECT_EQ(buffer[8], 96);  // Same slot!
    EXPECT_EQ(buffer[16], 96);
    EXPECT_EQ(buffer[96], 96);

    // Verify different slots
    EXPECT_EQ(buffer[1], 97);   // seq 97 % 8 = 1
    EXPECT_EQ(buffer[99], 99);  // seq 99 % 8 = 3
}

TEST_F(DisruptorTest, StaticRingBufferSequentialAccess) {
    StaticRingBuffer<int, 16> buffer;

    // Write sequential data
    for (std::int64_t i = 0; i < 16; ++i) {
        buffer[i] = static_cast<int>(i * 10);
    }

    // Read back
    for (std::int64_t i = 0; i < 16; ++i) {
        EXPECT_EQ(buffer[i], i * 10);
    }
}

TEST_F(DisruptorTest, StaticRingBufferGetVsOperator) {
    StaticRingBuffer<int, 8> buffer;

    buffer.get(5) = 100;
    EXPECT_EQ(buffer[5], 100);
    EXPECT_EQ(buffer.get(5), 100);
}

/*==============================================================================
 * WAIT STRATEGY TESTS
 *============================================================================*/

TEST_F(DisruptorTest, BusySpinWaitStrategyNoBlock) {
    constexpr BusySpinWaitStrategy strategy;
    const Sequence cursor{10};

    // Requesting sequence <= cursor should return immediately
    const auto start          = std::chrono::steady_clock::now();
    const std::int64_t result = strategy.wait_for(5, cursor);
    const auto elapsed        = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(result, 10);
    EXPECT_LT(elapsed, std::chrono::microseconds{100});  // Should be nearly instant
}

TEST_F(DisruptorTest, YieldingWaitStrategyNoBlock) {
    constexpr YieldingWaitStrategy strategy;
    const Sequence cursor{10};

    const std::int64_t result = strategy.wait_for(5, cursor);
    EXPECT_EQ(result, 10);
}

TEST_F(DisruptorTest, BlockingWaitStrategyNoBlock) {
    const BlockingWaitStrategy strategy;
    const Sequence cursor{10};

    const std::int64_t result = strategy.wait_for(5, cursor);
    EXPECT_EQ(result, 10);
}

TEST_F(DisruptorTest, BlockingWaitStrategyWithSignal) {
    const BlockingWaitStrategy strategy;
    Sequence cursor{0};

    std::atomic<bool> consumer_ready{false};
    std::atomic<std::int64_t> result{-1};

    // Consumer thread - waits for sequence 10
    std::thread consumer{[&]() {
        consumer_ready.store(true);
        result.store(strategy.wait_for(10, cursor));
    }};

    // Wait for consumer to start waiting
    while (!consumer_ready.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{50});

    // Producer publishes sequence 10
    cursor.set(10);
    strategy.signal();

    consumer.join();
    EXPECT_EQ(result.load(), 10);
}

/*==============================================================================
 * DISRUPTOR WRAPPER TESTS - Static compile-time sized
 *============================================================================*/

TEST_F(DisruptorTest, DisruptorSingleClaim) {
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    EXPECT_EQ(disruptor.sequencer().approx_size(), 0);

    const std::int64_t seq1 = disruptor.sequencer().next();
    EXPECT_EQ(seq1, 0);

    const std::int64_t seq2 = disruptor.sequencer().next();
    EXPECT_EQ(seq2, 1);

    const std::int64_t seq3 = disruptor.sequencer().next();
    EXPECT_EQ(seq3, 2);
}

TEST_F(DisruptorTest, DisruptorBatchClaim) {
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    const std::int64_t first = disruptor.sequencer().next_batch(5);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 5);  // Claimed 0-4

    const std::int64_t second = disruptor.sequencer().next_batch(3);
    EXPECT_EQ(second, 5);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 8);  // Claimed 5-7
}

TEST_F(DisruptorTest, DisruptorPublishAndAvailability) {
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    const std::int64_t seq = disruptor.sequencer().next();
    EXPECT_FALSE(disruptor.sequencer().is_available(seq));  // Not published yet

    disruptor.sequencer().publish(seq);
    EXPECT_TRUE(disruptor.sequencer().is_available(seq));  // Now available
}

TEST_F(DisruptorTest, DisruptorGapDetection) {
    /**
     * Test the critical gap detection logic:
     * - Thread A claims seq 0
     * - Thread B claims seq 1
     * - Thread B publishes seq 1 FIRST
     * - Consumer should NOT see seq 1 until seq 0 is published
     */
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    const std::int64_t seq0 = disruptor.sequencer().next();  // 0
    const std::int64_t seq1 = disruptor.sequencer().next();  // 1
    const std::int64_t seq2 = disruptor.sequencer().next();  // 2

    EXPECT_EQ(seq0, 0);
    EXPECT_EQ(seq1, 1);
    EXPECT_EQ(seq2, 2);

    // Publish out of order: 1, 2 (skip 0)
    disruptor.sequencer().publish(seq1);
    disruptor.sequencer().publish(seq2);

    // The published frontier must stop before the gap at seq0.
    std::int64_t highest = disruptor.sequencer().get_published_sequence(0);
    EXPECT_EQ(highest, -1);  // Gap at 0, so return -1 (0 - 1)

    // Now publish seq0
    disruptor.sequencer().publish(seq0);

    // Now all are available
    highest = disruptor.sequencer().get_published_sequence(0);
    EXPECT_EQ(highest, 2);  // All sequences 0-2 available
}

TEST_F(DisruptorTest, DisruptorBackpressure) {
    /**
     * Test backpressure: when buffer is full, next() should block
     * until the consumer releases completed reads
     */
    constexpr size_t BUFFER_SIZE = 8;
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    // Fill the buffer (claim 8 sequences)
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        std::ignore = disruptor.sequencer().next();
    }

    // Next claim would wrap around and overwrite seq 0
    // Should block until we consume a sequence
    std::atomic<bool> blocked{true};
    std::atomic<std::int64_t> claimed_seq{-1};

    std::thread producer{[&]() {
        claimed_seq.store(disruptor.sequencer().next());  // This should block
        blocked.store(false);
    }};

    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    EXPECT_TRUE(blocked.load());  // Should still be blocked

    // Consumer advances (consumed seq 0)
    disruptor.sequencer().consume(0);

    // Now producer should unblock
    producer.join();
    EXPECT_FALSE(blocked.load());
    EXPECT_EQ(claimed_seq.load(), 8);  // Successfully claimed next sequence
}

TEST_F(DisruptorTest, DisruptorRemainingCapacity) {
    constexpr size_t BUFFER_SIZE = 16;
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    // Initially, full capacity available
    EXPECT_EQ(disruptor.sequencer().remaining_capacity(), BUFFER_SIZE);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 0);

    // Claim 5 sequences
    for (int i = 0; i < 5; ++i) {
        std::ignore = disruptor.sequencer().next();
    }

    EXPECT_EQ(disruptor.sequencer().remaining_capacity(), BUFFER_SIZE - 5);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 5);

    // Consumer processes 3
    disruptor.sequencer().consume_batch(0, 2);

    EXPECT_EQ(disruptor.sequencer().remaining_capacity(), BUFFER_SIZE - 2);  // 3 consumed, 2 still pending
    EXPECT_EQ(disruptor.sequencer().approx_size(), 2);
}

/*==============================================================================
 * MULTI-THREADED ORDERING TESTS - The Critical Test!
 *============================================================================*/

namespace {
    struct TestEntry {
        std::int64_t sequence;
        std::int64_t thread_id;
        std::int64_t timestamp_ns;
    };
}  // namespace

TEST_F(DisruptorTest, MultiThreadedStrictOrdering) {
    /**
     * THE MOST IMPORTANT TEST: Verify strict ordering with multiple producers
     *
     * Setup:
     * - 4 producer threads, each publishing 1000 entries
     * - 1 consumer thread processing in order
     * - Consumer must see ALL 4000 entries in strict sequence order
     */
    constexpr size_t BUFFER_SIZE       = 1024;
    constexpr int NUM_PRODUCERS        = 4;
    constexpr int ENTRIES_PER_PRODUCER = 1000;
    constexpr int TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    Disruptor<TestEntry, MultiProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    std::atomic<bool> start{false};
    std::atomic<int> ready_count{0};

    // Consumer results
    std::vector<TestEntry> consumed_entries;
    consumed_entries.reserve(TOTAL_ENTRIES);

    // Consumer thread
    std::thread consumer{[&]() {
        std::int64_t next_sequence = 0;
        int processed              = 0;

        while (processed < TOTAL_ENTRIES) {
            const std::int64_t published = disruptor.sequencer().get_published_sequence(next_sequence);

            if (published < next_sequence) {
                std::this_thread::yield();
                continue;
            }

            if (const std::int64_t available = published; available >= next_sequence) {
                // Process batch
                for (std::int64_t seq = next_sequence; seq <= available; ++seq) {
                    consumed_entries.push_back(disruptor.ring_buffer()[seq]);
                    ++processed;
                }

                disruptor.sequencer().consume_batch(next_sequence, available);
                next_sequence = available + 1;
            } else {
                // No data available, yield
                std::this_thread::yield();
            }
        }
    }};

    // Producer threads
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&, tid]() {
            ready_count.fetch_add(1);

            // Wait for all producers to be ready
            while (!start.load()) {
                std::this_thread::yield();
            }

            // Publish entries
            for (int i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
                const std::int64_t seq = disruptor.sequencer().next();

                // Write entry
                disruptor.ring_buffer()[seq].sequence  = seq;
                disruptor.ring_buffer()[seq].thread_id = tid;
                disruptor.ring_buffer()[seq].timestamp_ns =
                    static_cast<std::int64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

                // Publish
                disruptor.sequencer().publish(seq);

                // Small delay to increase chance of out-of-order publishing
                if (i % 100 == 0) {
                    // std::this_thread::yield();
                }
            }
        });
    }

    // Wait for all producers to be ready
    while (ready_count.load() < NUM_PRODUCERS) {
        std::this_thread::yield();
    }

    // Start!
    start.store(true);

    // Wait for completion
    for (auto& p : producers) {
        p.join();
    }
    consumer.join();

    // VERIFY RESULTS
    ASSERT_EQ(consumed_entries.size(), TOTAL_ENTRIES);

    // Check strict sequence ordering
    for (size_t i = 0; i < consumed_entries.size(); ++i) {
        EXPECT_EQ(consumed_entries[i].sequence, static_cast<std::int64_t>(i))
            << "Entry at position " << i << " has wrong sequence number";
    }

    // Verify all thread IDs are present
    std::array<int, NUM_PRODUCERS> counts{};
    for (const auto& entry : consumed_entries) {
        ASSERT_GE(entry.thread_id, 0);
        ASSERT_LT(entry.thread_id, NUM_PRODUCERS);
        counts[static_cast<std::size_t>(entry.thread_id)]++;
    }

    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        EXPECT_EQ(counts[static_cast<std::size_t>(tid)], ENTRIES_PER_PRODUCER)
            << "Thread " << tid << " published wrong number of entries";
    }
}

TEST_F(DisruptorTest, MultiThreadedHighContention) {
    /**
     * Stress test with many threads and high contention
     */
    constexpr size_t BUFFER_SIZE       = 512;
    constexpr int NUM_PRODUCERS        = 8;
    constexpr int ENTRIES_PER_PRODUCER = 500;
    constexpr int TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    Disruptor<std::int64_t, MultiProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    std::barrier sync_point{NUM_PRODUCERS + 1};  // +1 for consumer
    std::vector<std::int64_t> consumed;
    consumed.reserve(TOTAL_ENTRIES);

    // Consumer
    std::thread consumer{[&]() {
        sync_point.arrive_and_wait();

        std::int64_t next_seq = 0;
        while (consumed.size() < TOTAL_ENTRIES) {
            if (const std::int64_t available = disruptor.sequencer().get_published_sequence(next_seq);
                available >= next_seq) {
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    consumed.push_back(disruptor.ring_buffer()[seq]);
                }
                disruptor.sequencer().consume_batch(next_seq, available);
                next_seq = available + 1;
            } else {
                std::this_thread::yield();
            }
        }
    }};

    // Producers
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (int tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&]() {
            sync_point.arrive_and_wait();

            for (int i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
                const std::int64_t seq       = disruptor.sequencer().next();
                disruptor.ring_buffer()[seq] = seq;
                disruptor.sequencer().publish(seq);
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }
    consumer.join();

    // Verify strict ordering
    ASSERT_EQ(consumed.size(), TOTAL_ENTRIES);
    for (size_t i = 0; i < consumed.size(); ++i) {
        EXPECT_EQ(consumed[i], static_cast<std::int64_t>(i));
    }
}

TEST_F(DisruptorTest, TryNextNonBlocking) {
    /**
     * Test non-blocking try_next() behavior
     */
    constexpr size_t BUFFER_SIZE = 4;
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    // Fill buffer
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        std::int64_t seq = disruptor.sequencer().try_next();
        EXPECT_NE(seq, -1);  // Should succeed
    }

    // Next try_next should fail (buffer full)
    std::int64_t seq = disruptor.sequencer().try_next();
    EXPECT_EQ(seq, -1);  // Should fail without blocking

    // Advance consumer
    disruptor.sequencer().consume_batch(0, 1);

    // Now should succeed again
    seq = disruptor.sequencer().try_next();
    EXPECT_NE(seq, -1);  // Should succeed
}

/*==============================================================================
 * DYNAMIC DISRUPTOR TESTS - Runtime-sized version
 *============================================================================*/

TEST_F(DynamicDisruptorTest, DynamicStaticRingBufferSizing) {
    // Runtime-sized buffers
    const RingBuffer<int> small_buffer{256};
    const RingBuffer<int> medium_buffer{1024};
    const RingBuffer<int> large_buffer{8192};

    EXPECT_EQ(small_buffer.capacity(), 256);
    EXPECT_EQ(medium_buffer.capacity(), 1024);
    EXPECT_EQ(large_buffer.capacity(), 8192);
}

TEST_F(DynamicDisruptorTest, DynamicStaticRingBufferIndexWrapping) {
    constexpr size_t SIZE = 8;
    RingBuffer<int> buffer{SIZE};

    // Test wrapping behavior
    for (std::int64_t seq = 0; seq < 100; ++seq) {
        buffer[seq] = static_cast<int>(seq);
    }

    // Verify wrap-around: sequences 0, 8, 16, 24... map to index 0
    EXPECT_EQ(buffer[0], 96);  // Overwritten by seq 96 (96 % 8 = 0)
    EXPECT_EQ(buffer[8], 96);  // Same slot!
    EXPECT_EQ(buffer[16], 96);
    EXPECT_EQ(buffer[96], 96);

    // Verify different slots
    EXPECT_EQ(buffer[1], 97);   // seq 97 % 8 = 1
    EXPECT_EQ(buffer[99], 99);  // seq 99 % 8 = 3
}

TEST_F(DynamicDisruptorTest, DynamicDisruptorSingleClaim) {
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    EXPECT_EQ(disruptor.sequencer().approx_size(), 0);

    const std::int64_t seq1 = disruptor.sequencer().next();
    EXPECT_EQ(seq1, 0);

    const std::int64_t seq2 = disruptor.sequencer().next();
    EXPECT_EQ(seq2, 1);

    const std::int64_t seq3 = disruptor.sequencer().next();
    EXPECT_EQ(seq3, 2);
}

TEST_F(DynamicDisruptorTest, DynamicDisruptorBatchClaim) {
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    const std::int64_t first = disruptor.sequencer().next_batch(5);
    EXPECT_EQ(first, 0);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 5);  // Claimed 0-4

    const std::int64_t second = disruptor.sequencer().next_batch(3);
    EXPECT_EQ(second, 5);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 8);  // Claimed 5-7
}

TEST_F(DynamicDisruptorTest, DynamicDisruptorGapDetection) {
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    const std::int64_t seq0 = disruptor.sequencer().next();  // 0
    const std::int64_t seq1 = disruptor.sequencer().next();  // 1
    const std::int64_t seq2 = disruptor.sequencer().next();  // 2

    EXPECT_EQ(seq0, 0);
    EXPECT_EQ(seq1, 1);
    EXPECT_EQ(seq2, 2);

    // Publish out of order: 1, 2 (skip 0)
    disruptor.sequencer().publish(seq1);
    disruptor.sequencer().publish(seq2);

    // The published frontier must stop before the gap at seq0.
    std::int64_t highest = disruptor.sequencer().get_published_sequence(0);
    EXPECT_EQ(highest, -1);  // Gap at 0, so return -1 (0 - 1)

    // Now publish seq0
    disruptor.sequencer().publish(seq0);

    // Now all are available
    highest = disruptor.sequencer().get_published_sequence(0);
    EXPECT_EQ(highest, 2);  // All sequences 0-2 available
}

TEST_F(DynamicDisruptorTest, DynamicDisruptorBackpressure) {
    constexpr size_t BUFFER_SIZE = 8;
    Disruptor<int, MultiProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    // Fill the buffer (claim 8 sequences)
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        std::ignore = disruptor.sequencer().next();
    }

    // Next claim would wrap around and overwrite seq 0
    // Should block until we consume a sequence
    std::atomic<bool> blocked{true};
    std::atomic<std::int64_t> claimed_seq{-1};

    std::thread producer{[&]() {
        claimed_seq.store(disruptor.sequencer().next());  // This should block
        blocked.store(false);
    }};

    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    EXPECT_TRUE(blocked.load());  // Should still be blocked

    // Consumer advances (consumed seq 0)
    disruptor.sequencer().consume(0);

    // Now producer should unblock
    producer.join();
    EXPECT_FALSE(blocked.load());
    EXPECT_EQ(claimed_seq.load(), 8);  // Successfully claimed next sequence
}

TEST_F(DynamicDisruptorTest, DynamicDisruptorMultiThreadedOrdering) {
    constexpr size_t BUFFER_SIZE       = 1024;
    constexpr int NUM_PRODUCERS        = 4;
    constexpr int ENTRIES_PER_PRODUCER = 1000;
    constexpr int TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    Disruptor<TestEntry, MultiProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    std::atomic<bool> start{false};
    std::atomic<int> ready_count{0};

    // Consumer results
    std::vector<TestEntry> consumed_entries;
    consumed_entries.reserve(TOTAL_ENTRIES);

    // Consumer thread
    std::thread consumer{[&]() {
        std::int64_t next_sequence = 0;
        int processed              = 0;

        while (processed < TOTAL_ENTRIES) {
            const std::int64_t published = disruptor.sequencer().get_published_sequence(next_sequence);

            if (published < next_sequence) {
                std::this_thread::yield();
                continue;
            }

            if (const std::int64_t available = published; available >= next_sequence) {
                // Process batch
                for (std::int64_t seq = next_sequence; seq <= available; ++seq) {
                    consumed_entries.push_back(disruptor.ring_buffer()[seq]);
                    ++processed;
                }

                disruptor.sequencer().consume_batch(next_sequence, available);
                next_sequence = available + 1;
            } else {
                // No data available, yield
                std::this_thread::yield();
            }
        }
    }};

    // Producer threads
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&, tid]() {
            ready_count.fetch_add(1);

            // Wait for all producers to be ready
            while (!start.load()) {
                std::this_thread::yield();
            }

            // Publish entries
            for (int i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
                const std::int64_t seq = disruptor.sequencer().next();

                // Write entry
                disruptor.ring_buffer()[seq].sequence  = seq;
                disruptor.ring_buffer()[seq].thread_id = tid;
                disruptor.ring_buffer()[seq].timestamp_ns =
                    static_cast<std::int64_t>(std::chrono::steady_clock::now().time_since_epoch().count());

                // Publish
                disruptor.sequencer().publish(seq);
            }
        });
    }

    // Wait for all producers to be ready
    while (ready_count.load() < NUM_PRODUCERS) {
        std::this_thread::yield();
    }

    // Start!
    start.store(true);

    // Wait for completion
    for (auto& p : producers) {
        p.join();
    }
    consumer.join();

    // VERIFY RESULTS
    ASSERT_EQ(consumed_entries.size(), TOTAL_ENTRIES);

    // Check strict sequence ordering
    for (size_t i = 0; i < consumed_entries.size(); ++i) {
        EXPECT_EQ(consumed_entries[i].sequence, static_cast<std::int64_t>(i))
            << "Entry at position " << i << " has wrong sequence number";
    }

    // Verify all thread IDs are present
    std::vector<int> counts(NUM_PRODUCERS, 0);
    for (const auto& entry : consumed_entries) {
        ASSERT_GE(entry.thread_id, 0);
        ASSERT_LT(entry.thread_id, NUM_PRODUCERS);
        counts[static_cast<std::size_t>(entry.thread_id)]++;
    }

    for (std::int64_t tid = 0; tid < NUM_PRODUCERS; ++tid) {
        EXPECT_EQ(counts[static_cast<std::size_t>(tid)], ENTRIES_PER_PRODUCER)
            << "Thread " << tid << " published wrong number of entries";
    }
}

TEST_F(DynamicDisruptorTest, DynamicDisruptorHighContention) {
    constexpr size_t BUFFER_SIZE       = 512;
    constexpr int NUM_PRODUCERS        = 8;
    constexpr int ENTRIES_PER_PRODUCER = 500;
    constexpr int TOTAL_ENTRIES        = NUM_PRODUCERS * ENTRIES_PER_PRODUCER;

    Disruptor<std::int64_t, MultiProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    std::barrier sync_point{NUM_PRODUCERS + 1};  // +1 for consumer
    std::vector<std::int64_t> consumed;
    consumed.reserve(TOTAL_ENTRIES);

    // Consumer
    std::thread consumer{[&]() {
        sync_point.arrive_and_wait();

        std::int64_t next_seq = 0;
        while (consumed.size() < TOTAL_ENTRIES) {
            if (const std::int64_t available = disruptor.sequencer().get_published_sequence(next_seq);
                available >= next_seq) {
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    consumed.push_back(disruptor.ring_buffer()[seq]);
                }
                disruptor.sequencer().consume_batch(next_seq, available);
                next_seq = available + 1;
            } else {
                std::this_thread::yield();
            }
        }
    }};

    // Producers
    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (int tid = 0; tid < NUM_PRODUCERS; ++tid) {
        producers.emplace_back([&]() {
            sync_point.arrive_and_wait();

            for (int i = 0; i < ENTRIES_PER_PRODUCER; ++i) {
                const std::int64_t seq       = disruptor.sequencer().next();
                disruptor.ring_buffer()[seq] = seq;
                disruptor.sequencer().publish(seq);
            }
        });
    }

    for (auto& p : producers) {
        p.join();
    }
    consumer.join();

    // Verify strict ordering
    ASSERT_EQ(consumed.size(), TOTAL_ENTRIES);
    for (size_t i = 0; i < consumed.size(); ++i) {
        EXPECT_EQ(consumed[i], static_cast<std::int64_t>(i));
    }
}

/*==============================================================================
 * SINGLE-PRODUCER SEQUENCER TESTS - SPSC fast path
 *
 * get_published_sequence() advances on publication, never merely on a claim.
 *============================================================================*/

namespace {
    class SingleProducerDisruptorTest : public ::testing::Test {};
}  // namespace

TEST_F(SingleProducerDisruptorTest, SingleClaim) {
    Disruptor<int, SingleProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    EXPECT_EQ(disruptor.sequencer().get_published_sequence(0), -1);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 0);
    EXPECT_EQ(disruptor.sequencer().next(), 0);
    EXPECT_EQ(disruptor.sequencer().next(), 1);
    EXPECT_EQ(disruptor.sequencer().next(), 2);
    EXPECT_EQ(disruptor.sequencer().get_published_sequence(0), -1);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 0);
}

TEST_F(SingleProducerDisruptorTest, BatchClaim) {
    Disruptor<int, SingleProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    EXPECT_EQ(disruptor.sequencer().next_batch(5), 0);  // claims 0..4, returns first
    EXPECT_EQ(disruptor.sequencer().next_batch(3), 5);  // claims 5..7, returns first
}

TEST_F(SingleProducerDisruptorTest, MixedClaimsRespectPartiallyReleasedCapacity) {
    SingleProducerSequencer<BusySpinWaitStrategy> sequencer{4};
    EXPECT_EQ(sequencer.next(), 0);
    EXPECT_EQ(sequencer.try_next(), 1);
    EXPECT_EQ(sequencer.next_batch(2), 2);
    sequencer.publish_batch(0, 3);
    EXPECT_EQ(sequencer.try_next(), -1);

    sequencer.consume_batch(0, 1);
    EXPECT_EQ(sequencer.next_batch(2), 4);
    sequencer.publish_batch(4, 5);
    EXPECT_EQ(sequencer.try_next(), -1);

    sequencer.consume(2);
    EXPECT_EQ(sequencer.try_next(), 6);
    sequencer.publish(6);
    EXPECT_EQ(sequencer.try_next(), -1);

    sequencer.consume_batch(3, 5);
    EXPECT_EQ(sequencer.next_batch(3), 7);
    EXPECT_EQ(sequencer.try_next(), -1);
}

TEST_F(SingleProducerDisruptorTest, CapacityCacheRemainsValidNearSequenceLimit) {
    // Exercise sequence arithmetic without allocating a ring of this size.
    constexpr auto capacity = std::int64_t{1} << 62;
    constexpr auto maximum  = std::numeric_limits<std::int64_t>::max();
    SingleProducerSequencer<BusySpinWaitStrategy> sequencer{static_cast<std::size_t>(capacity)};
    EXPECT_EQ(sequencer.next_batch(capacity), 0);
    sequencer.publish(capacity - 1);
    sequencer.consume_batch(0, 1);
    EXPECT_EQ(sequencer.next_batch(2), capacity);
    sequencer.publish(capacity + 1);
    sequencer.consume_batch(2, capacity + 1);

    // A cached consumed + capacity threshold now exceeds INT64_MAX, although
    // all sequences being claimed still fit. The threshold must not overflow.
    EXPECT_EQ(sequencer.next_batch(capacity - 3), capacity + 2);
    EXPECT_EQ(sequencer.next(), maximum);
}

TEST_F(SingleProducerDisruptorTest, PublishAndAvailability) {
    Disruptor<int, SingleProducerSequencer, YieldingWaitStrategy> disruptor{1024};

    const std::int64_t seq = disruptor.sequencer().next();
    EXPECT_FALSE(disruptor.sequencer().is_available(seq));  // claimed, not published

    disruptor.ring_buffer()[seq] = 42;
    disruptor.sequencer().publish(seq);

    EXPECT_TRUE(disruptor.sequencer().is_available(seq));
    EXPECT_EQ(disruptor.sequencer().get_published_sequence(0), seq);
    EXPECT_EQ(disruptor.sequencer().approx_size(), 1);
}

TEST_F(SingleProducerDisruptorTest, Backpressure) {
    constexpr size_t BUFFER_SIZE = 8;
    Disruptor<int, SingleProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    // Fill the buffer (claim 8 sequences)
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        std::ignore = disruptor.sequencer().next();
    }

    std::atomic<bool> blocked{true};
    std::atomic<std::int64_t> claimed_seq{-1};
    std::thread producer{[&]() {
        claimed_seq.store(disruptor.sequencer().next());  // 9th claim — should block
        blocked.store(false);
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    EXPECT_TRUE(blocked.load());  // still blocked

    disruptor.sequencer().consume(0);  // consumed seq 0
    producer.join();
    EXPECT_FALSE(blocked.load());
    EXPECT_EQ(claimed_seq.load(), 8);
}

TEST_F(SingleProducerDisruptorTest, SingleProducerSingleConsumerOrdering) {
    constexpr size_t BUFFER_SIZE = 1024;
    constexpr int TOTAL_ENTRIES  = 100'000;
    Disruptor<std::int64_t, SingleProducerSequencer, YieldingWaitStrategy> disruptor{BUFFER_SIZE};

    std::vector<std::int64_t> consumed;
    consumed.reserve(TOTAL_ENTRIES);

    std::thread consumer{[&]() {
        std::int64_t next_seq = 0;
        while (static_cast<int>(consumed.size()) < TOTAL_ENTRIES) {
            if (const std::int64_t available = disruptor.sequencer().get_published_sequence(next_seq);
                available >= next_seq) {
                for (std::int64_t seq = next_seq; seq <= available; ++seq) {
                    consumed.push_back(disruptor.ring_buffer()[seq]);
                }
                disruptor.sequencer().consume_batch(next_seq, available);
                next_seq = available + 1;
            } else {
                std::this_thread::yield();
            }
        }
    }};

    for (int i = 0; i < TOTAL_ENTRIES; ++i) {
        const std::int64_t seq       = disruptor.sequencer().next();
        disruptor.ring_buffer()[seq] = seq;
        disruptor.sequencer().publish(seq);
    }
    consumer.join();

    ASSERT_EQ(consumed.size(), TOTAL_ENTRIES);
    for (size_t i = 0; i < consumed.size(); ++i) {
        EXPECT_EQ(consumed[i], static_cast<std::int64_t>(i));
    }
}

namespace {
    struct QueueMessage {
        int id{};
        std::unique_ptr<int> payload;

        QueueMessage() = default;
        QueueMessage(const int message_id, std::unique_ptr<int> data) noexcept
            : id{message_id},
              payload{std::move(data)} {
        }
    };

    struct ThrowingMessage {
        int value{};
        ThrowingMessage() = default;
        explicit ThrowingMessage(const int input) {
            if (input < 0)
                throw std::invalid_argument("negative message");
            value = input;
        }
    };

    struct DelayedMessage {
        int value{};
        DelayedMessage() = default;
        DelayedMessage(const int input, std::latch* claimed, const std::latch* release) noexcept {
            if (claimed) {
                claimed->count_down();
                release->wait();
            }
            value = input;
        }
    };

    struct ThrowingMove {
        ThrowingMove()                    = default;
        ThrowingMove(const ThrowingMove&) = delete;
        ThrowingMove(ThrowingMove&&) noexcept(false) {
        }
    };
    template <typename Queue>
    concept HasPull = requires(Queue& queue) { queue.pull(); };
    static_assert(!HasPull<Disruptor<ThrowingMove>>);

    template <template <typename> class Sequencer>
    void check_emplace_failure() {
        Disruptor<ThrowingMessage, Sequencer> queue{2};
        EXPECT_THROW(queue.emplace(-1), std::invalid_argument);
        EXPECT_EQ(queue.sequencer().get_published_sequence(0), -1);
        EXPECT_EQ(queue.sequencer().approx_size(), 0);
        EXPECT_EQ(queue.sequencer().remaining_capacity(), 2);
        queue.emplace(17);
        EXPECT_EQ(queue.pull().value, 17);
        queue.emplace(29);
        EXPECT_EQ(queue.pull().value, 29);
    }

    template <typename WaitStrategy>
    void check_wait_widths() {
        const Sequence narrow{7};
        const WideSequence wide{11};
        auto wait = AnyWaitStrategy::make<WaitStrategy>();
        EXPECT_EQ(wait.wait_for(3, narrow), 7);
        EXPECT_EQ(wait.wait_for(3, wide), 11);
    }
}  // namespace

TEST(DisruptorQueueTest, ConstructsMoveOnlyMessagesAndPreservesFIFOAcrossWrap) {
    Disruptor<QueueMessage, SingleProducerSequencer> queue{2};
    queue.emplace(1, std::make_unique<int>(10));
    queue.emplace(2, std::make_unique<int>(20));
    const auto first = queue.pull();
    ASSERT_TRUE(first.payload);
    EXPECT_EQ(first.id, 1);
    EXPECT_EQ(*first.payload, 10);
    queue.emplace(3, std::make_unique<int>(30));
    const auto second = queue.pull();
    const auto third  = queue.pull();
    ASSERT_TRUE(second.payload);
    ASSERT_TRUE(third.payload);
    EXPECT_EQ(second.id, 2);
    EXPECT_EQ(*second.payload, 20);
    EXPECT_EQ(third.id, 3);
    EXPECT_EQ(*third.payload, 30);
}

TEST(DisruptorQueueTest, ConstructorFailureDoesNotLeaveUnpublishedClaims) {
    check_emplace_failure<SingleProducerSequencer>();
    check_emplace_failure<MultiProducerSequencer>();
}

TEST(DisruptorQueueTest, TransfersOwnershipAndDestroysQueuedResources) {
    std::weak_ptr<int> pending;
    {
        Disruptor<std::shared_ptr<int>> queue{1};
        auto first                        = std::make_shared<int>(42);
        const std::weak_ptr<int> consumed = first;
        queue.emplace(std::move(first));
        {
            const auto value = queue.pull();
            EXPECT_EQ(value.use_count(), 1);
            EXPECT_EQ(*value, 42);
        }
        EXPECT_TRUE(consumed.expired());
        auto second = std::make_shared<int>(99);
        pending     = second;
        queue.emplace(std::move(second));
        EXPECT_FALSE(pending.expired());
    }
    EXPECT_TRUE(pending.expired());
}

TEST(DisruptorQueueTest, PullConsumesManuallyPublishedProducerBatches) {
    Disruptor<int, SingleProducerSequencer> queue{4};
    const auto first               = queue.sequencer().next_batch(2);
    queue.ring_buffer()[first]     = 11;
    queue.ring_buffer()[first + 1] = 22;
    queue.sequencer().publish_batch(first, first + 1);
    queue.emplace(33);
    EXPECT_EQ(queue.pull(), 11);
    EXPECT_EQ(queue.pull(), 22);
    EXPECT_EQ(queue.pull(), 33);
}

TEST(DisruptorQueueTest, PullAndManualConsumptionShareOnePosition) {
    Disruptor<int, SingleProducerSequencer> queue{4};
    queue.emplace(11);
    queue.emplace(22);
    queue.emplace(33);
    queue.emplace(44);

    // Manual consumption can precede the first convenience pull.
    EXPECT_EQ(queue.ring_buffer()[0], 11);
    queue.sequencer().consume(0);
    EXPECT_EQ(queue.pull(), 22);

    // It can also advance beyond a position already cached by pull().
    EXPECT_EQ(queue.ring_buffer()[2], 33);
    queue.sequencer().consume(2);
    EXPECT_EQ(queue.pull(), 44);
    EXPECT_EQ(queue.sequencer().get_consumed_sequence(), 3);
}

TEST(DisruptorQueueTest, MultipleProducersDeliverEachStreamInOrder) {
    struct Event {
        std::uint64_t producer{};
        std::uint64_t index{};
    };
    constexpr std::uint64_t producer_count = 3;
    constexpr std::uint64_t per_producer   = 20'000;
    Disruptor<Event> queue{32};
    std::barrier start{4};
    std::uint64_t errors = 0;
    std::array<std::uint64_t, producer_count> received{};
    std::thread consumer{[&] {
        start.arrive_and_wait();
        for (std::uint64_t i = 0; i < producer_count * per_producer; ++i) {
            if (const auto [producer, index] = queue.pull(); producer >= producer_count) {
                ++errors;
            } else {
                errors += static_cast<std::uint64_t>(index != received[producer]++);
            }
        }
    }};
    std::vector<std::thread> producers;
    producers.reserve(producer_count);
    for (std::uint64_t id = 0; id < producer_count; ++id) {
        producers.emplace_back([&, id] {
            start.arrive_and_wait();
            for (std::uint64_t i = 0; i < per_producer; ++i)
                queue.emplace(id, i);
        });
    }
    for (auto& producer : producers)
        producer.join();
    consumer.join();
    EXPECT_EQ(errors, 0);
    for (const auto count : received)
        EXPECT_EQ(count, per_producer);
}

TEST(DisruptorQueueTest, BlockingPullWakesAfterPublication) {
    Disruptor<int, SingleProducerSequencer, BlockingWaitStrategy> queue{1};
    int errors = 0;
    std::thread consumer{[&] {
        for (int i = 0; i < 1000; ++i)
            errors += static_cast<int>(queue.pull() != i);
    }};
    for (int i = 0; i < 1000; ++i)
        queue.emplace(i);
    consumer.join();
    EXPECT_EQ(errors, 0);
}

TEST(DisruptorQueueTest, PullWaitsForPublicationBeforeCrossingAClaimedGap) {
    Disruptor<DelayedMessage> queue{2};
    std::latch claimed{1};
    std::latch release{1};
    std::thread first_producer{[&] { queue.emplace(11, &claimed, &release); }};
    claimed.wait();                       // Sequence 0 is reserved, but its constructor is still blocked.
    queue.emplace(22, nullptr, nullptr);  // Sequence 1 publishes out of order.
    auto first = std::async(std::launch::async, [&] { return queue.pull().value; });
    EXPECT_EQ(first.wait_for(std::chrono::milliseconds{20}), std::future_status::timeout);
    release.count_down();
    first_producer.join();
    EXPECT_EQ(first.get(), 11);
    EXPECT_EQ(queue.pull().value, 22);
}

TEST_F(DisruptorTest, ErasedWaitStrategiesAcceptBothSequenceWidths) {
    check_wait_widths<BusySpinWaitStrategy>();
    check_wait_widths<YieldingWaitStrategy>();
    check_wait_widths<BlockingWaitStrategy>();
    check_wait_widths<TimeoutBlockingWaitStrategy>();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
