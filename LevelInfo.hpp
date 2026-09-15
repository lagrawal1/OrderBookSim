#ifndef LEVELINFO_HPP
#define LEVELINFO_HPP

#include "order.hpp"
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

using LevelInfos = std::vector<LevelInfo>;

class OrderBookLevelInfos {

  public:
    OrderBookLevelInfos(const LevelInfos &bids, const LevelInfos &asks)
        : bids_(bids), asks_(asks) {};
    const LevelInfos &GetBids() const { return bids_; }
    const LevelInfos &GetAsks() const { return asks_; }

  private:
    LevelInfos bids_;
    LevelInfos asks_;
};

struct TradeInfo {
    OrderID orderID_;
    Price price_;
    Quantity quantity_;
};
#endif