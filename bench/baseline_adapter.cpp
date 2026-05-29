#ifndef USE_STL_ALLOC
#define USE_STL_ALLOC
#endif

#include "../Limit_Order_Book/Book.hpp"

#include "bench_types.hpp"
#include "../Limit_Order_Book/rdtsc.hpp"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace {

void dispatch(BookBaseline& book, const SyntheticOrder& order) {
    switch (order.type) {
        case OpType::Add:
            book.addLimitOrder(order.orderId, order.buy, order.shares, order.price);
            break;
        case OpType::Cancel:
            book.cancelLimitOrder(order.orderId);
            break;
        case OpType::Modify:
            book.modifyLimitOrder(order.orderId, order.newShares, order.newPrice);
            break;
        case OpType::Market:
            book.marketOrder(order.orderId, order.buy, order.shares);
            break;
    }
}

BenchStats compute_stats(const std::string& name, std::vector<uint64_t> cycles, double ghz) {
    BenchStats stats{name, std::move(cycles), ghz, 0, 0, 0, 0, 0, 0, 0};
    if (stats.cycles.empty()) {
        return stats;
    }

    stats.count = stats.cycles.size();
    std::vector<uint64_t> sorted = stats.cycles;
    std::sort(sorted.begin(), sorted.end());

    const auto to_ns = [ghz](uint64_t c) { return static_cast<double>(c) / ghz; };

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

}  // namespace

BenchStats run_baseline_workload(const std::vector<SyntheticOrder>& orders,
                                double ghz,
                                size_t warmup) {
    BookBaseline book;
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

    return compute_stats("BookBaseline", std::move(cycles), ghz);
}
