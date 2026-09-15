#ifndef ORDERBOOK_HPP
#define ORDERBOOK_HPP

#include "LevelInfo.hpp"
#include "order.hpp"
#include "orderbook.hpp"
#include "ordermodify.hpp"
#include "trade.hpp"

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

class OrderBook {
  private:
    struct OrderEntry {
        OrderPointer order_{nullptr};
        OrderPointers::iterator location_;
    };

    struct LevelData {
        Quantity quantity_{};
        Quantity count_{};

        enum class Action {
            Add,
            Remove,
            Match,
        };
    };

    std::map<Price, OrderPointers, std::greater<Price>>
        bids_; // std::greater makes descending

    std::map<Price, OrderPointers, std::less<Price>>
        asks_; // std::less makes ascending

    std::unordered_map<OrderID, OrderEntry> orders_;
    std::unordered_map<Price, LevelData> data_;

    mutable std::mutex ordersMutex_;
    std::thread ordersPruneThread_;
    std::condition_variable shutdownConditionVariable_;
    std::atomic<bool> shutdown_{false};

    bool CanFullyFill(Side side, Price price, Quantity quantity) const;
    bool CanMatch(Side side, Price price) const;
    Trades MatchOrders();

    void OnOrderCancelled(OrderPointer order);
    void OnOrderAdded(OrderPointer order);
    void OnOrderMatched(Price price, Quantity quantity, bool isFullyFilled);
    void UpdateLevelData(Price price, Quantity quantity,
                         LevelData::Action action);

  public:
    ~OrderBook();
    OrderBook() : ordersPruneThread_{[this] { PruneGoodForDayOrders(); }} {}

    Trades AddOrder(OrderPointer order);
    void CancelOrder(OrderID orderID);
    void CancelOrders(OrderIDs orderIDs);
    void CancelOrderInternal(OrderID orderID);

    Trades ModifyOrder(OrderModify order);

    std::size_t Size() const { return orders_.size(); }

    OrderBookLevelInfos GetOrderInfos() const {
        LevelInfos bidInfos, askInfos;
        bidInfos.reserve(orders_.size());
        askInfos.reserve(orders_.size());
        auto CreateLevelInfos = [](Price price, const OrderPointers &orders) {
            return LevelInfo{
                price,
                std::accumulate(
                    orders.begin(), orders.end(), (Quantity)0,
                    [](std::size_t runningSum, const OrderPointer &order) {
                        return runningSum + order->GetRemainingQuantity();
                    })};
        };

        for (const auto &[price, orders] : bids_) {
            bidInfos.push_back(CreateLevelInfos(price, orders));
        };

        for (const auto &[price, orders] : asks_) {
            askInfos.push_back(CreateLevelInfos(price, orders));
        };

        return OrderBookLevelInfos{bidInfos, askInfos};
    };

    void PruneGoodForDayOrders();
};

bool OrderBook::CanMatch(Side side, Price price) const {
    switch (side) {
    case Side::Buy: {
        if (asks_.empty())
            return false;

        const auto &bestAsk = asks_.begin()->first;
        return price >= bestAsk;
    }

    case Side::Sell: {
        if (bids_.empty())
            return false;

        const auto &bestBid = bids_.begin()->first;
        return price <= bestBid;
    }

    default:
        throw std::logic_error("Side must be sell or buy");
    }
}

bool OrderBook::CanFullyFill(Side side, Price price, Quantity quantity) const {
    if (!CanMatch(side, price)) {
        return false;
    }

    std::optional<Price> threshold;
    switch (side) {
    case Side::Buy: {

        const auto askPrice =
            asks_.begin()->first; // lowest price willing to sell at
        threshold = askPrice;
        break;
    }

    case Side::Sell: {
        const auto bidPrice =
            bids_.begin()->first; // highest price willing to buy at
        threshold = bidPrice;
        break;
    }
    }

    for (const auto &[levelPrice, levelData] : data_) {
        if (threshold.has_value() &&
            ((side == Side::Buy && threshold.value() > levelPrice) ||
             (side == Side::Sell && threshold.value() < levelPrice)))
            continue;

        if (((side == Side::Buy && levelPrice > price) ||
             (side == Side::Sell && levelPrice < price)))
            continue;

        if (quantity <= levelData.quantity_)
            return true;
        quantity -= levelData.quantity_;
    }
    return false;
}

