# NanoMatch: Ultra-Low Latency C++ Limit Order Book

NanoMatch is a high-frequency, ultra-low latency Limit Order Book (LOB) matching engine written in C++17. It is designed from the ground up for maximum mechanical sympathy, processing orders with deterministic execution times.

By eliminating OS-level memory allocation, stripping runtime branch prediction, preventing false sharing, and utilizing lock-free synchronization, NanoMatch is built to High-Frequency Trading (HFT) standards.

## ⚡ Architecture & Optimizations

### 1. $O(1)$ Slab Memory Allocator
Standard `std::map` and `new/delete` rely on the OS heap allocator (`__libc_malloc`), which can cost 1000+ CPU cycles per call due to cache misses. NanoMatch bypasses the OS entirely. 
* Pre-allocated contiguous memory pools (`lob::SlabPool`) are established at startup.
* Placement `new` constructs `Order` and `Limit` objects directly into cache-hot memory slots in $O(1)$ time (~4 cycles).

### 2. Lock-Free SPSC Logging Queue
Logging trade fills to a disk usually blocks the hot path. Mutexes force kernel context switches.
* Implemented a custom **Single-Producer Single-Consumer (SPSC)** ring buffer.
* Uses `std::atomic` with explicit memory barriers (`memory_order_release` / `acquire`).
* `head_` and `tail_` atomics are padded with `alignas(64)` to separate cache lines, completely eradicating false sharing.
* **Result:** Trade events are offloaded to a background I/O thread in ~5 nanoseconds, adding effectively zero latency (**<2% overhead**) to the matching loop.

### 3. Compile-Time Branch Elimination
In standard engines, the matching loop must evaluate `if (buyOrSell)` on every single order execution, leading to pipeline stalls.
* NanoMatch uses C++ templates (`if constexpr`) to generate two fully independent, side-specialized matching functions.
* The routing branch is resolved at compile time, ensuring a pure, linear execution pipeline.

### 4. Cache-Aligned Structs
Struct padding and false sharing can evict critical pointers from the L1 cache.
* The `Order` struct is packed strictly into a **40-byte** block to fit cleanly inside a standard 64-byte CPU cache line.
* The `Limit` struct utilizes `alignas(64)` to isolate the `headOrder` (touched every execution) onto Cache Line 0, keeping AVL Tree navigation pointers isolated.

### 5. NASDAQ ITCH 5.0 Zero-Copy Parser
* Utilizes `mmap` with `MADV_SEQUENTIAL` to map binary exchange data directly into virtual memory.
* Bypasses userspace copying entirely, achieving parsing speeds in excess of **95 Million messages per second**.

## 📊 Performance Benchmark

Measured using `rdtsc` hardware cycle counters on an Intel CPU @ 3.09GHz processing 490,000 synthetic orders:

| Metric | Baseline (`std::map` + `new`) | NanoMatch (Slab Pool) | Speedup |
| :--- | :--- | :--- | :--- |
| **p50 (Median)** | 1673 ns | **1222 ns** | 1.37x |
| **p90** | 4448 ns | **3556 ns** | 1.25x |
| **Min** | 20 ns | **20 ns** | 1.00x |

*Note: SPSC Background Logging added a negligible ~1.02x overhead to the p50 hot path.*

## 📈 Flame Graph Profiling
*Run `sudo perf record -F 99 -g -- ./build/bench/bench_tool` to recreate.*
See `docs/flamegraph.svg` to visually verify the eradication of `__libc_malloc` overhead from the execution path.

## 🛠️ Build Instructions
Requires a compiler that supports C++17. Built and tested on Linux (Ubuntu/WSL2).

```bash
git clone [https://github.com/shocking-shivam/Nanomatch-Project.git](https://github.com/shocking-shivam/Nanomatch-Project.git)
cd Nanomatch-Project
mkdir build && cd build

# Compile with Ultra-Low Latency Flags (-O3, -march=native, -fno-exceptions)
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4

# Run the core benchmark suite
./bench/bench_tool --out-csv docs/bench.csv --hist docs/hist.csv

# Run the lock-free queue stress tests
ctest --output-on-failure