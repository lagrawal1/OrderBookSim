#include "../orderbook.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string_view>
#include <tuple>
#include <vector>
namespace googletest = ::testing;

enum class ActionType {
    Add,
    Modify,
    Cancel,
};

struct Information {
    ActionType type_;
    OrderType orderType_;
    Side side_;
    Price price_;
    Quantity quantity_;
    OrderID orderID_;
};

using Informations = std::vector<Information>;

struct Result {
    std::size_t allCount_;
    std::size_t bidCount_;
    std::size_t askCount_;
    Informations resultOrderBook_;
};

struct InputHandler {
  private:
    std::uint32_t ToNumber(const std::string_view &str) const {
        std::int64_t value{};
        auto [_, ec] =
            std::from_chars(str.data(), str.data() + str.size(), value);
        if (value < 0) {
            throw std::logic_error("Value can't be negative");
        }

        if (ec == std::errc{}) {
            return static_cast<std::uint32_t>(value);
        }
        std::cout << std::make_error_code(ec);

        if (ec == std::errc::invalid_argument) {
            throw std::invalid_argument("Not a Number");
        } else {
            throw std::logic_error("Error Occurred in ToNumber");
        }
    }
    bool TryParseResult(const std::string_view &str, Result &result) const {
        if (str.at(0) != 'R')
            return false;

        auto values = Split(str, ' '); // R all bid ask
        result.allCount_ = ToNumber(values.at(1));
        result.bidCount_ = ToNumber(values.at(2));
        result.askCount_ = ToNumber(values.at(3));

        return true;
    }

    bool TryParseInformation(const std::string_view &str,
                             Information &info) const {

        auto val = str.at(0);
        auto values = Split(str, ' ');

        switch (val) {
        case 'A':
            info.type_ = ActionType::Add;
            info.side_ = ParseSide(values.at(1));
            info.orderType_ = ParseOrderType(values.at(2));
            info.price_ = ParsePrice(values.at(3));
            info.quantity_ = ParseQuantity(values.at(4));
            info.orderID_ = ParseOrderID(values.at(5));

            break;

        case 'M':
            info.type_ = ActionType::Modify;
            info.orderID_ = ParseOrderID(values.at(1));
            info.side_ = ParseSide(values.at(2));
            info.price_ = ParsePrice(values.at(3));
            info.quantity_ = ParseQuantity(values.at(4));

            break;

        case 'C':
            info.type_ = ActionType::Cancel;
            info.orderID_ = ParseOrderID(values.at(1));
            break;

        default:
            return false;
        }

        return true;
    }

    std::vector<std::string_view> Split(const std::string_view &str,
                                        char delimiter) const {
        std::vector<std::string_view> columns{};
        std::size_t startIndex{}, endIndex{};
        while ((endIndex = str.find(delimiter, startIndex)) &&
               endIndex != std::string::npos) {
            auto distance = endIndex - startIndex;
            auto column = str.substr(startIndex, distance);
            columns.push_back(column);
            startIndex = endIndex + 1;
        }
        columns.push_back(str.substr(startIndex));
        return columns;
    }

    Side ParseSide(std::string_view &str) const {
        if (str == "B")
            return Side::Buy;
        else if (str == "S")
            return Side::Sell;
        else
            throw std::logic_error("Unknown Side");
    }

    OrderType ParseOrderType(const std::string_view &str) const {
        if (str == "FillOrKill")
            return OrderType::FillOrKill;
        else if (str == "FillAndKill")
            return OrderType::FillAndKill;
        else if (str == "GoodForDay")
            return OrderType::GoodForDay;
        else if (str == "GoodTillCancel")
            return OrderType::GoodTillCancel;
        else if (str == "Market")
            return OrderType::Market;
        else
            throw std::logic_error("Unknown OrderType");
    };

    Price ParsePrice(const std::string_view &str) const {
        if (str.empty())
            throw std::logic_error("Unknown Price");

        return ToNumber(str);
    }

    Quantity ParseQuantity(const std::string_view &str) const {
        if (str.empty())
            throw std::logic_error("Unknown Quantity");

        return ToNumber(str);
    }

    Price ParseOrderID(const std::string_view &str) const {
        if (str.empty())
            throw std::logic_error("Unknown OrderID");

        return ToNumber(str);
    }

