#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../Limit_Order_Book/rdtsc.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>

namespace {

constexpr const char* kInputPath = "data/sample.bin";

#pragma pack(push, 1)
struct AddOrderPayload {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t timestamp[6];
    uint64_t order_reference_number;
    char buy_sell_indicator;
    uint32_t shares;
    char stock[8];
    uint32_t price;
    uint8_t attribution;
};
static_assert(sizeof(AddOrderPayload) == 36, "AddOrderPayload must be 36 bytes");

struct DeleteOrderPayload {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t timestamp[6];
    uint64_t order_reference_number;
};
static_assert(sizeof(DeleteOrderPayload) == 18, "DeleteOrderPayload must be 18 bytes");

struct OrderExecutedPayload {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t timestamp[6];
    uint64_t order_reference_number;
    uint32_t executed_shares;
    uint64_t match_number;
};
static_assert(sizeof(OrderExecutedPayload) == 30, "OrderExecutedPayload must be 30 bytes");

struct OrderReplacePayload {
    uint16_t stock_locate;
    uint16_t tracking_number;
    uint8_t timestamp[6];
    uint64_t original_order_reference_number;
    uint64_t new_order_reference_number;
    uint32_t shares;
    uint32_t price;
};
static_assert(sizeof(OrderReplacePayload) == 34, "OrderReplacePayload must be 34 bytes");
#pragma pack(pop)

inline uint16_t load_be16_unaligned(const uint8_t* p) {
    uint16_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return ntohs(v);
}

}  // namespace

int main() {
    const int fd = open(kInputPath, O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open " << kInputPath << "\n";
        return 1;
    }

    struct stat st {};
    if (fstat(fd, &st) != 0) {
        std::cerr << "fstat failed\n";
        close(fd);
        return 1;
    }

    if (st.st_size <= 0) {
        std::cerr << "Input file is empty\n";
        close(fd);
        return 1;
    }

    const std::size_t size = static_cast<std::size_t>(st.st_size);
    void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        std::cerr << "mmap failed\n";
        close(fd);
        return 1;
    }

    if (madvise(mapped, size, MADV_SEQUENTIAL) != 0) {
        std::cerr << "madvise(MADV_SEQUENTIAL) failed\n";
        munmap(mapped, size);
        close(fd);
        return 1;
    }

    const uint8_t* cursor = static_cast<const uint8_t*>(mapped);
    const uint8_t* const end = cursor + size;

    uint64_t add_count = 0;
    uint64_t del_count = 0;
    uint64_t exe_count = 0;
    uint64_t rep_count = 0;
    uint64_t total_count = 0;

    // Prevent the compiler from optimizing away payload reads.
    volatile uint64_t checksum = 0;

    const uint64_t t0 = rdtsc();
    while (cursor + 2 <= end) {
        const uint16_t length = load_be16_unaligned(cursor);
        cursor += 2;

        if (length == 0 || cursor + length > end) {
            break;
        }

        const uint8_t type = *cursor;
        const uint8_t* payload_ptr = cursor + 1;

        switch (type) {
            case 'A': {
                if (length >= 1 + sizeof(AddOrderPayload)) {
                    const auto* msg = reinterpret_cast<const AddOrderPayload*>(payload_ptr);
                    checksum += static_cast<uint64_t>(msg->buy_sell_indicator);
                }
                ++add_count;
                break;
            }
            case 'D': {
                if (length >= 1 + sizeof(DeleteOrderPayload)) {
                    const auto* msg = reinterpret_cast<const DeleteOrderPayload*>(payload_ptr);
                    checksum += static_cast<uint64_t>(msg->timestamp[0]);
                }
                ++del_count;
                break;
            }
            case 'E': {
                if (length >= 1 + sizeof(OrderExecutedPayload)) {
                    const auto* msg = reinterpret_cast<const OrderExecutedPayload*>(payload_ptr);
                    checksum += static_cast<uint64_t>(msg->timestamp[1]);
                }
                ++exe_count;
                break;
            }
            case 'U': {
                if (length >= 1 + sizeof(OrderReplacePayload)) {
                    const auto* msg = reinterpret_cast<const OrderReplacePayload*>(payload_ptr);
                    checksum += static_cast<uint64_t>(msg->timestamp[2]);
                }
                ++rep_count;
                break;
            }
            default:
                break;
        }

        cursor += length;
        ++total_count;
    }
    const uint64_t t1 = rdtsc();

    const uint64_t elapsed_cycles = t1 - t0;
    const double ghz = detect_ghz();
    const double elapsed_seconds = static_cast<double>(elapsed_cycles) / (ghz * 1e9);
    const double mps = (elapsed_seconds > 0.0)
        ? static_cast<double>(total_count) / elapsed_seconds
        : 0.0;

    std::cout << "Parsed messages: " << total_count << "\n";
    std::cout << "A count: " << add_count << "\n";
    std::cout << "D count: " << del_count << "\n";
    std::cout << "E count: " << exe_count << "\n";
    std::cout << "U count: " << rep_count << "\n";
    std::cout << "Elapsed cycles: " << elapsed_cycles << "\n";
    std::cout << "Elapsed seconds: " << elapsed_seconds << "\n";
    std::cout << "Messages/sec: " << mps << "\n";
    std::cout << "Checksum: " << checksum << "\n";

    munmap(mapped, size);
    close(fd);
    return 0;
}

