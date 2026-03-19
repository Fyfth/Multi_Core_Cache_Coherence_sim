#ifndef SET_ASSOCIATIVE_CACHE_H 
#define SET_ASSOCIATIVE_CACHE_H

#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include <cmath>
#include "lru.h" 

class bus;
class setAssociativeCache {
    private: 
        std::vector<LRUCache> sets; 
        int numSets;
        int bitsOffsets = 6;
        int way;
        setAssociativeCache* nextLevel; 
        setAssociativeCache* prevLevel; 
        int bitSets;
        bus* busPtr = nullptr;
        int core_id = -1;
        static std::unordered_map<uint32_t, uint8_t> RAM; 

       
    
    public: 
        // performance counters
        int hits = 0;
        int misses = 0;
        int evictions = 0;
        int coherenceInvalidations = 0; // times snoop write was called
        int coherenceDowngrades = 0; // times snoop read downgraded state

        int totalCycles = 0;
        
        int invalidationToRead = 0; // times fetched from RAM after invalidation
        bool wasRecentlyInvalidated = false;// flag to track invalidation→read
        int wastedSnoops = 0; // snoop called but address not found

        setAssociativeCache(int numSets, int setSize, setAssociativeCache* prevLevel, setAssociativeCache* nextLevel); 
        void setPrevLevel(setAssociativeCache* prev);
        void setNextLevel(setAssociativeCache* next);
        void setBus(bus* b, int id);
        int set(uint32_t address); 
        int offset(uint32_t address); 
        int tag(uint32_t address); 
        bool contains(uint32_t address); 
        void ramWrite(uint32_t address, uint8_t* data);
        std::vector<uint8_t> ramRead(uint32_t address); 
        void evictIfNeeded(int setIdx); 
        void writeBlock(uint32_t address, uint8_t* dataptr); 
        void write(uint32_t address, uint32_t data); 
        uint8_t* readBlock(uint32_t address); 
        int read(uint32_t address); 
        void backInvalidate(uint32_t address); 
        std::pair<STATE, uint8_t*> snoop(uint32_t address, int type);
        void printStats(std::string levelName); 
}; 

#endif