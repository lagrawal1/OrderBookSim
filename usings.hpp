#ifndef USINGS_HPP
#define USINGS_HPP

#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

using Price = std::int32_t;
using Quantity = std::uint32_t;
using OrderID = std::uint64_t;
using OrderIDs = std::vector<OrderID>;

const Price InvalidPrice = std::numeric_limits<Price>::quiet_NaN();

enum class OrderType {
    GoodTillCancel,
    FillAndKill,
    FillOrKill,
    GoodForDay,
    Market,
};

struct LevelInfo {
    Price price_;
    Quantity quantity_;
};

#endif