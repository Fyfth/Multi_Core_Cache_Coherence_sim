#include "lru.h"
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstring>
#include "setAssociativeCache.h"
#include "latency.h"
#include "bus.h"

using namespace std;
unordered_map<uint32_t, uint8_t> setAssociativeCache::RAM;

//must fetch on miss (write)!
    
setAssociativeCache::setAssociativeCache(int numSets, int setSize, setAssociativeCache* prevLevel, setAssociativeCache* nextLevel): numSets(numSets), way(setSize),prevLevel(prevLevel),nextLevel(nextLevel){
    for(int i=0; i<numSets; i++){
        sets.push_back(LRUCache(setSize));
    }
    this->bitSets = std::log2(numSets);
    for (uint32_t addr = 0; addr < 0x20000; addr ++) {  // Every byte; byte addresssable
        RAM[addr] = rand() % 256;  // Random "garbage" 2^8 = 256 0-255
    }
}

void setAssociativeCache::setPrevLevel(setAssociativeCache* prev) {
    prevLevel = prev;
}
void setAssociativeCache::setNextLevel(setAssociativeCache* next){
    nextLevel = next;
}

int setAssociativeCache::set(uint32_t address){
    int setIdx = (address >>bitsOffsets) & (numSets -1);
    return setIdx;
}

int setAssociativeCache::offset(uint32_t address){
    int offsetValue = address & ((1<<bitsOffsets)-1);
    return offsetValue;
}


int setAssociativeCache::tag(uint32_t address){
    int tagValue  = (address) >>(bitsOffsets + bitSets);
    return tagValue; 
}

bool setAssociativeCache::contains(uint32_t address){
    int setIdx = set(address); 
    int tagValue = tag(address);
    return sets[setIdx].contains(tagValue); 
}

void setAssociativeCache::ramWrite(uint32_t address, uint8_t* data){
    for (int i = 0; i < 64; i++) {
        RAM[address + i] = data[i];
    }
    //cout << "Snoop intercepted Modified snoopRead; line to RAM\n";
}


std::vector<uint8_t> setAssociativeCache::ramRead(uint32_t address){
    uint32_t blockStart = address & ~0x3F;  // Faster than modulo
    std::vector<uint8_t> blockData(64);  // Pre-allocate size
    for (int i = 0; i < 64; i++) {
        uint32_t currentAddr = blockStart + i;
        if (RAM.find(currentAddr) != RAM.end()) {
            blockData[i] = RAM[currentAddr]; 
        } else {
            blockData[i] = 0;
        }
    }
    return blockData;
}



void setAssociativeCache::evictIfNeeded(int set){
    if(sets[set].isFull()){//current set is full
        evictions++;

        // periodic bloom filter reset for L3
        if(nextLevel == nullptr && evictions % 50 == 0){
            bloomFilter.reset();
        }
        CacheLine temp  = sets[set].peek(); 
        uint32_t victimTag = temp.tag;
        uint32_t victimAddress = ((victimTag)<<(bitsOffsets + bitSets)) | (set << bitsOffsets);

        //upstream check
        if(prevLevel != nullptr){
            prevLevel->backInvalidate(victimAddress);   
        }                               

        CacheLine* victim = sets[set].get(victimTag); //update victim; cannot call evict/peek-> backinvalidate calls .put -> put in front; tag doesn't change
                        
        
        //downstream check
        if(nextLevel != nullptr){
            if(victim->state == MODIFIED){
                nextLevel->writeBlock(victimAddress, victim->data); 
                //cout << "Evicting dirty line to Next Level\n";
            }
        }else{
            // L3 to RAM
            if (victim->state == MODIFIED) {
                for (int i = 0; i < 64; i++) {
                    RAM[victimAddress + i] = victim->data[i];
                }
                // //cout << "Evicting dirty line to RAM\n";
                // cout << "wrote to RAM\n";
            }else{
                // cout << "NOT written to RAM (state not MODIFIED)\n";
            }
            
            
        }
        sets[set].remove(victimTag);
    }
}

void setAssociativeCache::writeBlock(uint32_t address, uint8_t* dataptr, STATE newState){
    int setIdx = set(address);
    int tagValue = tag(address);

    if(sets[setIdx].contains(tagValue)){
        CacheLine* line = sets[setIdx].get(tagValue);
        memcpy(line->data, dataptr, 64);
        line->state = newState; 
    }else{//miss, add, and update
        evictIfNeeded(setIdx); 
        sets[setIdx].put(tagValue, dataptr, newState);
    }

    // bloom filter
    if(nextLevel == nullptr){
        bloomFilter.insert(address);  // eviction
    }

}


