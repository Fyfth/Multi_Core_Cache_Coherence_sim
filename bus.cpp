#include "bus.h"
#include "setAssociativeCache.h"
#include "lru.h"
#include "latency.h"
#include <list> 
#include <unordered_map> 
#include <iostream>
#include <list>
#include <cstring>

using namespace std;

void bus::addListener(setAssociativeCache* L3){
    listeners.push_back(L3); //object exists -> pass by reference 
}

pair<bool,uint8_t*> bus::readBus(uint32_t address, int core_id){//the bus SHOULDN'T do the RAMRead -> leave it for Cache -> bool isShared
    //2 cases: 1. does other cores have dirty data? 2. no dirty data, just read from RAM
    readTransactions++;
    uint8_t* dataPtr= nullptr; 
    bool isShared = false; 
    for(int i =0; i<listeners.size(); i++){
        if(i==core_id){
            continue; 
        }
        totalSnoopsIssued++;
        totalSnoopCycles += BUS_CYCLES; // each snoop cycle costs
        pair<STATE, uint8_t*> snoopResult = listeners[i]->snoop(address,0); 
        if (snoopResult.first != INVALID){//Can't return early, first match sure, but every core needs to "downgrade" by calling snoop on itself
            isShared = true; 
        }
        if(snoopResult.first == MODIFIED){
            dataPtr = snoopResult.second; 
        }
    }
    return {isShared,dataPtr}; 
}

void bus::writeBus(uint32_t address, int core_id){
    writeTransactions++;
    for(int i = 0; i<listeners.size(); i++){
        if(i == core_id){
            continue; 
        }
        listeners[i]->snoop(address,1); 
    }
}

void bus::printStats(){
    cout << "========== BUS Stats ==========\n";
    cout << "Read transactions:   " << readTransactions  << "\n";
    cout << "Write transactions:  " << writeTransactions << "\n";
    cout << "Total snoops issued: " << totalSnoopsIssued << "\n";
    cout << "Total snoop cycles:  " << totalSnoopCycles  << "\n";
    if(totalSnoopsIssued > 0){
        cout << "Avg snoop latency:   " 
             << (float)totalSnoopCycles/totalSnoopsIssued 
             << " cycles\n";
    }
    cout << "Wasted snoop ratio:  ";
    // sum wasted snoops across all listeners
    int totalWasted = 0;
    for(auto l : listeners) totalWasted += l->wastedSnoops;
    if(totalSnoopsIssued > 0){
        cout << (float)totalWasted/totalSnoopsIssued * 100 << "%\n";
    }
    cout << "\n";
}

