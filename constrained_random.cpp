#include "constrained_random.h"
#include "bus.h"
#include <iostream>
#include <unordered_map>
#include <vector>

using namespace std;

void constrainedRandomTest(Core* cores[], int numCores, int numOps, int seed, TestResult& result){
    srand(seed);
    result.seed = seed;
    unordered_map<uint32_t, uint32_t> ref;

    for(int i = 0; i < numOps; i++){ 
        int coreId    = rand() % numCores;
        int op        = rand() % 2;
        //Test 1: show MESI working under stress -> PASS
        //uint32_t addr = ((rand() % 16) * 0x100); //--> HAMMER same SET -> High coherence trffic, low wasted snoops
        
        //Test 2: Large pool, sparse sharing
        uint32_t addr = ((rand() % 256) * 0x100); //--> high wasted snoops!

        //Test 3: Very Sequential -> intuition for Prefetcher --> TBD LOL
        
        uint32_t data = (uint32_t)(rand() & 0xFFFFFFF0);

        if(op == 1){
            cores[coreId]->write(addr, data);
            ref[addr] = data;
        } else {
            if(ref.find(addr) == ref.end()){
                result.skipped++;
                continue;
            }
            int got      = cores[coreId]->read(addr);
            int expected = (int)ref[addr];
            if(got == expected){
                result.passes++;
            } else {
                result.fails++;
                //[debug]
                // only print on failure
                cout << "FAIL seed=" << seed
                     << " iter=" << dec << i
                     << " core=" << coreId
                     << " addr=0x" << hex << addr
                     << " expected=0x" << expected
                     << " got=0x" << got << dec << "\n";
            }
        }
        
    }
}

void runConstrainedRandom(Core* cores[], bus* b, int numCores, 
                          int numSeeds, int opsPerSeed){
    cout << "\n****** CONSTRAINED RANDOM TESTS ******\n";
    cout << "  Seeds: "    << dec<< numSeeds
         << "  Ops/seed: " << opsPerSeed
         << "  Cores: "    << numCores << "\n\n";

    int totalPass = 0, totalFail = 0, totalSkip = 0;
    vector<int> failedSeeds;

    for(int seed = 0; seed < numSeeds; seed++){
        // fresh cores each seed
        for(int i = 0; i < numCores; i++){
            delete cores[i];
            cores[i] = new Core(i, b);
        }
        b->listeners.clear();
        for(int i = 0; i < numCores; i++){
            b->addListener(cores[i]->L3);
        }

        TestResult r;
        constrainedRandomTest(cores, numCores, opsPerSeed, seed, r);
        
        totalPass += r.passes;
        totalFail += r.fails;
        totalSkip += r.skipped;
        if(r.fails > 0) failedSeeds.push_back(seed);
    }

    cout << "********************\n";
    cout << "  SCOREBOARD\n";
    cout << "********************\n";
    cout << "  PASS:    " << totalPass  << "\n";
    cout << "  FAIL:    " << totalFail  << "\n";
    cout << "  SKIPPED: " << totalSkip  << "\n";
    cout << "  TOTAL:   " << totalPass + totalFail << "\n";

    if(failedSeeds.empty()){
        cout << "\n  ALL SEEDS PASSED\n";
    } else {
        cout << "\n  FAILED SEEDS: ";
        for(int s : failedSeeds) cout << s << " ";
        cout << "\n";
    }
    cout << "********************\n";
}