#include <algorithm>
#include <array>
#include <barrier>
#include <bit>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <disruptor.hpp>
#include <rigtorp/SPSCQueue.h>
#include <thread_pinning.hpp>

namespace {
    using Clock       = std::chrono::steady_clock;
    using Value       = std::uint64_t;
    using SingleQueue = menagerie::starling::Disruptor<Value, menagerie::starling::SingleProducerSequencer>;
    using MultiQueue  = menagerie::starling::Disruptor<Value>;

    class RigtorpQueue {
    public:
        explicit RigtorpQueue(const std::size_t capacity)
            : queue_{capacity} {
        }

        void emplace(const Value value) {
            queue_.emplace(value);
        }

        [[nodiscard]] Value pull() {
            Value* front;
            while ((front = queue_.front()) == nullptr) {
                menagerie::starling::pause_arc_agnostic();
            }
            const Value value = *front;
            queue_.pop();
            return value;
        }

    private:
        rigtorp::SPSCQueue<Value> queue_;
    };

    struct Options {
        std::uint64_t items    = 100'000'000;
        std::uint64_t warmup   = 1'000'000;
        std::size_t capacity   = 65'536;
        unsigned repetitions   = 3;
        int producer_cpu       = -1;
        int consumer_cpu       = -1;
        std::string_view queue = "all";
    };

    template <typename T>
    T number(const std::string_view value) {
        T result{};
        const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
        if (error != std::errc{} || end != value.data() + value.size()) {
            throw std::invalid_argument("Invalid number: " + std::string{value});
        }
        return result;
    }

    Options parse_options(const int argc, char** argv) {
        Options options;
        for (int i = 1; i < argc; ++i) {
            const std::string_view flag{argv[i]};
            if (flag == "--help") {
                std::cout << "SPSC: one emplace and one pull per transferred uint64_t.\n"
                             "--items N (100000000) --capacity N (65536, power of two)\n"
                             "--repetitions N (3) --warmup N (1000000 per queue/run)\n"
                             "--queue all|single|multi|rigtorp (all)\n"
                             "--producer-cpu N --consumer-cpu N (default: unpinned)\n";
                std::exit(EXIT_SUCCESS);
            }
            if (++i == argc) {
                throw std::invalid_argument("Missing value for " + std::string{flag});
            }
            const std::string_view value{argv[i]};
            if (flag == "--items")
                options.items = number<std::uint64_t>(value);
            else if (flag == "--warmup")
                options.warmup = number<std::uint64_t>(value);
            else if (flag == "--capacity")
                options.capacity = number<std::size_t>(value);
            else if (flag == "--repetitions")
                options.repetitions = number<unsigned>(value);
            else if (flag == "--producer-cpu")
                options.producer_cpu = number<int>(value);
            else if (flag == "--consumer-cpu")
                options.consumer_cpu = number<int>(value);
            else if (flag == "--queue")
                options.queue = value;
            else
                throw std::invalid_argument("Unknown option: " + std::string{flag});
        }
        constexpr auto max_sequence = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (options.items == 0 || options.repetitions == 0 || !std::has_single_bit(options.capacity) ||
            options.capacity > max_sequence || options.warmup > max_sequence ||
            options.items > max_sequence - options.warmup) {
            throw std::invalid_argument("Items/repetitions must be positive, capacity a representable power of two, "
                                        "and items + warmup must fit int64_t");
        }
        if (options.queue != "all" && options.queue != "single" && options.queue != "multi" &&
            options.queue != "rigtorp") {
            throw std::invalid_argument("Queue must be all, single, multi, or rigtorp");
        }
        if (options.producer_cpu < -1 || options.consumer_cpu < -1 ||
            (options.producer_cpu == -1) != (options.consumer_cpu == -1)) {
            throw std::invalid_argument("Specify both CPU IDs, or neither");
        }
        if (options.producer_cpu >= 0) {
            cpu_set_t allowed;
            if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
                throw std::runtime_error("Cannot read CPU affinity");
            }
            if (options.producer_cpu == options.consumer_cpu || options.producer_cpu >= CPU_SETSIZE ||
                options.consumer_cpu >= CPU_SETSIZE || !CPU_ISSET(options.producer_cpu, &allowed) ||
                !CPU_ISSET(options.consumer_cpu, &allowed)) {
                throw std::invalid_argument("CPU IDs must be distinct and in the process affinity mask");
            }
        }
        return options;
    }

    struct Result {
        double seconds;
        std::uint64_t checksum;
    };

    template <typename Queue>
    Result run(const Options& options) {
        Queue queue{options.capacity};
        Clock::time_point start;
        Clock::time_point producer_end;
        Clock::time_point consumer_end;
        // The completion runs before either worker is released into its timed loop.
        std::barrier start_gate{3, [&]() noexcept { start = Clock::now(); }};
        bool producer_pinned   = true;
        bool consumer_pinned   = true;
        std::uint64_t errors   = 0;
        std::uint64_t checksum = 0;

        std::thread consumer{[&] {
            if (options.consumer_cpu >= 0) {
                consumer_pinned = menagerie::starling::pin_current_thread_to_core(options.consumer_cpu);
            }
            for (std::uint64_t i = 0; i < options.warmup; ++i) {
                errors += static_cast<std::uint64_t>(queue.pull() != i);
            }
            start_gate.arrive_and_wait();
            for (std::uint64_t i = 0; i < options.items; ++i) {
                const auto value  = queue.pull();
                errors           += static_cast<std::uint64_t>(value != i);
                checksum         += value;
            }
            consumer_end = Clock::now();
        }};
        std::thread producer{[&] {
            if (options.producer_cpu >= 0) {
                producer_pinned = menagerie::starling::pin_current_thread_to_core(options.producer_cpu);
            }
            for (std::uint64_t i = 0; i < options.warmup; ++i) {
                queue.emplace(i);
            }
            start_gate.arrive_and_wait();
            for (std::uint64_t i = 0; i < options.items; ++i) {
                queue.emplace(i);
            }
            producer_end = Clock::now();
        }};
        start_gate.arrive_and_wait();
        producer.join();
        consumer.join();

        if (!producer_pinned || !consumer_pinned) {
            throw std::runtime_error("Worker CPU pinning failed; timing discarded");
        }
        // Divide before multiplying to preserve the modulo-2^64 sum for large N.
        const auto expected = options.items % 2 == 0 ? (options.items / 2) * (options.items - 1)
                                                     : options.items * ((options.items - 1) / 2);
        if (errors != 0 || checksum != expected) {
            throw std::runtime_error("FIFO/payload validation failed; timing discarded");
        }
        return {std::chrono::duration<double>(std::max(producer_end, consumer_end) - start).count(), checksum};
    }
}  // namespace