Trades OrderBook::MatchOrders() {
    Trades trades;
    trades.reserve(orders_.size());
    while (true) {
        if (bids_.empty() || asks_.empty()) {
            break;
        }

        auto &[bidPrice, bids] = *bids_.begin();
        auto &[askPrice, asks] = *asks_.begin();

        // if best bid is lower than best ask, then trade can't happen

        if (bidPrice < askPrice) {
            break;
        }

        while (bids.size() && asks.size()) {
            auto bid = bids.front();
            auto ask = asks.front();

            Quantity quantity = std::min(bid->GetRemainingQuantity(),
                                         ask->GetRemainingQuantity());
            bid->Fill(quantity);
            ask->Fill(quantity);

            if (bid->isFilled()) {
                bids.pop_front();
                orders_.erase(bid->GetOrderID());
            }

            if (ask->isFilled()) {
                asks.pop_front();
                orders_.erase(ask->GetOrderID());
            }

            trades.push_back(
                Trade{TradeInfo{bid->GetOrderID(), bid->GetPrice(), quantity},
                      TradeInfo(ask->GetOrderID(), ask->GetPrice(), quantity)});
            OnOrderMatched(bid->GetPrice(), quantity, bid->isFilled());
            OnOrderMatched(ask->GetPrice(), quantity, ask->isFilled());
        }

        if (bids.empty()) {
            bids_.erase(bidPrice);
            data_.erase(bidPrice);
        }

        if (asks.empty()) {
            asks_.erase(askPrice);
            data_.erase(askPrice);
        }
    }

    if (!bids_.empty()) {
        auto &bids = bids_.begin()->second;
        auto &order = bids.front();

        if (order->GetOrderType() == OrderType::FillAndKill) {
            CancelOrder(order->GetOrderID());
        }
    }

    if (!asks_.empty()) {
        auto &asks = asks_.begin()->second;
        auto &order = asks.front();

        if (order->GetOrderType() == OrderType::FillAndKill) {
            CancelOrder(order->GetOrderID());
        }
    }
    return trades;
}

Trades OrderBook::AddOrder(OrderPointer order) {
    if (orders_.contains(order->GetOrderID()))
        return {};

    if (order->GetOrderType() == OrderType::Market) {
        switch (order->GetSide()) {
        case Side::Buy: {
            if (asks_.empty())
                return {};
            const auto &worstAsk = *asks_.rbegin();
            order->SetMarketPrice(worstAsk.first);

            break;
        }

        case Side::Sell: {
            if (bids_.empty())
                return {};
            const auto &worstBid = *bids_.rbegin();
            order->SetMarketPrice(worstBid.first);
            break;
        }
        default:
            return {};
        }
    }

    if (order->GetOrderType() == OrderType::FillAndKill &&
        !CanMatch(order->GetSide(), order->GetPrice()))
        return {};

    if (order->GetOrderType() == OrderType::FillOrKill &&
        !CanFullyFill(order->GetSide(), order->GetPrice(),
                      order->GetInitialQuantity()))
        return {};

    OrderPointers::iterator iter;

    switch (order->GetSide()) {

    case Side::Buy: {
        auto &orders = bids_[order->GetPrice()];
        orders.push_back(order);
        iter = std::prev(orders.end());
        break;
    }

    case Side::Sell: {
        auto &orders = asks_[order->GetPrice()];
        orders.push_back(order);
        iter = std::prev(orders.end());
        break;
    }
    }

    orders_.insert({order->GetOrderID(), OrderEntry(order, iter)});
    OnOrderAdded(order);
    return MatchOrders();
}

