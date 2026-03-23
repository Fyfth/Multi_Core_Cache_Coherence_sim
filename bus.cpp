#include "bus.h"
#include "setAssociativeCache.h"
#include "lru.h"
#include "latency.h"
#include <list> 
#include <unordered_map> 
#include <iostream>
#include <list>
#include <cstring>
#include "bloomfilter.h"

using namespace std;

void bus::addListener(setAssociativeCache* L3){
    listeners.push_back(L3);
}

pair<bool,uint8_t*> bus::readBus(uint32_t address, int core_id){
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

        // bloom filter check before snooping
        if(!listeners[i]->bloomFilter.mightContain(address)){
            bloomPrevented++;
            listeners[i]->bloomFilter.wastedSnoopsPrevented++;
            continue;  // skip snoop entirely
        }

        pair<STATE, uint8_t*> snoopResult = listeners[i]->snoop(address,0); 
        
        if(snoopResult.first == INVALID){
            wastedSnoops++;  //did not have tag
        }
        if (snoopResult.first != INVALID){//Can't return early, first match sure, but every core needs to "downgrade" by calling snoop on itself
            isShared = true; 
        }
        if(snoopResult.first == MODIFIED||snoopResult.first == EXCLUSIVE){
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
        totalSnoopsIssued++;
        totalSnoopCycles += BUS_CYCLES;

        // bloom filter check
        if(!listeners[i]->bloomFilter.mightContain(address)){
            bloomPrevented++;
            listeners[i]->bloomFilter.wastedSnoopsPrevented++;
            continue;
        }

        auto result = listeners[i]->snoop(address,1);
        if(result.first == INVALID){
            wastedSnoops++;
        }
    }
}

void bus::printStats(){
    cout << dec;
    cout << "========== BUS Stats ==========\n";
    cout << "Read transactions:   " << readTransactions  << "\n";
    cout << "Write transactions:  " << writeTransactions << "\n";
    cout << "Total snoops issued: " << totalSnoopsIssued << "\n";
    cout << "Total snoop cycles:  " << totalSnoopCycles  << "\n";
    if(totalSnoopsIssued > 0){
        cout << "Avg snoop latency:   "<< (float)totalSnoopCycles/totalSnoopsIssued << " cycles\n";
    }
    
    // sum wasted snoops across all listeners
    cout << "Wasted snoops:       " << wastedSnoops << "\n";
    if(totalSnoopsIssued > 0){
        cout << "Wasted snoop ratio:  " <<(float)wastedSnoops/totalSnoopsIssued * 100 << "%\n";
    }
    int totalPrevented = 0;
    for(auto l : listeners) totalPrevented += l->bloomFilter.wastedSnoopsPrevented;

    cout << "Snoops issued:          " << totalSnoopsIssued << "\n";
    cout << "Snoops prevented:       " << bloomPrevented << "\n";
    cout << "Snoops issued+prevented:" << totalSnoopsIssued + bloomPrevented << "\n";
    cout << "Wasted (false positive):" << wastedSnoops << "\n";
    cout << "Bloom filter savings:   " 
        << (float)bloomPrevented/(totalSnoopsIssued + bloomPrevented) * 100
        << "% of total bus traffic\n";
}

