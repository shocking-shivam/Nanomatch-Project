#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>

// Lock-free single-producer/single-consumer (SPSC) queue.
//
// Notes:
// - Uses only memory_order_acquire for loads and memory_order_release for stores on the
//   head_/tail_ indices.
// - head_ and tail_ are kept on separate cache lines via aligned padded atomics.
template <class T, std::size_t N>
class SPSCQueue {
    static_assert(N >= 2, "SPSCQueue requires N >= 2");

    using Storage = std::aligned_storage_t<sizeof(T), alignof(T)>;

    struct alignas(64) PaddedAtomicIndex {
        std::atomic<std::size_t> v{0};
    };

public:
    SPSCQueue() = default;
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Enqueue by copy. Returns false if the queue is full.
    bool enqueue(const T& item) { return try_push_impl(item); }

    // Enqueue by move. Returns false if the queue is full.
    bool enqueue(T&& item) { return try_push_impl(std::move(item)); }

    // Dequeue into out. Returns false if the queue is empty.
    bool dequeue(T& out) {
        const std::size_t tail = tail_.v.load(std::memory_order_acquire);
        const std::size_t head = head_.v.load(std::memory_order_acquire);

        if (head == tail) {
            return false;
        }

        T* ptr = element_ptr(head);
        out = std::move(*ptr);
        ptr->~T();

        const std::size_t next_head = inc(head);
        head_.v.store(next_head, std::memory_order_release);
        return true;
    }

    // Convenience aliases (often used by callers).
    bool try_push(const T& item) { return enqueue(item); }
    bool try_push(T&& item) { return enqueue(std::move(item)); }
    bool try_pop(T& out) { return dequeue(out); }
    bool pop(T& out) { return dequeue(out); }

    bool empty() const {
        const std::size_t tail = tail_.v.load(std::memory_order_acquire);
        const std::size_t head = head_.v.load(std::memory_order_acquire);
        return head == tail;
    }

    bool full() const {
        const std::size_t head = head_.v.load(std::memory_order_acquire);
        const std::size_t tail = tail_.v.load(std::memory_order_acquire);
        return inc(tail) == head;
    }

private:
    static constexpr std::size_t kIndexModN = N;

    std::size_t inc(std::size_t i) const noexcept { return (i + 1 == kIndexModN) ? 0 : (i + 1); }

    T* element_ptr(std::size_t i) noexcept {
        return reinterpret_cast<T*>(&buffer_[i]);
    }

    const T* element_ptr(std::size_t i) const noexcept {
        return reinterpret_cast<const T*>(&buffer_[i]);
    }

    template <class U>
    bool try_push_impl(U&& item) {
        const std::size_t head = head_.v.load(std::memory_order_acquire);
        const std::size_t tail = tail_.v.load(std::memory_order_acquire);
        const std::size_t next_tail = inc(tail);

        // Ring buffer keeps one slot empty => max elements is N-1.
        if (next_tail == head) {
            return false;
        }

        // Construct the element at tail, then publish by storing tail_ with release semantics.
        new (&buffer_[tail]) T(std::forward<U>(item));
        tail_.v.store(next_tail, std::memory_order_release);
        return true;
    }

private:
    std::array<Storage, N> buffer_{};
    PaddedAtomicIndex head_{};
    PaddedAtomicIndex tail_{};
};

