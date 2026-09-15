#ifndef ORDER_HPP
#define ORDER_HPP
#include "side.hpp"
#include "usings.hpp"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <deque>
#include <format>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <set>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

class Order {

  public:
    Order(OrderType orderType, OrderID orderID, Side side, Price price,
          Quantity quantity)
        : orderType_(orderType), orderID_(orderID), side_(side), price_(price),
          initialQuantity_(quantity), remainingQuantity_(quantity) {};

    Order(OrderID orderID, Side side, Quantity quantity)
        : Order(OrderType::Market, orderID, side, InvalidPrice, quantity) {};

    OrderType GetOrderType() const { return orderType_; }
    OrderID GetOrderID() const { return orderID_; }
    Side GetSide() const { return side_; }
    Price GetPrice() const { return price_; }
    Quantity GetInitialQuantity() const { return initialQuantity_; }
    Quantity GetRemainingQuantity() const { return remainingQuantity_; }
    Quantity GetFilledQuantity() const {
        return initialQuantity_ - remainingQuantity_;
    }

    bool isFilled() { return GetRemainingQuantity() == 0; }

    void Fill(Quantity quantity);
    void SetMarketPrice(Price price);

  private:
    OrderType orderType_;
    OrderID orderID_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

void Order::Fill(Quantity quantity) {
    if (quantity > GetRemainingQuantity())
        throw std::logic_error(
            std::format("Order ({}) cannot be filled for more than its "
                        "remaining quantity!",
                        GetOrderID()));

    remainingQuantity_ -= quantity;
}

void Order::SetMarketPrice(Price price) {
    if (GetOrderType() != OrderType::Market || !std::isnan(price_)) {
        throw std::logic_error(
            std::format("Order ({}) cannot have its price adjusted, only "
                        "market orders can!",
                        GetOrderID()));
    }

    price_ = price;
}

#endif