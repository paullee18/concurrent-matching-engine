#include <iostream>
#include <thread>

#include "io.hpp"
#include "engine.hpp"

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}

void Engine::connection_thread(ClientConnection connection)
{
	while(true)
	{
		ClientCommand input {};
		switch(connection.readInput(input))
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break;
		}

		// Functions for printing output actions in the prescribed format are
		// provided in the Output class:
		switch(input.type)
		{
			case input_cancel: {
				SyncCerr {} << "Got cancel: ID: " << input.order_id << std::endl;
				if (!this->idToOrder.exists(input.order_id)) {
					Output::OrderDeleted(input.order_id, false, getCurrentTimestamp());
					break;
				}
				std::shared_ptr<RestingOrder> order = this->idToOrder.get(input.order_id);
				order->check_order_ready_or_wait();

				// If order is a Buy, should not execute with a concurrent Sell as Sells may use up this Buy
				std::shared_ptr orderbook = this->orderbooks.get(order->get_instrument());
				std::lock_guard<std::mutex> order_side_lock(order->get_side() == "buy" ? orderbook->sellMutex : orderbook->buyMutex);

				intmax_t timestamp = getCurrentTimestamp();
				intmax_t delete_timestamp = order->get_deleted_timestamp();
				if (delete_timestamp >= 0 && delete_timestamp < timestamp) {
					Output::OrderDeleted(input.order_id, false, timestamp);
				} else {
					order->delete_order(timestamp);
					Output::OrderDeleted(input.order_id, true, timestamp);
				}
				break;
			}
			default: {
				SyncCerr {}
				    << "Got order: " << static_cast<char>(input.type) << " " << input.instrument << " x " << input.count << " @ "
				    << input.price << " ID: " << input.order_id << std::endl;

				std::string side = input.type == CommandType::input_buy ? "buy" : "sell";
				std::string other_side = side == "buy" ? "sell" : "buy";

				if (!this->orderbooks.exists(input.instrument)) {
					this->orderbooks.insert_if_not_exist(input.instrument);
				}

				std::shared_ptr<Orderbook> orderbook_ptr = this->orderbooks.get(input.instrument);
				
				// Lock rest of same side orders
				std::lock_guard<std::mutex> order_side_lock(side == "buy" ? orderbook_ptr->buyMutex : orderbook_ptr->sellMutex);

				// Perform initial processing sequentially
				std::pair<intmax_t, std::shared_ptr<RestingOrder>> pair = orderbook_ptr->initialOrderProcessing(side, input.price, input.count, input.order_id, this->idToOrder);
				intmax_t timestamp = pair.first;
				std::shared_ptr<RestingOrder> initial_order = pair.second;

				std::vector<std::shared_ptr<RestingOrder>> orders_to_add_back;

				int64_t count_left = input.count;
				// Try to fill orders
				while (count_left > 0) {
					std::optional<std::shared_ptr<RestingOrder>> optional = orderbook_ptr->popTopOrder(other_side);
					if (optional.has_value()) {
						std::shared_ptr<RestingOrder> other_ptr = optional.value();

						intmax_t deleted_timestamp = other_ptr -> get_deleted_timestamp();
						if (deleted_timestamp >= 0 && deleted_timestamp < timestamp) {
							continue;
						}


						if (other_ptr->get_timestamp() > timestamp) {
							// Ignore and replace any orders that have a greater timestamp as this represents an order that would be added after this order
							orders_to_add_back.emplace_back(other_ptr);
							if ((side == "buy" && other_ptr -> get_price() > input.price) || (side == "sell" && other_ptr -> get_price() < input.price)) {
								break; // No resting orders can fill this order
							}
							continue;
						}
						if ((side == "buy" && other_ptr -> get_price() > input.price) || (side == "sell" && other_ptr -> get_price() < input.price)) {
							orders_to_add_back.emplace_back(other_ptr);
							break; // No resting orders can fill this order
						}

						// Check if order is ready, if not wait as this means there is another concurrent opp order with lower timestamp with a good price that should be matched
						other_ptr->check_order_ready_or_wait();

						// Order can be used
						uint32_t other_count = other_ptr->get_count();
						if (other_count == 0) {
							continue;
						}
						if (count_left < other_count) {
							orders_to_add_back.push_back(other_ptr);
							// Update count of resting order
							other_ptr -> decrease_count(count_left);
							// Check the parameter names in `io.hpp`.
							Output::OrderExecuted(other_ptr->get_order_id(), input.order_id, other_ptr->get_execution_id(), other_ptr->get_price(), count_left, timestamp);
							count_left = 0;
							continue;
						}
						// Execute using full resting order
						// We set deleted_timestamp
						other_ptr -> delete_order(timestamp);
						// Check the parameter names in `io.hpp`.
						Output::OrderExecuted(other_ptr->get_order_id(), input.order_id, other_ptr->get_execution_id(), other_ptr->get_price(), other_ptr->get_count(), timestamp);
						count_left -= other_ptr->get_count();
						continue;
					} else {
						// No available resting orders
						break;
					}
				}

				if (count_left > 0) {
					Output::OrderAdded(input.order_id, input.instrument, input.price, count_left, input.type == input_sell, timestamp);
				} else {
					initial_order->delete_order(timestamp);
				}
				initial_order->set_count(count_left);
				// Update initial_order to ready and notify and waiting threads
				initial_order->set_order_ready();

				// Add back any orders that should be added back
				for (std::shared_ptr<RestingOrder> order_ptr : orders_to_add_back) {
					orderbook_ptr->insert_order_with_val(other_side, order_ptr);
				}
				break;
			}
		}
	}
}