void setAssociativeCache::write (uint32_t address, uint32_t data){// I always write to L1
    if (address % 4 != 0) {
        cout << "ERROR: Unaligned access! Address " << hex << address << " is not a multiple of 4.\n";
        return; // Stop

    }
    int tagValue = tag(address); 
    int setIdx = set(address);
    int offsetId = offset(address);

    //only L1 should call this; recursively brings cache line up 
    readBlock(address);
    CacheLine* line = sets[setIdx].get(tagValue);
    // if(line->state != MODIFIED && busPtr !=nullptr){
    //     busPtr->writeBus(address, core_id); 
    // }

    
    // cout << "write: address=" << hex << address 
    //  << " line state before write=" << line->state << "\n";
    if(sets[setIdx].contains(tagValue)){      
        //unpacketize
        for(int i =0; i<4; i++){
            //little endian write
            line->data[i+offsetId] =  (data >> (i * 8)) & 0xFF; //modifies in place
        }
        line->state =MODIFIED; 
        //cout<<"L1 HIT: Wrote 64 bytes data "<<" to address "<<address<<"\n";
    }
}

uint8_t* setAssociativeCache::readBlock (uint32_t address){//fetch the block to L1
    int tagValue = tag(address); 
    int setIdx = set(address); 

    if(sets[setIdx].contains(tagValue)){
        CacheLine* line = sets[setIdx].get(tagValue);
        // // debug
        // uint32_t val = 0;
        // for(int i=0;i<4;i++) val |= ((uint32_t)line->data[i]<<(i*8));
        // cout << "readBlock: addr=0x" << hex << address
        //      << " state=" << line->state
        //      << " data=0x" << val << dec << "\n";
        
        if(line->state == INVALID){
            misses++;  // INVALID = treat as miss, fall through to fetch
        } else {
            // real hit
            hits++;

            //bloom filter 
            if(nextLevel == nullptr){
                bloomFilter.insert(address);  //also insert on hit -> bloom filter reset
            }
            
            if(prevLevel == nullptr)       totalCycles += L1_HIT_CYCLES;
            else if(nextLevel == nullptr)  totalCycles += L3_HIT_CYCLES;
            else                           totalCycles += L2_HIT_CYCLES;

            if(wasRecentlyInvalidated){
                invalidationToRead++;
                wasRecentlyInvalidated = false;
            }
            return line->data;
        }
    } else {
        misses++;
    }

    vector<uint8_t> fetchedData(64); 
    STATE initState = EXCLUSIVE; 
    if(nextLevel !=nullptr){
        uint8_t* ptr = nextLevel->readBlock(address);
        memcpy(fetchedData.data(), ptr, 64); 

    }else{//I am L3, look for ram or snoop other cores 
        //if snoop returns dirty get it, else go to ram 
        
       
        auto[isShared,dataPtr] = busPtr->readBus(address, core_id);
        if(dataPtr !=nullptr){
            memcpy(fetchedData.data(), dataPtr, 64);
            initState = SHARED;
        }else{
            fetchedData = ramRead(address); 
            initState = isShared? SHARED:EXCLUSIVE; 
        }
    }

    //miss, evict, and add
    evictIfNeeded(setIdx); 
    sets[setIdx].put(tagValue,fetchedData.data(), initState); 
    if(nextLevel == nullptr){  // I am L3
        bloomFilter.insert(address);  // tell bloom filter we have this line
    }

    return sets[setIdx].get(tagValue)->data; //new data
}

int setAssociativeCache::read (uint32_t address){ //read the offset
    int offsetId = offset(address);

    uint8_t* block = readBlock(address); 

    int data = 0; 
    for(int i =0; i<4; i++){
        data |= ((block[offsetId+i])<<i*8); 
    }
    //cout << "CPU Read: " << hex << data << " from " << address << dec << "\n";
    return data;            

}

void setAssociativeCache::backInvalidate(uint32_t address){//look upward, copy downstream if dirty
    int tagValue = tag(address); 
    int setIdx = set(address); 

    if(prevLevel != nullptr){//not in L1 yet
        prevLevel->backInvalidate(address);
    }

    //In prevLevel (start with L1; end at L3) 
    if(sets[setIdx].contains(tagValue)){
        CacheLine* line = sets[setIdx].get(tagValue); 

        if(line->state==MODIFIED && nextLevel !=nullptr){//pass down
            nextLevel->writeBlock(address,line->data); 
        }

        sets[setIdx].remove(tagValue); 
    //bool isDirty; 
    }
}

