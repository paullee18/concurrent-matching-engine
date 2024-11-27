# Overview
This was a school project to make a concurrent stock matching engine in C++.
The files I wrote are:
- order.cpp: The RestingOrder class which represents an order in the exchange
- orderbook.cpp: The Orderbook class handles the addition or filling of orders
- ts_orderbook_hashmap.hpp: This defines a simplified concurrent hashmap with chaining using one mutex per bucket
- engine.cpp: The Engine class which handles the logic when new orders are received

# To Run
There is a provided makefile.

The engine is in engine.cpp. To run it, run e.g ./engine socket.

The client is in client.cpp. To run it, run e.g. ./client socket. The client will read commands from standard input in the following format:

Create buy order
B <Order ID> <Instrument> <Price> <Count>
Create sell order
S <Order ID> <Instrument> <Price> <Count>
Cancel order
C <Order ID>

# The assignment writeup
## Data Structures

We wrote our own thread safe implementation of a hashmap by using a vector with a linked list in each vector index for chaining. This allowed us to use fine-grained locking by also
storing a vector of locks for each bucket. Within each bucket, a shared mutex is used so that multiple reads can be made concurrently and only 1 write can be made concurrently. This means that operations can be made concurrently to different vector buckets. The intention was to use a large enough bucket count, 1000, such that each bucket would store one value, then using chaining as a second resort. This thread safe hashmap was used for mapping instruments to orderbooks (and for mapping order_ids to RestingOrders for cancels). This meant that we could insert / retrieve orderbooks for different instruments concurrently and thus orders for different instruments are able to run concurrently.

For each orderbook, we stored a sell priority queue and a buy priority queue separately.
Higher priority is given to sell orders with lower price while higher priority is given to buy orders with higher price. For orders with the same price, priority is given to the earlier added order. Storing sell and buy orders separately allowed us to execute a buy and a sell order
concurrently. This is because a buy order only tries to match against resting sell orders in the sell heap and vice versa, thus a buy and a sell order can concurrently try to match against resting orders.


## Synchronisation Primitives

The implementation of concurrency control mechanisms in our system incorporates atomics, mutexes, and condition variables to ensure thread safety and correctness.

Atomics allow us to read and write from a counter atomically, ensuring that multiple threads reading and writing are able to do so safely without data races. We used atomics for generating timestamps for all of our Order operations (Execute, cancel, add) and the
execution ID of a resting order.

Mutexes were heavily used to protect our critical sections and we chose to specifically use shared_mutex whenever possible. Shared_mutex allowed us to use unique_lock for writes
and shared_lock for reads, which meant that multiple reads can occur concurrently, whereas a write will prevent any other read/write from happening concurrently. Shared mutexes were used for our hashmaps and priority_queue functions to ensure their correctness, for e.g to prevent popping from an empty priority_queue. We also made use of mutexes to ensure that attributes of our RestingOrders are retrieved/written to safely and correctly.

We also made use of a buyMutex and a sellMutex to ensure that only 1 buy order and 1 sell order for an orderbook can execute concurrently. Lastly, a mutex called incoming_order_mutex was used in each orderbook to ensure that there is an initial sequential portion that is run for each incoming order to an orderbook, this was vital for correctness.

Condition variables were utilised to allow for an order to wait for another concurrently
executing order to notify when it is ready, instead of having to busy wait. This was done by
storing a condition variable in each resting order, along with a boolean is_ready for signalling if an order is ready to be executed against, and a mutex to protect this boolean.
 


## Level Of Concurrency

In our program, we implemented phase level concurrency, which allows orders with opposing sides (i.e., one buy and one sell) to match concurrently. Here is the flow for buy/sell orders:

1.	When a new order comes in, we look for its orderbook or create one, this is concurrent with other orders from different instruments, as explained above.
2.	After finding its respective orderbook, the thread locks the buyMutex/sellMutex,
ensuring that no other same side order for this instrument is executing concurrently.
3.	The thread locks the incoming_order_mutex stored in its orderbook to perform a small initialisation process that has to be sequential. In this process, a timestamp is taken
and is used to dictate ordering for orders. Then, the order is added to its respective priority queue with the flag is_ready set to false. This is critical as an opposing
concurrent order might want to execute against this order. incoming_order_mutex is then unlocked.
4.	Now that incoming_order_mutex is released, our order is able to look for opposing resting orders to execute against by repeatedly popping from the opposing side’s priority queue. At the same time, opposing orders can run concurrently.
5.	Due to concurrency, our order can come across an order with a higher timestamp than it, and it will ignore this order when it compares it against its own timestamp.
6.	However, if it comes across an order whose is_ready flag is set to false, this means that there is a concurrent order that is still matching which has an attractive enough price. This is where the order uses the condition variable to wait for is_ready to be set to true. This is unavoidable for correctness and by only waiting for orders that we
want to execute against, we maximise concurrency. And of course to prevent deadlocks, orders only wait on orders of lower timestamp.
7.	After all order matching is complete, we generate our order adding output if it still has count left, we set our is_ready flag to true and notify all waiting threads.

Thus, orders for different instruments can execute concurrently. Apart from a small initial sequential portion, a buy and a sell for an instrument can match orders fully concurrently unless it reaches a point where one order has to wait and see if the other order will have a remaining count for it to use.

## Testing Methodology

The code is compiled with thread and address sanitisers to detect data races, deadlocks and memory leaks. We also hand wrote some of our concurrent test cases to specifically try to catch certain concurrency issues. This allowed us to find numerous bugs in the beginning,
such as races between cancels and buy/sells. We also wrote a test case generator which is able to generate as many concurrent test cases as we want. This allowed us to test extremely thoroughly to find issues that we could not think of.