Trades OrderBook::ModifyOrder(OrderModify order) {
    OrderType orderType;
    {
        std::scoped_lock ordersLock{ordersMutex_};

        if (!orders_.contains(order.GetOrderID()))
            return {};

        const auto &existingOrder = orders_.at(order.GetOrderID()).order_;
        orderType = existingOrder->GetOrderType();
    }
    CancelOrder(order.GetOrderID());
    return AddOrder(order.ToOrderPointer(orderType)); //
}

void OrderBook::CancelOrderInternal(OrderID orderID) {
    if (!orders_.contains(orderID))
        return;
    const auto [order, ordIter] = orders_.at(orderID);
    orders_.erase(orderID);

    switch (order->GetSide()) {
    case Side::Sell: {
        auto price = order->GetPrice();
        auto &orders = asks_.at(price);
        orders.erase(ordIter);

        if (orders.empty()) {
            asks_.erase(price);
        }

        break;
    }

    case Side::Buy: {
        auto price = order->GetPrice();
        auto &orders = bids_.at(price);
        orders.erase(ordIter);

        if (orders.empty()) {
            bids_.erase(price);
        }
    }
    }
    OnOrderCancelled(order);
}

void OrderBook::CancelOrder(OrderID orderID) {
    std::scoped_lock ordersLock{ordersMutex_};
    CancelOrderInternal(orderID);
}

void OrderBook::CancelOrders(OrderIDs orderIDs) {
    std::scoped_lock ordersLock{ordersMutex_};

    for (const auto &id : orderIDs) {
        CancelOrderInternal(id);
    }
}

void OrderBook::PruneGoodForDayOrders() {
    const auto end = std::chrono::hours(16);
    const auto now = std::chrono::system_clock::now();
    const auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_parts;
    localtime_r(&now_c, &now_parts);

    if (now_parts.tm_hour >= end.count()) {
        now_parts.tm_mday += 1;
    }

    now_parts.tm_hour = end.count();
    now_parts.tm_min = 0;
    now_parts.tm_sec = 0;

    auto next = std::chrono::system_clock::from_time_t(std::mktime(&now_parts));
    auto till = next - now + std::chrono::milliseconds(100);

    {
        std::unique_lock ordersLock{ordersMutex_};

        if (shutdown_.load(std::memory_order_acquire) ||
            shutdownConditionVariable_.wait_for(ordersLock, till) ==
                std::cv_status::no_timeout) {
            return;
        }
    }

    OrderIDs orderIDs;
    {
        std::scoped_lock ordersLock_{ordersMutex_};
        for (const auto &[_, entry] : orders_) {
            const auto &order = entry.order_;

            if (!(order->GetOrderType() == OrderType::GoodForDay ||
                  order->GetOrderType() == OrderType::Market))
                continue;
            orderIDs.push_back(order->GetOrderID());
        }
    }
    CancelOrders(orderIDs);
}

void OrderBook::OnOrderCancelled(OrderPointer order) {
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(),
                    LevelData::Action::Remove);
};

void OrderBook::OnOrderAdded(OrderPointer order) {
    UpdateLevelData(order->GetPrice(), order->GetRemainingQuantity(),
                    LevelData::Action::Add);
}

void OrderBook::OnOrderMatched(Price price, Quantity quantity,
                               bool isFullyFilled) {
    UpdateLevelData(price, quantity,
                    isFullyFilled ? LevelData::Action::Remove
                                  : LevelData::Action::Match);
}

void OrderBook::UpdateLevelData(Price price, Quantity quantity,
                                LevelData::Action action) {
    auto &data = data_[price];
    data.count_ += action == LevelData::Action::Remove ? -1
                   : action == LevelData::Action::Add  ? 1
                                                       : 0;

    switch (action) {
    case LevelData::Action::Add:
        data.quantity_ += quantity;
        break;

    case LevelData::Action::Remove:
        data.quantity_ -= quantity;
        break;

    case LevelData::Action::Match:
        data.quantity_ -= quantity;
        break;
    default:
        throw std::logic_error("Invalid LevelData Action");
    }
}

OrderBook::~OrderBook() {
    shutdown_.store(true, std::memory_order_release);
    shutdownConditionVariable_.notify_one();
    ordersPruneThread_.join();
}

#endif