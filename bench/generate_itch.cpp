#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace {

constexpr std::size_t kTargetBytes = 1024 * 1024;  // ~1 MB
constexpr const char* kOutputPath = "data/sample.bin";

inline uint16_t to_be16(uint16_t v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return v;
#else
    return __builtin_bswap16(v);
#endif
}

inline uint32_t to_be32(uint32_t v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return v;
#else
    return __builtin_bswap32(v);
#endif
}

inline uint64_t to_be64(uint64_t v) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return v;
#else
    return __builtin_bswap64(v);
#endif
}

inline void write_be48(uint8_t (&dst)[6], uint64_t v) {
    dst[0] = static_cast<uint8_t>((v >> 40) & 0xFFu);
    dst[1] = static_cast<uint8_t>((v >> 32) & 0xFFu);
    dst[2] = static_cast<uint8_t>((v >> 24) & 0xFFu);
    dst[3] = static_cast<uint8_t>((v >> 16) & 0xFFu);
    dst[4] = static_cast<uint8_t>((v >> 8) & 0xFFu);
    dst[5] = static_cast<uint8_t>(v & 0xFFu);
}

inline void write_stock_symbol(char (&dst)[8], const std::string& sym) {
    for (int i = 0; i < 8; ++i) {
        dst[i] = ' ';
    }
    for (std::size_t i = 0; i < sym.size() && i < 8; ++i) {
        dst[i] = sym[i];
    }
}

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
    uint8_t attribution;  // payload field (not message type), keeps payload at 36 bytes
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

template <typename PayloadT>
bool write_message(std::ofstream& out, char msg_type, const PayloadT& payload, std::size_t& bytes_written) {
    const uint16_t msg_len_be = to_be16(static_cast<uint16_t>(1 + sizeof(PayloadT)));

    out.write(reinterpret_cast<const char*>(&msg_len_be), sizeof(msg_len_be));
    out.write(&msg_type, sizeof(msg_type));
    out.write(reinterpret_cast<const char*>(&payload), sizeof(payload));

    if (!out.good()) {
        return false;
    }

    bytes_written += sizeof(msg_len_be) + sizeof(msg_type) + sizeof(payload);
    return true;
}

}  // namespace

int main() {
    std::filesystem::create_directories("data");

    std::ofstream out(kOutputPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return 1;
    }

    std::mt19937_64 rng(0x1A7C50ULL);
    std::uniform_int_distribution<int> msg_dist(0, 3);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<uint32_t> shares_dist(1, 5000);
    std::uniform_int_distribution<uint32_t> price_dist(100, 500000);
    std::uniform_int_distribution<uint16_t> locate_dist(1, 5000);
    std::uniform_int_distribution<uint16_t> track_dist(1, 65535);
    std::uniform_int_distribution<int> symbol_dist(0, 5);

    const std::string symbols[] = {"AAPL", "MSFT", "NVDA", "AMZN", "GOOG", "META"};

    uint64_t timestamp_counter = 1;
    uint64_t order_ref_counter = 1;
    uint64_t match_counter = 10'000;
    std::size_t bytes_written = 0;

    while (bytes_written < kTargetBytes) {
        const int kind = msg_dist(rng);

        if (kind == 0) {
            AddOrderPayload p{};
            p.stock_locate = to_be16(locate_dist(rng));
            p.tracking_number = to_be16(track_dist(rng));
            write_be48(p.timestamp, timestamp_counter++);
            p.order_reference_number = to_be64(order_ref_counter++);
            p.buy_sell_indicator = side_dist(rng) == 0 ? 'B' : 'S';
            p.shares = to_be32(shares_dist(rng));
            write_stock_symbol(p.stock, symbols[symbol_dist(rng)]);
            p.price = to_be32(price_dist(rng));
            p.attribution = 0;

            if (!write_message(out, 'A', p, bytes_written)) {
                return 2;
            }
        } else if (kind == 1) {
            DeleteOrderPayload p{};
            p.stock_locate = to_be16(locate_dist(rng));
            p.tracking_number = to_be16(track_dist(rng));
            write_be48(p.timestamp, timestamp_counter++);
            p.order_reference_number = to_be64((order_ref_counter > 1) ? (order_ref_counter - 1) : 1);

            if (!write_message(out, 'D', p, bytes_written)) {
                return 2;
            }
        } else if (kind == 2) {
            OrderExecutedPayload p{};
            p.stock_locate = to_be16(locate_dist(rng));
            p.tracking_number = to_be16(track_dist(rng));
            write_be48(p.timestamp, timestamp_counter++);
            p.order_reference_number = to_be64((order_ref_counter > 1) ? (order_ref_counter - 1) : 1);
            p.executed_shares = to_be32(shares_dist(rng));
            p.match_number = to_be64(match_counter++);

            if (!write_message(out, 'E', p, bytes_written)) {
                return 2;
            }
        } else {
            OrderReplacePayload p{};
            p.stock_locate = to_be16(locate_dist(rng));
            p.tracking_number = to_be16(track_dist(rng));
            write_be48(p.timestamp, timestamp_counter++);
            p.original_order_reference_number = to_be64((order_ref_counter > 1) ? (order_ref_counter - 1) : 1);
            p.new_order_reference_number = to_be64(order_ref_counter++);
            p.shares = to_be32(shares_dist(rng));
            p.price = to_be32(price_dist(rng));

            if (!write_message(out, 'U', p, bytes_written)) {
                return 2;
            }
        }
    }

    out.flush();
    return out.good() ? 0 : 3;
}