pair<STATE, uint8_t*> setAssociativeCache::snoop(uint32_t address, int type){// pass in the address, type: 0 = read/1 = write 
    //assume only L3 calls this -> inclusive writeback
    int tagValue = tag(address); 
    int setIdx = set(address); 
    // cout << "SNOOP: addr=0x" << hex << address 
    //      << " type=" << type
    //      << " core_id=" << core_id
    //      << " contains=" << sets[setIdx].contains(tagValue) << dec << "\n";

    if(!sets[setIdx].contains(tagValue)){
        wastedSnoops++;
        
        // even if L3 doesn't have it, L1/L2 might have SHARED copies
        // on write snoop, kill them too
        if(type == 1 && prevLevel != nullptr){
            prevLevel->invalidateUp(address);
        }
        
        return {INVALID, nullptr};
    }
    CacheLine* line = sets[setIdx].get(tagValue);//fresh grab
    STATE original_state = line->state;
    uint8_t* data_flush = nullptr; 
    flushToMe(address); //pass down 
    line = sets[setIdx].get(tagValue);

    if(type ==0){//read --> snoop to "share state" 
            if(original_state == MODIFIED){
                coherenceDowngrades++;
                data_flush = new uint8_t[64];// Very important! Need to allocate space at DEST for the "stuff" to go in to 
                memcpy(data_flush, line->data, 64);//deep copy 
                line->state = SHARED;
                ramWrite(address, data_flush); // RAM GETS A COPY TOO!!!!!!!
            } else if (original_state == EXCLUSIVE) {
                coherenceDowngrades++;
                data_flush = new uint8_t[64];
                memcpy(data_flush, line->data, 64);
                line->state = SHARED; // Downgrade (no intercept needed, Read from Ram is fine)
            }else if(original_state == SHARED){
                // do nothing, RAM up to date, no data change needed 
            }
        }
    
    else{//write -> shared state wants to write -> must invalidate all other cores
        if(line->state ==MODIFIED){
            coherenceInvalidations++;
            data_flush = new uint8_t[64];
            memcpy(data_flush, line->data, 64); //save and return 
            ramWrite(address, data_flush);
        }
        line->state = INVALID;
        if(prevLevel != nullptr){
            prevLevel->invalidateUp(address);
        }
        coherenceInvalidations++;
        wasRecentlyInvalidated = true; // flag for next read

    }
    return {original_state, data_flush};
}
void setAssociativeCache::flushToMe(uint32_t address){
    int tagValue = tag(address);
    int setIdx   = set(address);

    if(prevLevel != nullptr){
        prevLevel->flushToMe(address);
    }

    if(sets[setIdx].contains(tagValue)){
        CacheLine* line = sets[setIdx].get(tagValue);
        if(line->state == MODIFIED){
            if(nextLevel != nullptr){
                nextLevel->writeBlock(address, line->data);
                line->state = SHARED;
            } else {
                // I am L3 — write to RAM
                ramWrite(address, line->data);
                line->state = SHARED;
            }
        }
    }
}
void setAssociativeCache::setBus(bus* b, int id){
    busPtr = b;
    core_id = id;
}
void setAssociativeCache::printStats(std::string levelName){//chatgpt
    int total = hits + misses;
    float hitRate = total > 0 ? (float)hits/total * 100.0f : 0;
    float missRate = total > 0 ? (float)misses/total * 100.0f : 0;
    
    cout << dec;
    cout << "*** " << levelName << " Stats ***\n";
    cout << "Total accesses:          " << total << "\n";
    cout << "Hits:                    " << hits << "\n";
    cout << "Misses:                  " << misses << "\n";
    cout << "Hit rate:                " << hitRate << "%\n";
    cout << "Miss rate:               " << missRate << "%\n";
    cout << "Evictions:               " << evictions << "\n";
    cout << "Coherence invalidations: " << coherenceInvalidations << "\n";
    cout << "Coherence downgrades:    " << coherenceDowngrades << "\n";
    cout << "\n";
}

void setAssociativeCache::printCache(std::string levelName){
    std::cout << "*** " << levelName << " ***\n";
    for(int i = 0; i < numSets; i++){
        if(sets[i].size() > 0){  // only print non-empty sets
            sets[i].printContents("set " + std::to_string(i));
        }
    }
}
STATE setAssociativeCache::getState(uint32_t address){
    int tagValue = tag(address);
    int setIdx = set(address);
    if(sets[setIdx].contains(tagValue)){
        return sets[setIdx].get(tagValue)->state;
    }
    return INVALID;
}
void setAssociativeCache::invalidateUp(uint32_t address){
    int tagValue = tag(address);
    int setIdx = set(address);
    
    // go to L1 first
    if(prevLevel != nullptr){
        prevLevel->invalidateUp(address);
    }
    
    // just invalidate, dirty data already handled by flushToMe
    if(sets[setIdx].contains(tagValue)){
        sets[setIdx].get(tagValue)->state = INVALID;
    }
}
