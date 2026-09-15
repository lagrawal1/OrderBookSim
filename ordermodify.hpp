#ifndef ORDERMODIFY_HPP
#define ORDERMODIFY_HPP
#include "order.hpp"

#include <algorithm>
#include <list>
#include <map>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify {
  public:
    OrderModify(OrderID orderID, Side side, Price price, Quantity quantity)
        : orderID_(orderID), side_(side), price_(price), quantity_(quantity) {}

    OrderID GetOrderID() const { return orderID_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetQuantity() const { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type) const;

  private:
    OrderID orderID_;
    Side side_;
    Price price_;
    Quantity quantity_;
};

OrderPointer OrderModify::ToOrderPointer(OrderType type) const {
    return std::make_shared<Order>(type, GetOrderID(), GetSide(), GetPrice(),
                                   GetQuantity());
}

#endif