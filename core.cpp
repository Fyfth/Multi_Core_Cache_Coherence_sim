#include "bus.h"
#include "setAssociativeCache.h"
#include "lru.h"
#include "core.h"
#include "latency.h"

#include <list> 
#include <unordered_map> 
#include <iostream>
#include <list>
#include <cstring>

using namespace std;

void Core::write(uint32_t address, uint32_t data){
    STATE currentState = INVALID;
    if(L1->contains(address)){
        currentState = L1->getState(address);
    } else if(L2->contains(address)){
        currentState = L2->getState(address);
    } else if(L3->contains(address)){
        currentState = L3->getState(address);
    }

    // step 1: write to L1
    L1->write(address, data);

    // step 2: push to L2/L3 AND RAM immediately
    CacheLine* l1Line = L1->sets[L1->set(address)].get(L1->tag(address));
    if(l1Line != nullptr){
        L2->writeBlock(address, l1Line->data, EXCLUSIVE);
        L3->writeBlock(address, l1Line->data, EXCLUSIVE);
        // keep RAM in sync
        for(int i = 0; i < 64; i++){
            setAssociativeCache::RAM[address + i] = l1Line->data[i];
        }
    }

    // step 3: invalidate other cores
    if(currentState != MODIFIED){
        b->writeBus(address, core_id);
    }
}

int Core::read(uint32_t address){
    // check all levels before reading
    if(L1->contains(address)){
        STATE s = L1->getState(address);
    }
    if(L3->contains(address)){
        STATE s = L3->getState(address);
    }
    return L1->read(address);
}
void Core::printStats(){
    cout << "========== CORE " << core_id << " ==========\n\n";
    
    // per level stats
    L1->printStats("L1");
    L2->printStats("L2");
    L3->printStats("L3");
    
    // AMAT
    cout << "AMAT: " << calculateAMAT() << " cycles\n\n";
    
    // coherence overhead
    cout << "=== Coherence Overhead ===\n";
    cout << "Invalidations received:  " << L3->coherenceInvalidations << "\n";
    cout << "Downgrades received:     " << L3->coherenceDowngrades    << "\n";
    cout << "Wasted snoops:           " << L3->wastedSnoops           << "\n";
    cout << "Invalidation→Read:       " << L3->invalidationToRead     << "\n\n";
}

float Core::calculateAMAT(){
    // AMAT = L1 hit time + L1 miss rate * (L2 hit time + L2 miss rate * (L3 hit time + L3 miss rate * RAM penalty))
    
    float l1Total = L1->hits + L1->misses;
    float l2Total = L2->hits + L2->misses;
    float l3Total = L3->hits + L3->misses;
    
    float l1MissRate = l1Total > 0 ? (float)L1->misses / l1Total : 0;
    float l2MissRate = l2Total > 0 ? (float)L2->misses / l2Total : 0;
    float l3MissRate = l3Total > 0 ? (float)L3->misses / l3Total : 0;
    
    float ramPenalty = RAM_CYCLES;
    float l3Penalty = L3_HIT_CYCLES + (l3MissRate * ramPenalty);
    float l2Penalty = L2_HIT_CYCLES + (l2MissRate * l3Penalty);
    float amat = L1_HIT_CYCLES + (l1MissRate * l2Penalty);
    
    return amat;
}

