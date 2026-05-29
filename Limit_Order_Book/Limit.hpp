#ifndef LIMIT_HPP
#define LIMIT_HPP

#include <cstddef>

#ifdef USE_STL_ALLOC
namespace baseline {
#endif

class Order;

struct alignas(64) Limit {
private:
    Order* headOrder;
    Order* tailOrder;
    int size;
    int totalVolume;
    int limitPrice;
    int _pad0;
    bool buyOrSell;
    char _padLine0[64 - 33];

    alignas(64) Limit* parent;
    Limit* leftChild;
    Limit* rightChild;
    char _padLine1[40];

    friend class Order;
public:
    Limit(int _limitPrice, bool _buyOrSell, int _size=0, int _totalVolume=0);
    ~Limit();

    Order* getHeadOrder() const;
    int getLimitPrice() const;
    int getSize() const;
    int getTotalVolume() const;
    bool getBuyOrSell() const;
    Limit* getParent() const;
    Limit* getLeftChild() const;
    Limit* getRightChild() const;
    void setParent(Limit* newParent);
    void setLeftChild(Limit* newLeftChild);
    void setRightChild(Limit* newRightChild);
    void partiallyFillTotalVolume(int orderedShares);

    void append(Order *_order);

    void printForward() const;
    void printBackward() const;
    void print() const;
};

static_assert(sizeof(Limit) == 128, "Limit must be exactly 128 bytes");
// static_assert(offsetof(Limit, headOrder) == 0, "headOrder must start at offset 0");
// static_assert(offsetof(Limit, parent) == 64, "parent must start at offset 64");

#ifdef USE_STL_ALLOC
} // namespace baseline
#endif

#endif
