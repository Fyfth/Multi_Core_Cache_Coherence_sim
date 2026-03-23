#ifndef BUS_H 
#define BUS_H

#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include <cmath>
#include "lru.h" 
#include "setAssociativeCache.h"

class setAssociativeCache;

class bus{
    private: 
               
    public: 
        void addListener(setAssociativeCache* L3);
        std::vector<setAssociativeCache*> listeners; //all L3's
        std::pair<bool, uint8_t*> readBus(uint32_t address, int core_id);
        void writeBus(uint32_t address, int core_id);
        void printStats();
        int totalSnoopCycles = 0;
        int readTransactions = 0;
        int writeTransactions = 0;
        int totalSnoopsIssued = 0;
        int wastedSnoops = 0;           // snoops issued but returned INVALID
        int bloomPrevented = 0;         // snoops skipped by bloom filter
}; 



    
    
        


#endif