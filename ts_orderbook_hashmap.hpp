#ifndef TS_ORDERBOOK_HASHMAP_HPP
#define TS_ORDERBOOK_HASHMAP_HPP

#include <vector>
#include <memory>
#include <list>
#include <utility>
#include <shared_mutex>

template<typename K, typename V>
class ts_orderbook_hashmap {
private:
    const int bucket_count = 1000;
    std::vector<std::list<std::pair<K, std::shared_ptr<V>>>> vec;
public:
    std::vector<std::shared_mutex> mutexes;
    ts_orderbook_hashmap() : vec(bucket_count), mutexes(bucket_count) {}
    int hash(const K &key) {
        return std::hash<K>{}(key) % bucket_count;
    }

    void insert(const K &key, std::shared_ptr<V> ptr) {
        int index = hash(key);
        std::unique_lock<std::shared_mutex> lock(mutexes[index]);
        vec[index].emplace_back(std::make_pair(key,ptr));
    }

    void insert_if_not_exist(const K &key) {
        int index = hash(key);
        std::unique_lock<std::shared_mutex> lock(mutexes[index]);
        for (auto it = vec[index].begin(); it != vec[index].end(); it++) {
            if (it->first == key) {
                return;
            }
        }
        vec[index].emplace_back(std::make_pair(key,std::make_shared<V>(key)));
    }

    std::shared_ptr<V> get(const K &key) {
        int index = hash(key);
        std::shared_lock<std::shared_mutex> lock(mutexes[index]);
        for (auto it = vec[index].begin(); it != vec[index].end(); it++) {
            if (it->first == key) {
                return it->second;
            }
        }
        return nullptr;
    }

    bool exists(const K &key) {
        int index = hash(key);
        std::shared_lock<std::shared_mutex> lock(mutexes[index]);
        for (auto it = vec[index].begin(); it != vec[index].end(); it++) {
            if (it->first == key) {
                return true;
            }
        }
        return false;
    }

    void erase(const K &key) {
        int index = hash(key);
        std::shared_lock<std::shared_mutex> lock(mutexes[index]);
        for (auto it = vec[index].begin(); it != vec[index].end(); it++) {
            if (it->first == key) {
                vec[index].erase(it);
                return;
            }
        }
    }
};

#endif
