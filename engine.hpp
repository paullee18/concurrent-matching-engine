// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include "io.hpp"
#include "ts_orderbook_hashmap.hpp"
#include "orderbook.h"
#include <string>

struct Engine
{
public:
	void accept(ClientConnection conn);

private:
	ts_orderbook_hashmap<std::string, Orderbook> orderbooks;
	ts_orderbook_hashmap<uint32_t, RestingOrder> idToOrder;
	void connection_thread(ClientConnection conn);
};

inline std::atomic<intmax_t> timestamp{0};

inline intmax_t getCurrentTimestamp() {
	return timestamp++;
}

#endif
