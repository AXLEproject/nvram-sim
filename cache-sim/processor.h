
#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

#include "cache.h"
#include <map>

#include <iostream>
#include <iomanip>

/** 
 * Basic emulation of a processor that reads and writes
 * Used only for simulating some line traveling between processor cores
 * @author Sasa Tomic
 * */

struct Processor : Cache
{
  Processor (
      std::string name,
      GenericMemoryPtr parent_memory
    ) : Cache(name, parent_memory, 64, 2, 8, 1, true)
    {
      // allocate all direct entries
      _entries.resize(_num_direct_entries);
      for (size_t i=0; i<_num_direct_entries; i++) {
        _entries[i].set_capacity(_associativity);
      }
      // connect to a parent memory
      _parent->add_child(this);
    }
  /// gets line for writing
  void write(Addr addr, int val, size_t &cost_ticks) {
    uint8_t *data;
    this->line_get(addr, LINE_MOD, cost_ticks, data);
//for (int i=0; i<get_line_size()-1; i++) //NVLOG("%x%s ", line->pdata[i], (i==(addr-line->addr)?"*":"")); //NVLOG("%x\n", line->pdata[get_line_size()-1]);
    *(int*)&data[mask_bits(addr, get_line_size())] = val;
    //NVLOG("%s write[%lx]=%d\n", _name.c_str(), addr, val);
//for (int i=0; i<get_line_size()-1; i++) //NVLOG("%x%s ", line->pdata[i], (i==(addr-line->addr)?"*":"")); //NVLOG("%x\n", line->pdata[get_line_size()-1]);
  }
  /// gets line for reading
  void read(Addr addr, int &val, size_t &cost_ticks) {
    uint8_t *data;
    this->line_get(addr, LINE_SHR, cost_ticks, data);
    val = *(int*)&data[mask_bits(addr, get_line_size())];
    //NVLOG("%s read[%lx]=%d\n", _name.c_str(), addr, val);
//if (line->pdata) {
//for (int i=0; i<get_line_size()-1; i++) //NVLOG("%x%s ", line->pdata[i], (i==(addr-line->addr)?"*":"")); //NVLOG("%x\n", line->pdata[get_line_size()-1]);
//}
  }
  virtual void add_child(GenericMemoryPtr child_memory) {assert(false);}
};

#endif //__PROCESSOR_H__
