#pragma once

#include "spsc_queue.hpp"

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <thread>
#include <utility>

// Exactly 32 bytes.
struct TradeEvent {
    uint64_t ts_ns = 0;
    uint64_t seq = 0;
    uint32_t price = 0;
    uint32_t shares = 0;
    uint32_t side = 0;
    uint32_t action = 0;
};

static_assert(sizeof(TradeEvent) == 32, "TradeEvent must be exactly 32 bytes");

class TradeLogger {
public:
    explicit TradeLogger(std::string csv_path)
        : csv_path_(std::move(csv_path)) {}

    TradeLogger(const TradeLogger&) = delete;
    TradeLogger& operator=(const TradeLogger&) = delete;

    ~TradeLogger() { stop(); }

    // Start the background logger thread. Safe to call once.
    void start() {
        bool expected = false;
        if (!started_.compare_exchange_strong(
                expected, true, std::memory_order_release, std::memory_order_acquire)) {
            return;
        }
        worker_ = std::thread(&TradeLogger::logger_loop, this);
    }

    // Log is non-blocking; events are dropped if the queue is full.
    void log(const TradeEvent& ev) { try_log(ev); }
    void log(TradeEvent&& ev) { try_log(std::move(ev)); }

    // Attempts to enqueue; returns false if the queue is full or stopping.
    bool try_log(const TradeEvent& ev) {
        if (stop_requested_.load(std::memory_order_acquire)) {
            return false;
        }
        return queue_.enqueue(ev);
    }

    bool try_log(TradeEvent&& ev) {
        if (stop_requested_.load(std::memory_order_acquire)) {
            return false;
        }
        return queue_.enqueue(std::move(ev));
    }

    void stop() {
        bool expected = false;
        if (!stop_requested_.compare_exchange_strong(
                expected, true, std::memory_order_release, std::memory_order_acquire)) {
            // already requested
        }

        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    static constexpr std::size_t kQueueCapacity = 1u << 16;

    void logger_loop() {
        std::ofstream out(csv_path_, std::ios::out | std::ios::trunc);
        if (!out.is_open()) {
            // No exceptions in this project; best-effort fallback is just to exit the logger.
            return;
        }

        out << "ts_ns,seq,price,shares,side,action\n";

        TradeEvent ev;
        while (!stop_requested_.load(std::memory_order_acquire) || !queue_.empty()) {
            if (queue_.dequeue(ev)) {
                out << ev.ts_ns << ','
                    << ev.seq << ','
                    << ev.price << ','
                    << ev.shares << ','
                    << ev.side << ','
                    << ev.action << '\n';
            } else {
                // Avoid burning CPU when no events are available.
                std::this_thread::yield();
            }
        }

        out.flush();
    }

private:
    std::string csv_path_;
    SPSCQueue<TradeEvent, kQueueCapacity> queue_;

    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> started_{false};
    std::thread worker_;
};

