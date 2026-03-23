#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

#include <vector>
#include <cstdint>

class BloomFilter {
private:
    std::vector<bool> bits;
    int size;

    int hash1(uint32_t addr){ return (addr) % size; }
    int hash2(uint32_t addr){ return (addr * 2654435761u) % size; }
    int hash3(uint32_t addr){ return (addr ^ (addr >> 16)) % size; }

public:
    int wastedSnoopsPrevented = 0;  // stat tracking

    BloomFilter(int size = 32768) : bits(size, false), size(size){}
    //size of L1 -> 4 sets of 4 ways 
    //4*64*8*4 = 8192

    //size of L2 -> 8 sets of 8 ways 
    //8*64*8*8 = 32786

    void insert(uint32_t address){
        bits[hash1(address)] = true;
        bits[hash2(address)] = true;
        bits[hash3(address)] = true;
    }

    bool mightContain(uint32_t address){
        return bits[hash1(address)] &&
               bits[hash2(address)] &&
               bits[hash3(address)];
        // false = Definitely not present --> safe to skip snoop
        // true  = Maybe present --> do full snoop
    }

    void reset(){
        std::fill(bits.begin(), bits.end(), false);
    }
};

#endif