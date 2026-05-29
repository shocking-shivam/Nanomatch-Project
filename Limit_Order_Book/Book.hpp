#ifndef BOOK_HPP
#define BOOK_HPP

#include <new>
#ifndef USE_STL_ALLOC
#include "mempool.hpp"
#include <array>
#endif
#include <unordered_map>
#include <vector>
#include <random>
#include <unordered_set>

#include "trade_logger.hpp"

#ifdef USE_STL_ALLOC
namespace baseline {
#endif

class Limit;
class Order;

class Book {
private:
#ifndef USE_STL_ALLOC
    lob::SlabPool<Order> orderPool_;
    lob::SlabPool<Limit> limitPool_;
#endif

    Limit *buyTree;
    Limit *sellTree;
    alignas(64) Limit *highestBuyPtr_;
    alignas(64) Limit *lowestSellPtr_;

    Limit *stopBuyTree;
    Limit *stopSellTree;
    Limit *highestStopSell;
    Limit *lowestStopBuy;

#ifndef USE_STL_ALLOC
    std::array<Limit*, 1024> buyLimitArr_{};
    std::array<Limit*, 1024> sellLimitArr_{};
    std::array<Order*, 1 << 17> orderArr_{};
#else
    std::unordered_map<int, Order*> orderMap;
    std::unordered_map<int, Limit*> limitBuyMap;
    std::unordered_map<int, Limit*> limitSellMap;
#endif
    std::unordered_map<int, Limit*> stopMap;

    TradeLogger* logger_ = nullptr;

    Order* allocateOrder(int id, bool buyOrSell, int shares, int limitPrice);
    void deallocateOrder(Order* order);
    Limit* allocateLimit(int limitPrice, bool buyOrSell);
    void deallocateLimit(Limit* limit);

    void storeOrder(int orderId, Order* order);
    Order* findOrder(int orderId) const;
    void clearOrder(int orderId);

    Limit* findLimit(int limitPrice, bool buyOrSell) const;
    bool hasLimit(int limitPrice, bool buyOrSell) const;
    void setLimit(int limitPrice, bool buyOrSell, Limit* limit);
    void clearLimit(int limitPrice, bool buyOrSell);

    void addLimit(int limitPrice, bool buyOrSell);
    void addStop(int stopPrice, bool buyOrSell);
    Limit* insert(Limit* root, Limit* limit, Limit* parent=nullptr);
    Limit* insertStop(Limit* root, Limit* limit, Limit* parent=nullptr);
    void updateBookEdgeInsert(Limit* newLimit);
    void updateStopBookEdgeInsert(Limit* newStop);
    void updateBookEdgeRemove(Limit* limit);
    void updateStopBookEdgeRemove(Limit* stopLevel);
    void changeBookRoots(Limit* limit);
    void changeStopBookRoots(Limit* stopLevel);
    void deleteLimit(Limit* limit);
    void deleteStopLevel(Limit* limit);
    void deleteFromOrderMap(int orderId);
    void deleteFromLimitMaps(int LimitPrice, bool buyOrSell);
    void deleteFromStopMap(int StopPrice);
    int limitOrderAsMarketOrder(int orderId, bool buyOrSell, int shares, int limitPrice);
    int stopOrderAsMarketOrder(int orderId, bool buyOrSell, int shares, int stopPrice);
    int existingOrderAsMarketOrder(Order* headOrder, bool buyOrSell);
    int stopLimitOrderAsLimitOrder(int orderId, bool buyOrSell, int shares, int limitPrice, int stopPrice);
    void executeStopOrders(bool buyOrSell);
    void stopLimitOrderToLimitOrder(Order* headOrder, bool buyOrSell);
    void marketOrderHelper(int orderId, bool buyOrSell, int shares);

    // Functions to balance AVL tree
    int limitHeightDifference(Limit* limit);
    Limit* rr_rotate(Limit* limit);
    Limit* ll_rotate(Limit* limit);
    Limit* lr_rotate(Limit* limit);
    Limit* rl_rotate(Limit* limit);
    Limit* balance(Limit* limit);
    Limit* rr_rotateStop(Limit* limit);
    Limit* ll_rotateStop(Limit* limit);
    Limit* lr_rotateStop(Limit* limit);
    Limit* rl_rotateStop(Limit* limit);
    Limit* balanceStop(Limit* limit);

    template <bool IsBuy> void matchOrder(int id, int shares, int limitPrice);

public:
    explicit Book(TradeLogger* logger = nullptr);
    ~Book();

    // Counts used in order book perforamce visualisations
    int executedOrdersCount=0;
    int AVLTreeBalanceCount=0;

    // Getter and setter functions
    Limit* getBuyTree() const;
    Limit* getSellTree() const;
    Limit* getLowestSell() const;
    Limit* getHighestBuy() const;
    Limit* getStopBuyTree() const;
    Limit* getStopSellTree() const;
    Limit* getHighestStopSell() const;
    Limit* getLowestStopBuy() const;

    // Functions for different types of orders
    void marketOrder(int orderId, bool buyOrSell, int shares);
    void addLimitOrder(int orderId, bool buyOrSell, int shares, int limitPrice);
    void cancelLimitOrder(int orderId);
    void modifyLimitOrder(int orderId, int newShares, int newLimit);
    void addStopOrder(int orderId, bool buyOrSell, int shares, int stopPrice);
    void cancelStopOrder(int orderId);
    void modifyStopOrder(int orderId, int newShares, int newStopPrice);
    void addStopLimitOrder(int orderId, bool buyOrSell, int shares, int limitPrice, int stopPrice);
    void cancelStopLimitOrder(int orderId);
    void modifyStopLimitOrder(int orderId, int newShares, int newLimitPrice, int newStopPrice);

    // Functions that needed to be public for testing purposes
    int getLimitHeight(Limit* limit) const;
    Order* searchOrderMap(int orderId) const;
    Limit* searchLimitMaps(int limitPrice, bool buyOrSell) const;
    Limit* searchStopMap(int stopPrice) const;

    // Functions for visualising the order book
    void printLimit(int limitPrice, bool buyOrSell) const;
    void printOrder(int orderId) const;
    void printBookEdges() const;
    void printOrderBook() const;
    std::vector<int> inOrderTreeTraversal(Limit* root) const;
    std::vector<int> preOrderTreeTraversal(Limit* root) const;
    std::vector<int> postOrderTreeTraversal(Limit* root) const;

    // Functions and data structures needed for generating sample data
    Order* getRandomOrder(int key, std::mt19937 gen) const;
    std::unordered_set<Order*> limitOrders;
    std::unordered_set<Order*> stopOrders;
    std::unordered_set<Order*> stopLimitOrders;
};

#ifdef USE_STL_ALLOC
}  // namespace baseline
#endif

#ifdef USE_STL_ALLOC
namespace baseline {
class Book;
}
using BookBaseline = baseline::Book;
#else
// When compiling the main pooled version, alias BookBaseline to regular Book
// so that files like baseline_adapter can always find a valid class type.
using BookBaseline = Book;
#endif

#endif