  public:
    std::tuple<Informations, Result>
    GetInformations(const std::filesystem::path &path) const {
        Informations testInfos;
        Result result;
        testInfos.reserve(1'000);
        result.resultOrderBook_.reserve(1'000);
        std::string line;
        std::ifstream file{path};
        bool isUpdate = true;
        bool hasResult = false;
        while (std::getline(file, line)) {

            if (line.empty())
                break;

            if (line.at(0) == 'R') {
                isUpdate = false;
            };

            if (isUpdate) {
                Information update;
                auto isValid = TryParseInformation(line, update);
                std::cout << isValid << std::endl;

                if (!isValid) {
                    throw std::logic_error(
                        std::format("Invalid update : {}", line));
                }
                testInfos.push_back(update);

            } else {
                hasResult = true;

                if (line.at(0) == 'R') {
                    auto isValid = TryParseResult(line, result);
                    if (!isValid) {
                        throw std::logic_error(
                            std::format("Invalid Result : {}", line));
                    }
                } else {
                    Information res;
                    std::cout << line;
                    auto isValid = TryParseInformation(line, res);
                    if (!isValid) {
                        throw std::logic_error(std::format(
                            "Invalid Result Information : {}", line));
                    }
                    result.resultOrderBook_.push_back(res);
                }
            }
        }

        if (!hasResult)
            throw std::logic_error("No result found");

        return {testInfos, result};
    }
};

class OrderBookTestFixtures : public googletest::TestWithParam<const char *> {
  private:
    const static inline std::filesystem::path Root{
        std::filesystem::current_path()};
    const static inline std::filesystem::path TestFolder{"TestFolder"};

  public:
    const static inline std::filesystem::path TestFolderPath{Root / TestFolder};
};

TEST_P(OrderBookTestFixtures, OrderBookTestSuite) {
    const auto file = OrderBookTestFixtures::TestFolderPath / GetParam();
    InputHandler handler;
    const auto [updates, result] = handler.GetInformations(file);

    auto GetOrder = [](const Information &information) {
        return std::make_shared<Order>(
            information.orderType_, information.orderID_, information.side_,
            information.price_, information.quantity_);
    };

    auto GetOrderModify = [](const Information &information) {
        return OrderModify{information.orderID_, information.side_,
                           information.price_, information.quantity_};
    };

    OrderBook orderbook;
    OrderBook accurateOrderBook;

    for (const auto &update : updates) {
        switch (update.type_) {
        case ActionType::Add: {
            const Trades &trades = orderbook.AddOrder(GetOrder(update));
            break;
        }
        case ActionType::Cancel: {
            orderbook.CancelOrder(update.orderID_);
            break;
        }
        case ActionType::Modify: {
            const Trades &trades =
                orderbook.ModifyOrder(GetOrderModify(update));
            break;
        }

        default: {
            throw std::logic_error("Unsupported Update");
            break;
        }
        };
    }

    for (const auto &res : result.resultOrderBook_) {
        std::cout << "order_id" << res.orderID_;
        switch (res.type_) {
        case ActionType::Add: {
            const Trades &trades = accurateOrderBook.AddOrder(GetOrder(res));
            break;
        }
        case ActionType::Cancel: {
            accurateOrderBook.CancelOrder(res.orderID_);
            break;
        }
        case ActionType::Modify: {
            const Trades &trades =
                accurateOrderBook.ModifyOrder(GetOrderModify(res));
            break;
        }
        default: {
            throw std::logic_error("Unsupported Update");
            break;
        }
        };
    }

    const auto &orderbookinfos = orderbook.GetOrderInfos();

    const auto &accorderbookinfos = accurateOrderBook.GetOrderInfos();

    for (auto &order : orderbookinfos.GetAsks()) {
        std::cout << std::format("Order Price {}, Order Quantity {}",
                                 order.price_, order.quantity_)
                  << std::endl;
    }

    for (auto &order : orderbookinfos.GetBids()) {
        std::cout << std::format("Order Price {}, Order Quantity {}",
                                 order.price_, order.quantity_)
                  << std::endl;
    }

    ASSERT_EQ(orderbook.Size(), result.allCount_);
    ASSERT_EQ(orderbookinfos.GetBids().size(), result.bidCount_);
    ASSERT_EQ(orderbookinfos.GetAsks().size(), result.askCount_);

    ASSERT_EQ(orderbookinfos.GetBids().size(),
              accorderbookinfos.GetBids().size());
    ASSERT_EQ(orderbookinfos.GetAsks().size(),
              accorderbookinfos.GetAsks().size());

    for (auto i{0uz}; i < orderbookinfos.GetAsks().size(); ++i) {
        auto accAsks = accorderbookinfos.GetAsks()[i];
        auto asks = orderbookinfos.GetAsks()[i];
        ASSERT_EQ(accAsks.price_, asks.price_);
        ASSERT_EQ(accAsks.quantity_, asks.quantity_);
    }

    for (auto i{0uz}; i < orderbookinfos.GetBids().size(); ++i) {
        auto accBids = accorderbookinfos.GetBids()[i];
        auto bids = orderbookinfos.GetBids()[i];
        ASSERT_EQ(accBids.price_, bids.price_);
        ASSERT_EQ(accBids.quantity_, bids.quantity_);
    }
}

INSTANTIATE_TEST_SUITE_P(Tests, OrderBookTestFixtures,
                         googletest::ValuesIn({
                             "Cancel_Success.txt",
                             "Match_FillAndKill.txt",
                             "Match_FillOrKill_Hit.txt",
                         }));