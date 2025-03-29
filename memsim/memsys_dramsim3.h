#pragma once

#include <unordered_map>
#include "global_types.h"


#include "dramsim3.h"

/*
From dramsim3.h
class MemorySystem {
   public:
    MemorySystem(const std::string &config_file, const std::string &output_dir,
                 std::function<void(uint64_t)> read_callback,
                 std::function<void(uint64_t)> write_callback);
    ~MemorySystem();
    void ClockTick();
    void RegisterCallbacks(std::function<void(uint64_t)> read_callback,
                           std::function<void(uint64_t)> write_callback);
    double GetTCK() const;
    int GetBusBits() const;
    int GetBurstLength() const;
    int GetQueueSize() const;
    void PrintStats() const;
    void ResetStats();

    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write) const;
    bool AddTransaction(uint64_t hex_addr, bool is_write);
};
 */

typedef struct MemSys MemSys;

typedef struct MSHR_Entry MSHR_Entry;

struct MSHR_Entry
{
  uns coreid;
  uns robid;
  uns64 inst_num;
};


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

struct MemSys
{
  dramsim3::MemorySystem          *mainmem;
  uns64                 lines_in_mainmem_rbuf;
  std::unordered_map<uns64, MSHR_Entry> mshr;
};

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

MemSys *memsys_new(void);
Flag    memsys_access(MemSys *m, Addr lineaddr, uns coreid, uns robid, uns64 inst_num, Addr wb_lineaddr);
void    memsys_cycle(MemSys *m);
void    memsys_print_state(MemSys *m);
void    memsys_print_stats(MemSys *m);
void    memsys_mshr_insert(MemSys *m, Addr lineaddr, uns coreid, uns robid, uns64 inst_num);
void    memsys_callback(MemSys *m, Addr lineaddr);
void    memsys_callback_write(MemSys *m, Addr lineaddr);
Flag    memsys_isindrfm(MemSys *m, Addr lineaddr);
Flag    memsys_isinref(MemSys *m, Addr lineaddr);

///////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////