int main(const int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        std::cout << "# baseline=rigtorp::SPSCQueue\n"
                  << "# compiler=" << __VERSION__ << " payload_bytes=" << sizeof(Value)
                  << " sequence_bytes=" << sizeof(menagerie::starling::Sequence)
                  << " single_queue_bytes=" << sizeof(SingleQueue)
                  << " single_sequencer_bytes="
                  << sizeof(menagerie::starling::SingleProducerSequencer<menagerie::starling::BusySpinWaitStrategy>)
                  << " warmup=" << options.warmup << " producer_cpu=" << options.producer_cpu
                  << " consumer_cpu=" << options.consumer_cpu << '\n'
                  << "# One transfer = one emplace + one pull; ns/transfer is inverse throughput, not latency.\n"
                  << "queue,repetition,items,capacity,seconds,million_transfers_per_second,ns_per_transfer,checksum\n";
        std::array<std::string_view, 3> queues{"single", "multi", "rigtorp"};
        for (unsigned repetition = 0; repetition < options.repetitions; ++repetition) {
            for (const auto name : queues) {
                if (options.queue != "all" && options.queue != name)
                    continue;
                const auto result = name == "single"  ? run<SingleQueue>(options)
                                    : name == "multi" ? run<MultiQueue>(options)
                                                      : run<RigtorpQueue>(options);
                const auto items  = static_cast<double>(options.items);
                std::cout << name << ',' << repetition + 1 << ',' << options.items << ',' << options.capacity << ','
                          << std::fixed << std::setprecision(6) << result.seconds << ',' << items / result.seconds / 1e6
                          << ',' << result.seconds * 1e9 / items << ',' << result.checksum << std::endl;
            }
            // Rotate first/last position to reduce systematic run-order bias.
            std::rotate(queues.begin(), queues.begin() + 1, queues.end());
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "spsc_queue_benchmark: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
