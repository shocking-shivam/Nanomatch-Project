#include "../Limit_Order_Book/rdtsc.hpp"
#include "bench_types.hpp"
#include "baseline_adapter.h"

#include "../Limit_Order_Book/Book.hpp"
#include "../Limit_Order_Book/trade_logger.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr size_t kDefaultWorkloadSize = 490000;
constexpr size_t kWarmupOps = 10000;
constexpr int kPriceMean = 300;
constexpr int kPriceSigma = 30;
constexpr int kMaxPrice = 1023;

class OrderGenerator {
public:
    explicit OrderGenerator(uint64_t seed) : rng_(seed) {}

    std::vector<SyntheticOrder> generate(size_t count) {
        std::vector<SyntheticOrder> orders;
        orders.reserve(count);

        std::vector<int> liveIds;
        liveIds.reserve(count / 2);

        std::uniform_int_distribution<int> sideDist(0, 1);
        std::uniform_int_distribution<int> sharesDist(1, 200);
        std::uniform_int_distribution<int> kindDist(0, 99);
        std::uniform_real_distribution<double> uni(0.0, 1.0);

        int nextId = 1;

        for (size_t i = 0; i < count; ++i) {
            SyntheticOrder o{};
            const int roll = kindDist(rng_);

            if (roll < 55 || liveIds.empty()) {
                o.type = OpType::Add;
                o.orderId = nextId++;
                o.buy = sideDist(rng_) == 0;
                o.shares = sharesDist(rng_);
                o.price = samplePrice();
                liveIds.push_back(o.orderId);
            } else if (roll < 75) {
                o.type = OpType::Cancel;
                const size_t idx = static_cast<size_t>(uni(rng_) * liveIds.size());
                if (idx >= liveIds.size()) {
                    continue;
                }
                o.orderId = liveIds[idx];
                liveIds.erase(liveIds.begin() + static_cast<std::ptrdiff_t>(idx));
            } else if (roll < 90) {
                o.type = OpType::Modify;
                const size_t idx = static_cast<size_t>(uni(rng_) * liveIds.size());
                if (idx >= liveIds.size()) {
                    continue;
                }
                o.orderId = liveIds[idx];
                o.newShares = sharesDist(rng_);
                o.newPrice = samplePrice();
                o.buy = sideDist(rng_) == 0;
            } else {
                o.type = OpType::Market;
                o.orderId = nextId++;
                o.buy = sideDist(rng_) == 0;
                o.shares = sharesDist(rng_);
            }

            orders.push_back(o);
        }

        return orders;
    }

private:
    int samplePrice() {
        const int raw = static_cast<int>(std::lround(priceDist_(rng_)));
        return std::clamp(raw, 0, kMaxPrice);
    }

    std::mt19937_64 rng_;
    std::normal_distribution<double> priceDist_{
        static_cast<double>(kPriceMean), static_cast<double>(kPriceSigma)};
};

BenchStats compute_stats(const std::string& name, std::vector<uint64_t> cycles, double ghz) {
    BenchStats stats{name, std::move(cycles), ghz, 0, 0, 0, 0, 0, 0, 0};
    if (stats.cycles.empty()) {
        return stats;
    }

    stats.count = stats.cycles.size();
    std::vector<uint64_t> sorted = stats.cycles;
    std::sort(sorted.begin(), sorted.end());

    const auto to_ns = [ghz](uint64_t cycles) {
        return (static_cast<double>(cycles) / ghz);
    };

    uint64_t sum = 0;
    for (uint64_t c : stats.cycles) {
        sum += c;
    }

    stats.mean_ns = to_ns(sum / stats.count);
    stats.min_ns = to_ns(sorted.front());
    stats.max_ns = to_ns(sorted.back());

    auto pct = [&](double p) {
        const size_t idx = static_cast<size_t>(p * static_cast<double>(sorted.size() - 1));
        return to_ns(sorted[idx]);
    };

    stats.p50_ns = pct(0.50);
    stats.p90_ns = pct(0.90);
    stats.p99_ns = pct(0.99);
    return stats;
}

void write_percentile_csv(const std::string& path,
                          const BenchStats& baseline,
                          const BenchStats& slab,
                          const BenchStats& slabLogged) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        std::cerr << "Error: could not open " << path << " for writing\n";
        return;
    }

    out << "implementation,count,mean_ns,p50_ns,p90_ns,p99_ns,min_ns,max_ns\n";
    auto row = [&](const BenchStats& s) {
        out << s.name << ','
            << s.count << ','
            << s.mean_ns << ','
            << s.p50_ns << ','
            << s.p90_ns << ','
            << s.p99_ns << ','
            << s.min_ns << ','
            << s.max_ns << '\n';
    };
    row(baseline);
    row(slab);
    row(slabLogged);
}

