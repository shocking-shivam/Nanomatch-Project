#ifndef ORDER_HPP
#define ORDER_HPP

#ifdef USE_STL_ALLOC
namespace baseline {
#endif

class Limit;

class Order {
private:
    int idNumber;
    int shares;
    int limit;
    bool buyOrSell;
    Order *nextOrder;
    Order *prevOrder;
    Limit *parentLimit;

    friend class Limit;
public:
    Order(int _idNumber, bool _buyOrSell, int _shares, int _limit);

    int getShares() const;
    int getOrderId() const;
    bool getBuyOrSell() const;
    int getLimit() const;
    Limit* getParentLimit() const;

    void partiallyFillOrder(int orderedShares);
    void cancel();
    void execute();
    void modifyOrder(int newShares, int newLimit);
    void setShares(int newShares);

    void print() const;
};

static_assert(sizeof(Order) <= 64, "Order exceeds cache line");

#ifdef USE_STL_ALLOC
} // namespace baseline
#endif

#endif