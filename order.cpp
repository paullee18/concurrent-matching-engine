#include "order.h"
#include <shared_mutex>

RestingOrder::RestingOrder(uint32_t order_id, const std::string& instrument, uint32_t price, uint32_t count, const std::string& side, intmax_t timestamp)
        : order_id(order_id), instrument(instrument), price(price), count(count), side(side), timestamp(timestamp) {
            this->deleted_timestamp = -1;
            this->executed_timestamp = -1;
            this->is_ready = false;
        }


// Define move constructor
RestingOrder::RestingOrder(RestingOrder&& other) noexcept
    : order_id(other.order_id), instrument(std::move(other.instrument)), price(other.price), count(other.count),
        side(std::move(other.side)), timestamp(other.timestamp), deleted_timestamp(other.deleted_timestamp), executed_timestamp(other.executed_timestamp),
        curr_execution_id(other.curr_execution_id.load()) {}

// Define move assignment operator
RestingOrder& RestingOrder::operator=(RestingOrder&& other) noexcept {
    if (this != &other) {
        order_id = other.order_id;
        instrument = std::move(other.instrument);
        price = other.price;
        count = other.count;
        side = std::move(other.side);
        timestamp = other.timestamp;
        deleted_timestamp = other.deleted_timestamp;
        executed_timestamp = other.executed_timestamp;
        curr_execution_id.store(other.curr_execution_id.load());
    }
    return *this;
}

// Get and increment curr_execution_id
uint32_t RestingOrder::get_execution_id() {
    std::shared_lock<std::shared_mutex> lock(this->mut);
    return this->curr_execution_id++;
}

intmax_t RestingOrder::get_deleted_timestamp() {
    std::shared_lock<std::shared_mutex> lock(this->mut);
    return this->deleted_timestamp;
}

void RestingOrder::delete_order(intmax_t timestamp) {
    std::unique_lock<std::shared_mutex> lock(this->mut);
    if (this->deleted_timestamp == -1 || this->deleted_timestamp > timestamp) {
        this->deleted_timestamp = timestamp;
    }
}

uint32_t RestingOrder::get_price() {
    std::shared_lock<std::shared_mutex> lock(this->mut);
    return this->price;
}

intmax_t RestingOrder::get_timestamp() {
    std::shared_lock<std::shared_mutex> lock(this->mut);
    return this->timestamp;
}

void RestingOrder::check_order_ready_or_wait() {
    std::unique_lock<std::mutex> lk{this->is_ready_mutex};
    while (!this->is_ready) {
        this->is_ready_cond_var.wait(lk);
    }
}

uint32_t RestingOrder::get_count() {
    std::shared_lock<std::shared_mutex> lock(this->mut);
    return this->count;
}

void RestingOrder::decrease_count(uint32_t decrease_by) {
    std::unique_lock<std::shared_mutex> lock(this->mut);
    this->count -= decrease_by;        
}

uint32_t RestingOrder::get_order_id() {
    std::shared_lock<std::shared_mutex> lock(this->mut);
    return this->order_id;
}

void RestingOrder::set_count(uint32_t count) {
    std::unique_lock<std::shared_mutex> lock(this->mut);
    this->count = count;
}

void RestingOrder::set_order_ready() {
    {
        std::lock_guard<std::mutex> lk{this->is_ready_mutex};
        this->is_ready = true;
    }
    this->is_ready_cond_var.notify_all();
}

std::string RestingOrder::get_side() {
    std::shared_lock<std::shared_mutex> lock(this->mut);
    return this->side;
}

std::string RestingOrder::get_instrument() {
    std::shared_lock<std::shared_mutex> lock(this->mut);
    return this->instrument;
}