void write_histogram_csv(const std::string& path,
                         const BenchStats& baseline,
                         const BenchStats& slab,
                         const BenchStats& slabLogged,
                         double ghz) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        std::cerr << "Error: could not open " << path << " for writing\n";
        return;
    }

    auto build_hist = [ghz](const std::vector<uint64_t>& cycles) {
        std::map<uint64_t, uint64_t> buckets;
        for (uint64_t c : cycles) {
            const uint64_t ns = static_cast<uint64_t>(static_cast<double>(c) / ghz);
            const uint64_t bucket = (ns / 10) * 10;
            buckets[bucket] += 1;
        }
        return buckets;
    };

    const auto baseHist = build_hist(baseline.cycles);
    const auto slabHist = build_hist(slab.cycles);
    const auto slabLoggedHist = build_hist(slabLogged.cycles);

    std::vector<uint64_t> edges;
    edges.reserve(baseHist.size() + slabHist.size() + slabLoggedHist.size());
    for (const auto& [edge, _] : baseHist) {
        edges.push_back(edge);
    }
    for (const auto& [edge, _] : slabHist) {
        edges.push_back(edge);
    }
    for (const auto& [edge, _] : slabLoggedHist) {
        edges.push_back(edge);
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());

    out << "latency_bucket_ns," << baseline.name << ',' << slab.name << ',' << slabLogged.name << '\n';
    for (uint64_t edge : edges) {
        out << edge << ','
            << (baseHist.count(edge) ? baseHist.at(edge) : 0) << ','
            << (slabHist.count(edge) ? slabHist.at(edge) : 0) << ','
            << (slabLoggedHist.count(edge) ? slabLoggedHist.at(edge) : 0) << '\n';
    }
}

template <typename BookT>
void dispatch(BookT& book, const SyntheticOrder& order) {
    switch (order.type) {
        case OpType::Add:
            book.addOrder(order.orderId, order.buy, order.shares, order.price);
            break;
        case OpType::Cancel:
            book.cancelOrder(order.orderId);
            break;
        case OpType::Modify:
            book.modifyOrder(order.orderId, order.newShares, order.newPrice);
            break;
        case OpType::Market:
            book.marketOrder(order.orderId, order.buy, order.shares);
            break;
    }
}

struct SlabBookAdapter {
    Book book;

    void addOrder(int orderId, bool buyOrSell, int shares, int limitPrice) {
        book.addLimitOrder(orderId, buyOrSell, shares, limitPrice);
    }

    void cancelOrder(int orderId) { book.cancelLimitOrder(orderId); }

    void modifyOrder(int orderId, int newShares, int newLimit) {
        book.modifyLimitOrder(orderId, newShares, newLimit);
    }

    void marketOrder(int orderId, bool buyOrSell, int shares) {
        book.marketOrder(orderId, buyOrSell, shares);
    }
};

struct SlabBookWithLoggerAdapter {
    TradeLogger* logger;
    Book book;

    explicit SlabBookWithLoggerAdapter(TradeLogger* loggerIn)
        : logger(loggerIn)
        , book(loggerIn) {}

    void addOrder(int orderId, bool buyOrSell, int shares, int limitPrice) {
        book.addLimitOrder(orderId, buyOrSell, shares, limitPrice);
    }

    void cancelOrder(int orderId) { book.cancelLimitOrder(orderId); }

    void modifyOrder(int orderId, int newShares, int newLimit) {
        book.modifyLimitOrder(orderId, newShares, newLimit);
    }

    void marketOrder(int orderId, bool buyOrSell, int shares) {
        book.marketOrder(orderId, buyOrSell, shares);
    }
};

template <typename BookT>
BenchStats run_workload(const std::string& name,
                        const std::vector<SyntheticOrder>& orders,
                        double ghz,
                        size_t warmup) {
    BookT book;
    const size_t warmupCount = std::min(warmup, orders.size());

    for (size_t i = 0; i < warmupCount; ++i) {
        dispatch(book, orders[i]);
    }

    std::vector<uint64_t> cycles;
    cycles.reserve(orders.size() - warmupCount);

    for (size_t i = warmupCount; i < orders.size(); ++i) {
        const uint64_t t0 = rdtsc();
        dispatch(book, orders[i]);
        const uint64_t t1 = rdtsc();
        cycles.push_back(t1 - t0);
    }

    return compute_stats(name, std::move(cycles), ghz);
}

BenchStats run_workload_with_logger(const std::string& name,
                                    const std::vector<SyntheticOrder>& orders,
                                    double ghz,
                                    size_t warmup) {
    TradeLogger logger("docs/bench_trades.csv");
    logger.start();

    SlabBookWithLoggerAdapter bookWithLogger(&logger);

    const size_t warmupCount = std::min(warmup, orders.size());
    for (size_t i = 0; i < warmupCount; ++i) {
        dispatch(bookWithLogger, orders[i]);
    }

    std::vector<uint64_t> cycles;
    cycles.reserve(orders.size() - warmupCount);

    for (size_t i = warmupCount; i < orders.size(); ++i) {
        const uint64_t t0 = rdtsc();
        dispatch(bookWithLogger, orders[i]);
        const uint64_t t1 = rdtsc();
        cycles.push_back(t1 - t0);
    }

    logger.stop();

    return compute_stats(name, std::move(cycles), ghz);
}

