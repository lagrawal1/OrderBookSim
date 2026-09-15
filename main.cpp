#include "LevelInfo.hpp"
#include "order.hpp"
#include "orderbook.hpp"
#include "ordermodify.hpp"
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

int main() {
    OrderBook orderbook;
    const OrderID orderID = 1;
    orderbook.AddOrder(OrderPointer(std::make_shared<Order>(
        OrderType::GoodTillCancel, orderID, Side::Buy, 100, 10)));

    orderbook.AddOrder(OrderPointer(
        std::make_shared<Order>(OrderType::FillOrKill, 2, Side::Sell, 100, 9)));

    std::cout << "Size: " << orderbook.Size() << "\n";
    std::cout << "Size: " << orderbook.GetOrderInfos().GetAsks().size() << "\n";

    return 0;
}