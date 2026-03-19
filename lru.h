#ifndef LRU_H
#define LRU_H


//word granularity -> real cache offset 

#include <list> 
#include <unordered_map> 
#include <iostream>
#include <list>
#include <cstring>

enum STATE{
    INVALID = 0,   // deafult/empty
    SHARED = 1,    // clean/read-only; s-> broadcast change -> m 
    EXCLUSIVE = 2, // clean/read-write allowed; e-> m; optimization; goal: less bus traffic
    MODIFIED = 3   // Dirty / Write-Back needed
};

struct CacheLine{
    uint32_t tag; 
    uint8_t data[64]; //64 bytes per line 
    STATE state; 

    CacheLine(uint32_t t = 0, uint8_t* d = nullptr, STATE s = INVALID) : tag(t), state(s) {
        if (d != nullptr) {
            memcpy(data, d, 64);
        } else {
            memset(data, 0, 64);
        }
    }
};


class LRUCache{//operator overload -> doing than just dereferencing; deference still in iterator 
    private: 
    std::list<CacheLine> items; 
    std::unordered_map<uint32_t, std::list<CacheLine>::iterator> cacheMap; 
    int capacity;
    //void printCache();

    public: 
    LRUCache(int cap);           // Just declare, don't implement here
    CacheLine* get(uint32_t key);
    void put(uint32_t key, uint8_t value[64], STATE s = INVALID);
    bool contains(uint32_t key);
    int size();
    bool isFull();
    CacheLine eviction();
    void remove(int tag);
    CacheLine peek();
};
#endif
