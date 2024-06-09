#include <list>
#include "orderbook.h"
#include <memory>
#include "engine.hpp"
#include "ts_orderbook_hashmap.hpp"

Orderbook::Orderbook(std::string instrument) : instrument(instrument) {};

std::shared_ptr<RestingOrder> Orderbook::insert_order(const std::string& side, uint32_t order_id, const std::string& instrument, uint32_t price, uint32_t count, uint32_t timestamp) {
    std::shared_ptr<RestingOrder> order = std::make_shared<RestingOrder>(order_id, instrument, price, count, side, timestamp);
    
    if (side == "buy") {
        std::lock_guard<std::mutex> lock(this->buyPQMutex);
        this->buyOrders.push(order);
    } else {
        std::lock_guard<std::mutex> lock(this->sellPQMutex);
        this->sellOrders.push(order);
    }
    return order;
}

void Orderbook::insert_order_with_val(const std::string& side, std::shared_ptr<RestingOrder> val) {
    if (side == "buy") {
        std::lock_guard<std::mutex> lock(this->buyPQMutex);
        this->buyOrders.push(val);
    } else {
        std::lock_guard<std::mutex> lock(this->sellPQMutex);
        this->sellOrders.push(val);
    }
}

// Get the top order
std::optional<std::shared_ptr<RestingOrder>> Orderbook::get_top_order(std::string side) {
    if (side == "buy") {
        std::lock_guard<std::mutex> lock(this->buyPQMutex);
        if (!this->buyOrders.empty()) {
            return this->buyOrders.top();
        } else {
            return std::nullopt;
        }
    } else {
        std::lock_guard<std::mutex> lock(this->sellPQMutex);
        if (!this->sellOrders.empty()) {
            return this->sellOrders.top();
        } else {
            return std::nullopt;
        }
    }
}

// Pop the top order
std::optional<std::shared_ptr<RestingOrder>> Orderbook::popTopOrder(std::string side) {
    if (side == "buy") {
        std::lock_guard<std::mutex> lock(this->buyPQMutex);
        if (!this->buyOrders.empty()) {
            std::optional<std::shared_ptr<RestingOrder>> order = this->buyOrders.top();
            this->buyOrders.pop();
            return order;
        }
    } else {
        std::lock_guard<std::mutex> lock(this->sellPQMutex);
        if (!this->sellOrders.empty()) {
            std::optional<std::shared_ptr<RestingOrder>> order = this->sellOrders.top();
            this->sellOrders.pop();
            return order;
        }
    }

    return std::nullopt;
}

// Pop top order if deleted
void Orderbook::popTopOrderIfDeleted(std::string side, intmax_t curr_timestamp) {
    if (side == "buy") {
        std::lock_guard<std::mutex> lock(this->buyPQMutex);
        if (!this->buyOrders.empty() && this->buyOrders.top()->get_deleted_timestamp() >= 0 && this->buyOrders.top()->get_deleted_timestamp() <= curr_timestamp) {
            this->buyOrders.pop();
        }
    } else {
        std::lock_guard<std::mutex> lock(this->sellPQMutex);
        if (!this->sellOrders.empty() && this->sellOrders.top()->get_deleted_timestamp() >= 0 && this->sellOrders.top()->get_deleted_timestamp() <= curr_timestamp) {
            this->sellOrders.pop();
        }
    }
}

/*
Routine for initial order processing.
Prevent any other incoming order from getting processed.
Get timestamp and insert order with is_ready set to false.
*/ 
std::pair<intmax_t, std::shared_ptr<RestingOrder>> Orderbook::initialOrderProcessing(std::string side, uint32_t price, uint32_t count, uint32_t order_id,
	ts_orderbook_hashmap<uint32_t, RestingOrder> &order_map) {
    // Acquire full orderbook lock
    std::lock_guard<std::mutex> lock(this->incoming_order_mutex);
    intmax_t timestamp = getCurrentTimestamp();
    // Insert into side
    std::shared_ptr<RestingOrder> order = this->insert_order(side, order_id, this->instrument, price, count, timestamp);
    order_map.insert(order_id, order);
    return std::make_pair(timestamp, order);
}
