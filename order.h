#ifndef ORDER_H
#define ORDER_H

#include <string>
#include <map>
#include <list>
#include <atomic>
#include <condition_variable>
#include <shared_mutex>

class RestingOrder {
private:
    uint32_t order_id;
    std::string instrument;
    uint32_t price;
    uint32_t count;
    std::string side; // buy or sell
    intmax_t timestamp;
    intmax_t deleted_timestamp;
    intmax_t executed_timestamp;
    std::atomic<uint32_t> curr_execution_id{1};
    std::shared_mutex mut; // Protect read/writes to the order

    bool is_ready;
    std::mutex is_ready_mutex;
    std::condition_variable is_ready_cond_var;

public:
    RestingOrder(uint32_t order_id, const std::string& instrument, uint32_t price, uint32_t count, const std::string& side, intmax_t timestamp);
    RestingOrder(RestingOrder&& other) noexcept; // move constructor
    RestingOrder& operator=(RestingOrder&& other) noexcept; // move assignment operator
    uint32_t get_execution_id();
    intmax_t get_deleted_timestamp();
    uint32_t get_price();
    intmax_t get_timestamp();
    uint32_t get_count();
    void delete_order(intmax_t timestamp);
    void check_order_ready_or_wait();
    void decrease_count(uint32_t decrease_by);
    uint32_t get_order_id();
    void set_count(uint32_t count);
    void set_order_ready(); 
    std::string get_side();
    std::string get_instrument();
};

#endif