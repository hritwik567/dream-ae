#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <functional>
#include "externs.h"
#include "memsys_dramsim3.h"
#include "mcore.h"

extern MCore *mcore[MAX_THREADS];

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

MemSys *memsys_new(void)
{
  MemSys *m = new MemSys;
  std::cout << "Config: " << DRAMSIM3CFG << std::endl;
  m->mainmem = dramsim3::GetMemorySystem(DRAMSIM3CFG, ".", std::bind(&memsys_callback, m, std::placeholders::_1), std::bind(&memsys_callback_write, m, std::placeholders::_1));
  m->lines_in_mainmem_rbuf = MEM_PAGESIZE/LINESIZE; // static
   
  return m;
}

Flag memsys_isindrfm(MemSys *m, Addr lineaddr)
{
  Flag retval = FALSE;
  if (m->mainmem->IsInDRFM(lineaddr * LINESIZE))
  {
    retval = TRUE;
  }

  return retval;
}

Flag memsys_isinref(MemSys *m, Addr lineaddr)
{
  Flag retval = FALSE;
  if (m->mainmem->IsInREF(lineaddr * LINESIZE))
  {
    retval = TRUE;
  }

  return retval;
}


//////////////////////////////////////////////////////////////////////////
// NOTE: ACCESSES TO THE MEMORY USE LINEADDR OF THE CACHELINE ACCESSED
//////////////////////////////////////////////////////////////////////////

Flag memsys_access(MemSys *m, Addr lineaddr,  uns coreid, uns robid, uns64 inst_num, Addr wb_lineaddr)
{
  Flag retval = FALSE;

  Addr byteaddress = lineaddr * LINESIZE;
  if(m->mainmem->WillAcceptTransaction(byteaddress, FALSE))
  {
    m->mainmem->AddTransaction(byteaddress, FALSE);
    memsys_mshr_insert(m, lineaddr, coreid, robid, inst_num);
    retval = TRUE;
  }
  
  if (wb_lineaddr)
  {
    Addr wb_byteaddress = wb_lineaddr * LINESIZE;
    if(m->mainmem->WillAcceptTransaction(wb_byteaddress, TRUE))
    {
      m->mainmem->AddTransaction(wb_byteaddress, TRUE);
    }
  }
  return retval;
}


//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void  memsys_cycle(MemSys *m)
{
  m->mainmem->ClockTick(); 
}

//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void memsys_print_state(MemSys *m)
{
  // m->mainmem->PrintDeadlock();
}

//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void memsys_print_stats(MemSys *m)
{
  m->mainmem->PrintStats(true);
  m->mainmem->ResetStats();
}

//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void memsys_mshr_insert(MemSys *m, Addr lineaddr, uns coreid, uns robid, uns64 inst_num)
{
  MSHR_Entry entry;
  entry.coreid = coreid;
  entry.robid = robid;
  entry.inst_num = inst_num;
  assert(m->mshr.count(lineaddr)==0); // the line should not be present already
  m->mshr[lineaddr] = entry;
}

//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void memsys_callback(MemSys *m, Addr byteaddress)
{
  Addr lineaddr = byteaddress / LINESIZE;
  assert(m->mshr.count(lineaddr)>0);
  MSHR_Entry entry = m->mshr[lineaddr];
  mcore_rob_wakeup(mcore[entry.coreid], entry.robid, entry.inst_num);
  m->mshr.erase(lineaddr);
}

void memsys_callback_write(MemSys *m, Addr byteaddress)
{
}

//////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
