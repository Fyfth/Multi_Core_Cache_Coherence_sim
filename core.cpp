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
    L1->write(address, data);
}

int Core::read(uint32_t address){
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

int main(){
    bus* b = new bus();
    
    Core* core0 = new Core(0, b);
    Core* core1 = new Core(1, b);
    Core* core2 = new Core(2, b);
    
    // register L3s with bus
    b->addListener(core0->L3);
    b->addListener(core1->L3);
    b->addListener(core2->L3);
    
    // test 1: simple read/write
    core0->write(0x100, 0xDEADBEEF);
    core0->read(0x100);
    
    // test 2: true sharing
    // core0 writes, core1 reads same address
    core0->write(0x200, 0xCAFEBABE);
    core1->read(0x200);  // should get 0xCAFEBABE
    
    // test 3: write contention
    // core0 and core1 both write same address
    core0->write(0x300, 0x11111111);
    core1->write(0x300, 0x22222222);
    core0->read(0x300);  // should get 0x22222222

    // print stats
    core0->printStats();
    core1->printStats();
    core2->printStats();


    // bus level stats
    cout << "=== Bus Stats ===\n";
    cout << "Read transactions:  " << b->readTransactions << "\n";
    cout << "Write transactions: " << b->writeTransactions << "\n";
    
    delete core0;
    delete core1;
    delete core2;
    delete b;
}