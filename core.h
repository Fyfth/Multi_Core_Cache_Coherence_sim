#ifndef CORE_H 
#define CORE_H

#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include <cmath>
#include "lru.h" 
#include "setAssociativeCache.h"
#include "bus.h"

class Core {
public:
    int core_id;
    bus* b;
    setAssociativeCache* L1;
    setAssociativeCache* L2;
    setAssociativeCache* L3;
    
    Core(int id, bus* busPtr){
        core_id = id;
        b = busPtr;
        
        //taking a guess at param
        L1 = new setAssociativeCache(4,  4,  nullptr, nullptr);
        L2 = new setAssociativeCache(8,  8,  nullptr, nullptr);
        L3 = new setAssociativeCache(16, 11, nullptr, nullptr);
        
        // wire hierarchy: L1 -> L2 -> L3
        L1->setNextLevel(L2);
        L2->setPrevLevel(L1);
        L2->setNextLevel(L3);
        L3->setPrevLevel(L2);
        
        // wire bus to L3 only
        L3->setBus(b, core_id);
    }
    
    ~Core(){
        delete L1;
        delete L2;
        delete L3;
    }
    void write(uint32_t address, uint32_t data);
    int read(uint32_t address);
    void printStats();
    float calculateAMAT();
    // void printAllCaches(std::string label){
    // std::cout << "\n====== CORE " << core_id << " cache state: " << label << " ======\n";
    // // L1->printCache("L1");
    // // L2->printCache("L2");
    // // L3->printCache("L3");
    // }
};

#endif