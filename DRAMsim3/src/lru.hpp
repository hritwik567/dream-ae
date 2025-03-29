#pragma once
#include <iostream>
#include <vector>
class LRUCache {
private:
    struct CacheEntry
    {
        uint64_t addr;
        uint64_t last_access;
        bool valid;
        bool dirty;
    };
    int sets;
    int ways;
    std::vector<CacheEntry> cache;
    uint64_t access_count;
    int hits;
    int misses;
    int reads;
    int writes;
    int writebacks;
public:
    LRUCache() = default;

    void update_size(int _sets, int _ways)
    {
        sets = _sets;
        ways = _ways;
        cache.resize(sets * ways);
        access_count = 0;
        hits = 0;
        misses = 0;
        reads = 0;
        writes = 0;
        writebacks = 0;
    }

    int get_index(uint64_t addr)
    {
        return addr % sets;
    }

    int64_t read(uint64_t addr, uint64_t _set)
    {
        // std::cout << "Read: addr: " << std::hex << addr << std::dec << " set: " << std::hex << _set << std::dec << std::endl;
        int64_t writeback_addr = -1;
        reads++;
        int set = get_index(_set);
        // std::cout << "Set: " << set << std::endl;
        for (int i = 0; i < ways; i++)
        {
            if (cache[set * ways + i].addr == addr && cache[set * ways + i].valid)
            {
                hits++;
                cache[set * ways + i].last_access = access_count++;
                // std::cout << "Hit" << std::endl;
                return 0; // Cache hit
            }
        }
        // std::cout << "Miss" << std::endl;
        misses++;
        int victim = find_victim(set);
        if (cache[set * ways + victim].valid and cache[set * ways + victim].dirty)
        {
            writeback_addr = cache[set * ways + victim].addr;
            writeback(set, victim);
        }

        cache[set * ways + victim].addr = addr;
        cache[set * ways + victim].valid = true;
        cache[set * ways + victim].last_access = access_count++;
        cache[set * ways + victim].dirty = false;
        return writeback_addr;
    }

    int64_t write(uint64_t addr, uint64_t _set)
    {
        // std::cout << "Write: addr: " << std::hex << addr << std::dec << " set: " << std::hex << _set << std::dec << std::endl;
        int64_t writeback_addr = -1;
        writes++;
        int set = get_index(_set);
        // std::cout << "Set: " << set << std::endl;
        for (int i = 0; i < ways; i++)
        {
            if (cache[set * ways + i].addr == addr && cache[set * ways + i].valid)
            {
                cache[set * ways + i].last_access = access_count++;
                cache[set * ways + i].dirty = true;
                return 0; // Cache hit
            }
        }
        int victim = find_victim(set);
        if (cache[set * ways + victim].valid and cache[set * ways + victim].dirty)
        {
            writeback_addr = cache[set * ways + victim].addr;
            writeback(set, victim);
        }
        cache[set * ways + victim].addr = addr;
        cache[set * ways + victim].valid = true;
        cache[set * ways + victim].dirty = true;
        cache[set * ways + victim].last_access = access_count++;
        return writeback_addr;
    }

    uint32_t find_victim(int set)
    {
        // Firts look for invalid entries
        for (int i = 0; i < ways; i++)
        {
            if (!cache[set * ways + i].valid)
            {
                return i;
            }
        }

        // If all entries are valid, evict the least recently used
        uint32_t victim = 0;
        uint64_t min_access = cache[set * ways].last_access;
        for (int i = 1; i < ways; i++)
        {
            if (cache[set * ways + i].last_access < min_access)
            {
                min_access = cache[set * ways + i].last_access;
                victim = i;
            }
        }
        return victim;
    }

    void writeback(int set, int way)
    {
        writebacks++;
        cache[set * ways + way].valid = false;
    }

    void print_stats()
    {
        std::cout << "Hits: " << hits << std::endl;
        std::cout << "Misses: " << misses << std::endl;
        std::cout << "Reads: " << reads << std::endl;
        std::cout << "Writes: " << writes << std::endl;
        std::cout << "Writebacks: " << writebacks << std::endl;
    }
};