double throughput_mps(const BenchStats& stats) {
    if (stats.count == 0 || stats.cycles.empty() || stats.ghz <= 0.0) {
        return 0.0;
    }

    uint64_t cycle_sum = 0;
    for (uint64_t c : stats.cycles) {
        cycle_sum += c;
    }

    const double elapsed_seconds =
        static_cast<double>(cycle_sum) / (stats.ghz * 1e9);
    if (elapsed_seconds <= 0.0) {
        return 0.0;
    }

    return (static_cast<double>(stats.count) / elapsed_seconds) / 1000000.0;
}

struct BenchConfig {
    size_t msgs = kDefaultWorkloadSize;
    std::string out_csv = "docs/bench.csv";
    std::string hist_csv = "docs/hist.csv";
};

BenchConfig parse_args(int argc, char** argv) {
    BenchConfig cfg;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--msgs" && i + 1 < argc) {
            cfg.msgs = static_cast<size_t>(std::strtoull(argv[++i], nullptr, 10));
        } else if (arg == "--out-csv" && i + 1 < argc) {
            cfg.out_csv = argv[++i];
        } else if (arg == "--hist" && i + 1 < argc) {
            cfg.hist_csv = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << '\n';
        }
    }
    return cfg;
}

void print_comparison_table(const BenchStats& baseline,
                            const BenchStats& slab,
                            const BenchStats& slabLogged) {
    const auto speedup = [&](double base, double opt) {
        return base / opt;
    };

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\n=== Latency comparison (nanoseconds) ===\n";
    std::cout << std::setw(18) << "metric"
              << std::setw(18) << baseline.name
              << std::setw(18) << slab.name
              << std::setw(18) << slabLogged.name
              << std::setw(14) << "speedup (baseline/slab)"
              << std::setw(14) << "overhead (logged/slab)\n";

    const auto row = [&](const char* label, double b, double s, double sl) {
        std::cout << std::setw(18) << label
                  << std::setw(18) << b
                  << std::setw(18) << s
                  << std::setw(18) << sl
                  << std::setw(14) << speedup(b, s) << "x"
                  << std::setw(14) << (sl / s) << "x\n";
    };

    row("mean", baseline.mean_ns, slab.mean_ns, slabLogged.mean_ns);
    row("p50", baseline.p50_ns, slab.p50_ns, slabLogged.p50_ns);
    row("p90", baseline.p90_ns, slab.p90_ns, slabLogged.p90_ns);
    row("p99", baseline.p99_ns, slab.p99_ns, slabLogged.p99_ns);
    row("throughput (M/s)", throughput_mps(baseline), throughput_mps(slab), throughput_mps(slabLogged));
    row("min", baseline.min_ns, slab.min_ns, slabLogged.min_ns);
    row("max", baseline.max_ns, slab.max_ns, slabLogged.max_ns);

    std::cout << "\nSamples per run: " << baseline.count << '\n';
    std::cout << "CPU frequency (GHz): " << baseline.ghz << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    const BenchConfig cfg = parse_args(argc, argv);

    std::cout << "Calibrating TSC frequency...\n";
    const double ghz = detect_ghz();
    std::cout << "Detected CPU: " << ghz << " GHz\n";

    OrderGenerator generator(0xC0FFEE);
    const std::vector<SyntheticOrder> orders = generator.generate(cfg.msgs);
    std::cout << "Generated " << orders.size() << " synthetic orders\n";

    std::cout << "Running warmup + benchmark (BookBaseline / STL alloc)...\n";
    BenchStats baselineStats = run_baseline_workload(orders, ghz, kWarmupOps);

    std::cout << "Running warmup + benchmark (Book / slab pool)...\n";
    BenchStats slabStats =
        run_workload<SlabBookAdapter>("Book", orders, ghz, kWarmupOps);

    std::cout << "Running warmup + benchmark (Book / slab pool + trade logging)...\n";
    BenchStats slabLoggedStats =
        run_workload_with_logger("Book+Logger", orders, ghz, kWarmupOps);

    print_comparison_table(baselineStats, slabStats, slabLoggedStats);

    const auto parent = std::filesystem::path(cfg.out_csv).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    const auto hist_parent = std::filesystem::path(cfg.hist_csv).parent_path();
    if (!hist_parent.empty()) {
        std::filesystem::create_directories(hist_parent);
    }

    write_percentile_csv(cfg.out_csv, baselineStats, slabStats, slabLoggedStats);
    write_histogram_csv(cfg.hist_csv, baselineStats, slabStats, slabLoggedStats, ghz);

    std::cout << "\nWrote " << cfg.out_csv << " and " << cfg.hist_csv << '\n';
    return 0;
}
