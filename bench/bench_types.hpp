#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class OpType : uint8_t { Add, Cancel, Market, Modify };

struct SyntheticOrder {
    OpType type = OpType::Add;
    int orderId = 0;
    bool buy = true;
    int shares = 1;
    int price = 0;
    int newShares = 0;
    int newPrice = 0;
};

struct BenchStats {
    std::string name;
    std::vector<uint64_t> cycles;
    double ghz = 0.0;
    uint64_t count = 0;
    double mean_ns = 0.0;
    double p50_ns = 0.0;
    double p90_ns = 0.0;
    double p99_ns = 0.0;
    double min_ns = 0.0;
    double max_ns = 0.0;
};
