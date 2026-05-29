#pragma once

#include "bench_types.hpp"

#include <cstddef>
#include <vector>

BenchStats run_baseline_workload(const std::vector<SyntheticOrder>& orders,
                               double ghz,
                               std::size_t warmup);
