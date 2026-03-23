#include "bus.h"
#include "core.h"
#include "constrained_random.h"
#include <iostream>

using namespace std;

int main(){
    bus* b = new bus();

    Core* core0 = new Core(0, b);
    Core* core1 = new Core(1, b);
    Core* core2 = new Core(2, b);

    b->addListener(core0->L3);
    b->addListener(core1->L3);
    b->addListener(core2->L3);

    Core* coreArr[] = {core0, core1, core2};

    // directed tests
    cout << "\n========== TEST 1: Basic Read/Write ==========\n";
    // core0 writes then reads same address
    core0->write(0x100, 0xDEADBEEF);
    int r1 = core0->read(0x100);
    cout << (r1 == (int)0xDEADBEEF ? "PASS" : "FAIL") 
         << " expected=deadbeef got=" << hex << r1 << "\n";

    cout << "\n========== TEST 2: Same Core Write Twice ==========\n";
    // overwrite same address, should get new value
    core0->write(0x100, 0x11111111);
    int r2 = core0->read(0x100);
    cout << (r2 == (int)0x11111111 ? "PASS" : "FAIL")
         << " expected=11111111 got=" << hex << r2 << "\n";

    cout << "\n========== TEST 3: Cross-Core Read ==========\n";
    // core0 writes, core1 reads
    core0->write(0x200, 0xCAFEBABE);
    int r3 = core1->read(0x200);
    cout << (r3 == (int)0xCAFEBABE ? "PASS" : "FAIL")
         << " expected=cafebabe got=" << hex << r3 << "\n";

    cout << "\n========== TEST 4: Write Contention ==========\n";
    // core0 writes, core1 overwrites, core0 reads new value
    core0->write(0x300, 0x11111111);
    core1->write(0x300, 0x22222222);
    int r4 = core0->read(0x300);
    cout << (r4 == (int)0x22222222 ? "PASS" : "FAIL")
         << " expected=22222222 got=" << hex << r4 << "\n";

    cout << "\n========== TEST 5: Three Core Contention ==========\n";
    // all three cores write same address, last writer wins
    core0->write(0x400, 0xAAAAAAAA);
    core1->write(0x400, 0xBBBBBBBB);
    core2->write(0x400, 0xCCCCCCCC);
    int r5a = core0->read(0x400);
    int r5b = core1->read(0x400);
    int r5c = core2->read(0x400);
    cout << (r5a == (int)0xCCCCCCCC ? "PASS" : "FAIL")
         << " core0 expected=cccccccc got=" << hex << r5a << "\n";
    cout << (r5b == (int)0xCCCCCCCC ? "PASS" : "FAIL")
         << " core1 expected=cccccccc got=" << hex << r5b << "\n";
    cout << (r5c == (int)0xCCCCCCCC ? "PASS" : "FAIL")
         << " core2 expected=cccccccc got=" << hex << r5c << "\n";

    cout << "\n========== TEST 6: False Sharing ==========\n";
    // different addresses but same cache line
    // 0x500 and 0x504 are 4 bytes apart, same 64 byte cache line
    core0->write(0x500, 0xAAAAAAAA);
    core1->write(0x504, 0xBBBBBBBB);
    int r6a = core0->read(0x500);
    int r6b = core1->read(0x504);
    cout << (r6a == (int)0xAAAAAAAA ? "PASS" : "FAIL")
         << " core0 expected=aaaaaaaa got=" << hex << r6a << "\n";
    cout << (r6b == (int)0xBBBBBBBB ? "PASS" : "FAIL")
         << " core1 expected=bbbbbbbb got=" << hex << r6b << "\n";

    cout << "\n========== TEST 7: Ping Pong ==========\n";
    // cores alternately write same address
    // tests repeated invalidation
    core0->write(0x600, 0x00000001);
    core1->write(0x600, 0x00000002);
    core0->write(0x600, 0x00000003);
    core1->write(0x600, 0x00000004);
    int r7 = core0->read(0x600);
    cout << (r7 == 0x00000004 ? "PASS" : "FAIL")
         << " expected=00000004 got=" << hex << r7 << "\n";

    cout << "\n========== TEST 8: Temporal Locality ==========\n";
    // same address accessed many times
    // after first miss should be all hits
    core0->write(0x700, 0x12345678);
    core0->read(0x700);   // miss
    core0->read(0x700);   // should hit L1
    core0->read(0x700);   // should hit L1
    int r8 = core0->read(0x700);
    cout << (r8 == (int)0x12345678 ? "PASS" : "FAIL")
         << " expected=12345678 got=" << hex << r8 << "\n";

    cout << "\n========== TEST 9: Read Then Write ==========\n";
    // core0 reads (gets EXCLUSIVE)
    // core1 reads same (both SHARED)
    // core0 writes (invalidates core1)
    // core1 reads again (should get new value)
    core0->write(0x800, 0xDEAD0000);
    int r9a = core1->read(0x800);   // core1 gets SHARED copy
    core0->write(0x800, 0xDEAD1111); // core0 invalidates core1
    int r9b = core1->read(0x800);   // core1 should get new value
    cout << (r9a == (int)0xDEAD0000 ? "PASS" : "FAIL")
         << " core1 first read expected=dead0000 got=" << hex << r9a << "\n";
    cout << (r9b == (int)0xDEAD1111 ? "PASS" : "FAIL")
         << " core1 second read expected=dead1111 got=" << hex << r9b << "\n";

    cout << "\n========== TEST 10: Multiple Addresses ==========\n";
    // make sure different addresses don't interfere
    core0->write(0x900, 0x11111111);
    core0->write(0x940, 0x22222222);
    core0->write(0x980, 0x33333333);
    int r10a = core0->read(0x900);
    int r10b = core0->read(0x940);
    int r10c = core0->read(0x980);
    cout << (r10a == (int)0x11111111 ? "PASS" : "FAIL")
         << " expected=11111111 got=" << hex << r10a << "\n";
    cout << (r10b == (int)0x22222222 ? "PASS" : "FAIL")
         << " expected=22222222 got=" << hex << r10b << "\n";
    cout << (r10c == (int)0x33333333 ? "PASS" : "FAIL")
         << " expected=33333333 got=" << hex << r10c << "\n";

    cout << "\n========== TEST 11: Eviction ==========\n";
    // hammer set 0 of L1 with 5 addresses (4-way, so 5th triggers eviction)
    // these all map to set 0: 0x000, 0x100, 0x200, 0x300, 0x400
    core0->write(0x000, 0x00000001);
    core0->write(0x100, 0x00000002);
    core0->write(0x200, 0x00000003);
    core0->write(0x300, 0x00000004);
    // L1 set 0 is now full (4 ways)

    core0->write(0x400, 0x00000005);
    // this should evict 0x000 (LRU) from L1
    // 0x000 is dirty (MODIFIED) so should writeback to L2

    // now read 0x000 — should miss L1, hit L2
    int r11 = core0->read(0x000);
    cout << (r11 == 0x00000001 ? "PASS" : "FAIL")
        << " evicted line readable from L2, expected=1 got=" << hex << r11 << "\n";

    // and 0x400 should still be in L1
    int r11b = core0->read(0x400);
    cout << (r11b == 0x00000005 ? "PASS" : "FAIL")
     << " newest line still in L1, expected=5 got=" << hex << r11b << "\n";
    
    cout << "\n========== TEST 12: Eviction + Cross-Core Snoop ==========\n";
    // core0 fills L1 set 0 completely
    core0->write(0x000, 0xAAAA0001);
    core0->write(0x100, 0xAAAA0002);
    core0->write(0x200, 0xAAAA0003);
    core0->write(0x300, 0xAAAA0004);
    // L1 set 0 full — 0x000 is LRU

    // core1 reads 0x000 BEFORE eviction
    // core0 has it MODIFIED in L1
    // snoop should pull fresh data from core0
    int r12a = core1->read(0x000);
    cout << (r12a == (int)0xAAAA0001 ? "PASS" : "FAIL")
        << " core1 read before eviction, expected=aaaa0001 got=" << hex << r12a << "\n";

    // now core0 writes 0x400 → triggers eviction of 0x000 from L1
    // 0x000 is now SHARED (core1 has it) so writeback goes to L2
    core0->write(0x400, 0xAAAA0005);

    // core1 should still have correct value for 0x000
    int r12b = core1->read(0x000);
    cout << (r12b == (int)0xAAAA0001 ? "PASS" : "FAIL")
        << " core1 still correct after core0 eviction, expected=aaaa0001 got=" << hex << r12b << "\n";

    // now core0 writes NEW value to 0x000
    // this should invalidate core1's copy
    core0->write(0x000, 0xBBBB0001);

    // core1 reads 0x000 — should get new value
    int r12c = core1->read(0x000);
    cout << (r12c == (int)0xBBBB0001 ? "PASS" : "FAIL")
        << " core1 sees updated value after invalidation, expected=bbbb0001 got=" << hex << r12c << "\n";

    // core0 reads back its own write
    int r12d = core0->read(0x000);
    cout << (r12d == (int)0xBBBB0001 ? "PASS" : "FAIL")
        << " core0 reads own write, expected=bbbb0001 got=" << hex << r12d << "\n";

    // single seed, tiny ops, verbose
    cout << "*****Constrained Random*****"<<"\n";

    TestResult r;
    runConstrainedRandom(coreArr, b, 3, 20, 500);
   

    // stats
    coreArr[0]->printStats();
    coreArr[1]->printStats();
    coreArr[2]->printStats();
    b->printStats();

    delete coreArr[0];
    delete coreArr[1];
    delete coreArr[2];
    delete b;

    return 0;
}