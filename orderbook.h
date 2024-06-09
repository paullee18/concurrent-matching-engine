#ifndef ORDERBOOK_H
#define ORDERBOOK_H

#include <list>
#include <queue>
#include <optional>
#include "order.h"
#include <memory>
#include "ts_orderbook_hashmap.hpp"

struct BuyOrderComparator {
    bool operator()(const std::shared_ptr<RestingOrder>& a, const std::shared_ptr<RestingOrder>& b) const {
        uint32_t a_price = a->get_price();
        uint32_t b_price = b->get_price();
        if (a_price != b_price) 
            return a_price < b_price; // Priority highest price
        else
            return a->get_timestamp() > b->get_timestamp();
    }
};

struct SellOrderComparator {
    bool operator()(const std::shared_ptr<RestingOrder>& a, const std::shared_ptr<RestingOrder>& b) const {
        uint32_t a_price = a->get_price();
        uint32_t b_price = b->get_price();
        if (a_price != b_price)
            return a_price > b_price;
        else
            return a->get_timestamp() > b->get_timestamp();
    }
};

class Orderbook {
private:
    std::priority_queue<std::shared_ptr<RestingOrder>, std::vector<std::shared_ptr<RestingOrder>>, BuyOrderComparator> buyOrders;
    // mutex for protecting PQ operations
    std::mutex buyPQMutex;
    std::priority_queue<std::shared_ptr<RestingOrder>, std::vector<std::shared_ptr<RestingOrder>>, SellOrderComparator> sellOrders;
    // mutex for protecting PQ operations
    std::mutex sellPQMutex;
    std::string instrument;
    std::mutex incoming_order_mutex;
public:
    Orderbook(std::string instrument);

    // mutex for blocking processing sell orders
    std::mutex sellMutex;
    // mutex for blocking processing buy orders
    std::mutex buyMutex;

    // Delete copy constructor and copy assignment operator
    Orderbook(const Orderbook&) = delete;
    Orderbook& operator=(const Orderbook&) = delete;

    std::shared_ptr<RestingOrder> insert_order(const std::string& side, uint32_t order_id, const std::string& instrument, uint32_t price, uint32_t count, uint32_t timestamp);
    void insert_order_with_val(const std::string& side, std::shared_ptr<RestingOrder> val);
    // Get the top order
    std::optional<std::shared_ptr<RestingOrder>> get_top_order(std::string side);

    // Pop the top order and return value
    std::optional<std::shared_ptr<RestingOrder>> popTopOrder(std::string side);

    // Pop top order if deleted before the provided timestamp
    void popTopOrderIfDeleted(std::string side, intmax_t curr_timestamp);

    std::pair<intmax_t, std::shared_ptr<RestingOrder>>initialOrderProcessing(std::string side, uint32_t price, uint32_t count, uint32_t order_id, ts_orderbook_hashmap<uint32_t, RestingOrder> &order_map);

};
#endif 