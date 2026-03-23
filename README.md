# Multi-Core Cache Coherence Simulator

A multi-core cache hierarchy simulator implementing the **MESI coherence protocol** with LRU replacement, snooping bus, and inclusive write-back policy.

---

## Basics

**Cache Line:** 64 bytes  
**Physical Address:** 32 bits

| Field  | Bits | Formula |
|--------|------|---------|
| Offset | 6    | `address & ((1 << 6) - 1)` |
| Set    | log₂(#sets) | `(address >> 6) & (numSets - 1)` |
| Tag    | 32 - (Offset + Set) | `address >> (6 + bitSets)` |

---

## Cache Structure

![Cache Structure](cache_tree.png)

Each cache level is a **set-associative cache** backed by an LRU eviction policy.  
Every set is an `LRUCache` — a doubly linked list paired with a hash map for O(1) lookup and O(1) LRU updates.  
Each node in the list is a `CacheLine` struct holding a 64-byte data block, a tag, and a MESI state.

| Level | Sets | Ways | Size  |
|-------|------|------|-------|
| L1    | 4    | 4    | 1 KB  |
| L2    | 8    | 8    | 4 KB  |
| L3    | 16   | 11   | 11 KB |

---

## Cache Hierarchy

![Cache Hierarchy](cache_hierarchy.png)

Each core owns a **private** L1, L2, and L3. All L3s connect to a shared snooping bus.  
On a read miss, the request cascades down: L1 → L2 → L3 → Bus → RAM.  
On a write, the requesting core invalidates all other copies via the bus before modifying its L1.

---

## MESI Protocol

| State | Meaning |
|-------|---------|
| **M** odified  | Dirty, sole owner. Must write back before eviction. |
| **E** xclusive | Clean, sole owner. Can upgrade to M on write without bus transaction. |
| **S** hared    | Clean, multiple cores may have copies. Must notify bus before write. |
| **I** nvalid   | Line is not valid. Treated as a miss. |

---

## API

### `bus.h`
| Function | Description |
|----------|-------------|
| `addListener(L3)` | Register a core's L3 with the bus |
| `readBus(address, core_id)` | Snoop all other L3s on a read miss. Returns dirty data if found |
| `writeBus(address, core_id)` | Invalidate all other copies on a write |
| `printStats()` | Print bus traffic metrics |

### `core.h`
| Function | Description |
|----------|-------------|
| `write(address, data)` | CPU write — checks bus before modifying L1 |
| `read(address)` | CPU read — returns value from closest cache level |
| `printStats()` | Print per-level hit/miss rates and AMAT |
| `calculateAMAT()` | Compute Average Memory Access Time across all levels |

### `lru.h`
| Function | Description |
|----------|-------------|
| `get(key)` | O(1) lookup, moves line to MRU position |
| `put(key, data, state)` | Insert new line at MRU, evict LRU if full |
| `contains(key)` | Check if tag exists in set |
| `peek()` | View LRU victim without removing |
| `eviction()` | Remove and return LRU victim |
| `remove(tag)` | Remove specific line by tag |

### `setAssociativeCache.h`
| Function | Description |
|----------|-------------|
| `readBlock(address)` | Miss cascade — fetch from next level or bus |
| `write(address, data)` | Modify data in place, set state to MODIFIED |
| `writeBlock(address, data, state)` | Write a full 64-byte block, used during eviction and coherence |
| `evictIfNeeded(setIdx)` | LRU eviction with dirty writeback to next level |
| `backInvalidate(address)` | Pull fresh data from L1/L2 down before eviction |
| `flushToMe(address)` | Flush dirty data down WITHOUT removing lines — used by snoop |
| `snoop(address, type)` | Respond to bus read (type=0) or write (type=1) request |
| `invalidateUp(address)` | Invalidate L1/L2 after snoop write — ensures full coherence |
| `set/tag/offset(address)` | Address decomposition helpers |
| `printStats(levelName)` | Hit rate, miss rate, evictions, coherence events |
| `printCache(levelName)` | Dump all valid lines with state and data |

---

## Performance Analysis

Two workloads were evaluated using constrained random testing 
(20 seeds × 500 operations, 3 cores):

- **Heavy Contention:** 16 addresses, all cores competing for same lines
- **Sparse Workload:** 256 addresses, cores rarely share data

---

### Cache Hit Rates

| Level | Heavy Contention | Sparse Workload |
|-------|-----------------|-----------------|
| L1    | ~20%            | ~2%             |
| L2    | ~30%            | ~3%             |
| L3    | ~5%             | ~11%            |
| AMAT  | ~140 cycles     | ~220 cycles     |

**Observation:** Heavy contention keeps data in L3 through sharing.
Sparse workload causes constant cold misses — cores never benefit 
from each other's cached data.

---

### Coherence Overhead

| Metric | Heavy Contention | Sparse Workload |
|--------|-----------------|-----------------|
| Invalidations (per core) | ~120 | ~18 |
| Downgrades (per core) | ~65 | ~28 |
| Invalidation→Read | ~4 | ~8 |

**Observation:** Heavy contention generates 6x more invalidations — 
constant ownership transfers between cores. Sparse workload has 
fewer invalidations because cores rarely share the same lines.

---

### Bus Traffic & Bloom Filter Analysis

Bloom filter size: 32,768 bits (matching L2 cache size)  
Reset frequency: every 50 evictions

| Metric | Heavy Contention | Heavy + Bloom | Sparse | Sparse + Bloom |
|--------|-----------------|---------------|--------|----------------|
| Total Snoops Issued | 18,574 | 18,574 | 20,956 | 20,954 |
| Wasted Snoops | 8,233 | 5,997 | 17,873 | 5,003 |
| Wasted Snoop Ratio | 44.3% | 32.3% | 85.3% | 23.9% |
| Snoops Prevented | — | 2,237 | — | 12,967 |
| Bus Cycle Savings | — | 10.8% | — | 38.2% |

**Observation:** Bloom filter is dramatically more effective on sparse 
workloads (38% savings vs 11%). This matches intuition — sparse 
workloads have many cores with empty caches, so the bloom filter 
correctly identifies "definitely not present" most of the time.

**Limitation:** Standard bloom filter cannot delete entries on eviction. 
This causes false positives that accumulate over time, motivating a 
counting bloom filter as future work. Aggressive resets (< 50 evictions) 
cause coherence violations — demonstrating the correctness/performance 
tradeoff inherent in probabilistic filtering.

---
## Verification Methodology

### Directed Tests
12 hand-written tests targeting specific coherence scenarios in order of increasing complexity:

| Test | Scenario | What It Verifies |
|------|----------|-----------------|
| 1 | Basic read/write | Single core correctness |
| 2 | Same core write twice | LRU update path |
| 3 | Cross-core read | Read snoop, data transfer |
| 4 | Write contention | Write invalidation |
| 5 | Three-core contention | Last writer wins |
| 6 | False sharing | Same line, different words |
| 7 | Ping pong | Repeated invalidation |
| 8 | Temporal locality | L1 hit after warmup |
| 9 | Read then write | SHARED → MODIFIED transition |
| 10 | Multiple addresses | No aliasing bugs |
| 11 | Eviction | LRU eviction + dirty writeback |
| 12 | Eviction + cross-core snoop | Combined eviction and coherence |

### Constrained Random Verification

A self-checking testbench with a reference model — the same methodology used in industry CPU design verification.

**How it works:**
```
srand(seed) → reproducible random sequence
↓
Random: core id, operation (read/write), address from constrained pool
↓
On write: update cache AND reference model (simple hashmap)
On read:  compare cache result against reference model
↓
Mismatch = coherence bug
```

**Key properties:**
- **Reproducible:** seed-based random — any failing seed can be re-run identically for debugging
- **Self-checking:** reference model tracks ground truth after every operation
- **Constrained:** address pool controls sharing behavior (small pool = high contention, large pool = sparse)
- **Sequential:** reference model updates after every op, so state carries forward correctly across operations

**Results:**
```
20 seeds × 500 operations = 10,000 operations
4,686 verified read transactions
0 failures
```

Constrained random found **4 coherence bugs** that directed tests missed — all requiring 80+ sequential operations to trigger, involving interactions between eviction, cross-core invalidation, and LRU state that no hand-written test would think to cover.

---
## Bug Report

9 coherence bugs found and fixed during development. Each bug is documented with root cause and fix.

### Bug 1: `backInvalidate` in `snoop` → SEGFAULT
**Root cause:** `snoop` called `backInvalidate` to flush dirty data from L1/L2 to L3 before responding to the bus. But `backInvalidate` removes lines as it flushes. The L3 line disappeared before snoop could read its state — null pointer dereference.  
**Fix:** Created `flushToMe()` — flushes dirty data down without removing lines.

### Bug 2: `writeBus` never firing
**Root cause:** `writeBus` was called from `setAssociativeCache::write()` where `busPtr == nullptr` on L1. The bus pointer is only set on L3. So the `busPtr != nullptr` check always failed — writeBus never executed.  
**Fix:** Moved writeBus call to `Core::write()` where the bus pointer is directly accessible.

### Bug 3: Snoop invalidated L3 but not L1/L2
**Root cause:** `writeBus` snoops L3 and invalidates it. But L1/L2 were never told. They still held stale copies — subsequent L1 hit returned wrong value.  
**Fix:** Added `invalidateUp()` — called from snoop write case, travels L3→L2→L1 setting all copies INVALID.

### Bug 4: INVALID lines served as cache hits
**Root cause:** `readBlock` checked `contains(tagValue)` for the hit path. INVALID lines physically remain in the set — `contains()` returns true. Stale data was being returned as valid hits.  
**Fix:** Added state check before returning hit:
```cpp
if(line->state == INVALID) misses++;  // fall through
else { hits++; return line->data; }
```

### Bug 5: `writeBus` fires before `readBlock` fetches data
**Root cause:** `Core::write` called writeBus before `L1->write()` ran readBlock. writeBus invalidated other cores first — then readBlock tried to fetch from those now-invalidated cores and got nothing, falling through to stale RAM.  
**Fix:** Swapped order — readBlock fetches data first, then writeBus invalidates the source.

### Bug 6: `invalidateUp` writing stale data to RAM
**Root cause:** When invalidateUp ran on a core being invalidated, it wrote MODIFIED data to RAM before setting INVALID. But this data was stale — the writing core had already put the correct new value in RAM. The stale writeback overwrote the correct value.  
**Fix:** Removed RAM write from `invalidateUp` entirely. The writing core writes to RAM in Core::write Step 2. Invalidated cores just set INVALID — no writeback needed.

### Bug 7: `flushToMe` not writing to RAM from L3
**Root cause:** `flushToMe` only flushed MODIFIED data to `nextLevel`. For L3, `nextLevel == nullptr` — so L3 MODIFIED data was never written to RAM.  
**Fix:** Added L3 → RAM path in `flushToMe`:
```cpp
if(nextLevel != nullptr) nextLevel->writeBlock(address, line->data);
else ramWrite(address, line->data);  // L3 writes to RAM
line->state = SHARED;
```

### Bug 8: Snoop checking `original_state` instead of `line->state`
**Root cause:** `original_state` was captured BEFORE `flushToMe` ran. `flushToMe` could change the line state (e.g. EXCLUSIVE → MODIFIED by pulling fresh data from L1). Write case checked stale `original_state` and skipped RAM writeback.  
**Fix:** Write case now checks `line->state` (post-flushToMe) instead of `original_state`.

### Bug 9: `LRUCache::put()` ignoring state parameter ← root cause of constrained random failures
**Root cause:** `put()` update case hardcoded `MODIFIED` regardless of the state parameter passed:
```cpp
it->state = MODIFIED;  // ignores state parameter entirely
```
When `Core::write` Step 2 called `L2->writeBlock(address, data, EXCLUSIVE)`, put() ignored EXCLUSIVE and set MODIFIED. Next time `Core::write` checked L2 state, it found MODIFIED → skipped writeBus → other cores with SHARED copies never invalidated → stale reads.  
**Fix:**
```cpp
it->state = state;  // use passed state parameter
```
One line. Found by constrained random after 83 sequential operations — impossible to find with directed tests.

---

**Meta observation:** Every bug fell into one of three categories:
```
1. Wrong operation ordering  (Bugs 2, 5)
2. Missing base cases        (Bugs 3, 4, 7)
3. Stale state variables     (Bugs 6, 8, 9)
```
These are the same failure modes found in real CPU design verification.
---

## Build & Run

```bash
g++ -std=c++17 -o cache_sim bus.cpp core.cpp lru.cpp setAssociativeCache.cpp
./cache_sim
```

For debug output with address sanitizer:
```bash
g++ -std=c++17 -fsanitize=address -g -o cache_sim bus.cpp core.cpp lru.cpp setAssociativeCache.cpp
./cache_sim
```
