#include <string.h>
#include "cache.h"
#include "quicktest.h"

#define globalmem_size 4*1024*1024
extern int *globalmem;

QT_TEST(lowest_bit_set)
{
  QT_CHECK_EQUAL(lowestBitSet32(0UL), -1);
  for (int i=0; i<32; i++) {
    //QT_CHECK_EQUAL(1UL<<i, i);
    QT_CHECK_EQUAL(lowestBitSet32(1UL<<i), i);
  }
  QT_CHECK_EQUAL(lowestBitSet64(0ULL), -1);
  for (int i=0; i<63; i++) {
    //QT_CHECK_EQUAL(1ULL<<i, i);
    QT_CHECK_EQUAL(lowestBitSet64(1ULL<<i), i);
  }
}
uint8_t *data;

QT_TEST(cache_line_add_remove)
{
#define CREATE_CHILD_MEM_VECT NULL
  MainMemory main_mem;
  size_t direct_entries = 1;
  size_t associativity = 4;
  size_t line_size_bytes = 64;
  Cache cache("L1", &main_mem, direct_entries, associativity, line_size_bytes);
  const Addr addr = (Addr)&globalmem[0];
  size_t num_ticks = 0;
	QT_CHECK_EQUAL(false, cache.is_line_present(addr));
  cache.line_get(addr, LINE_SHR, num_ticks, data);
	QT_CHECK_EQUAL(true, cache.is_line_present(addr));
  cache.line_evict(addr);
	QT_CHECK_EQUAL(false, cache.is_line_present(addr));
}

QT_TEST(cache_line_max_entries)
{
  MainMemory main_mem;
  size_t direct_entries = 1;
  size_t associativity = 4;
  size_t line_size_bytes = 64;
  Cache cache("L1", &main_mem, direct_entries, associativity, line_size_bytes);
  Addr tmp_addr = (Addr)&globalmem[0];
  size_t cnt=0;
  size_t num_ticks = 0;
  // fill the cache up to its capacity
  for (cnt=0; cnt<associativity; cnt++) {
    QT_CHECK_EQUAL(cache.is_line_present(tmp_addr), false);
    cache.line_get(tmp_addr, LINE_SHR, num_ticks, data);
    QT_CHECK_EQUAL(cache.is_line_present(tmp_addr), true);
    tmp_addr += line_size_bytes;
  }
  QT_CHECK_EQUAL(cache.get_num_valid_entries(), associativity);
  // go one over the capacity...
  QT_CHECK_EQUAL(cache.is_line_present(tmp_addr), false);
  cache.line_get(tmp_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(cache.is_line_present(tmp_addr), true);
  QT_CHECK_EQUAL(cache.get_num_valid_entries(), associativity);
}

QT_TEST(set_assoc_addr2set)
{
  MainMemory main_mem;
  size_t direct_entries = 512;
  size_t associativity = 4;
  size_t line_size_bytes = 64;
  Cache cache("L1", &main_mem, direct_entries, associativity, line_size_bytes);
  size_t entry_cycle = line_size_bytes * direct_entries;
  QT_CHECK_EQUAL(cache.addr2directentry(0),                   0);
  QT_CHECK_EQUAL(cache.addr2directentry(line_size_bytes-1),     0);
  QT_CHECK_EQUAL(cache.addr2directentry(line_size_bytes),       1);
  QT_CHECK_EQUAL(cache.addr2directentry(2*line_size_bytes-1),   1);
  QT_CHECK_EQUAL(cache.addr2directentry(2*line_size_bytes),     2);
  QT_CHECK_EQUAL(cache.addr2directentry(entry_cycle-1),     511);
  QT_CHECK_EQUAL(cache.addr2directentry(entry_cycle),         0);
  QT_CHECK_EQUAL(cache.addr2directentry(2*entry_cycle-1),   511);
  QT_CHECK_EQUAL(cache.addr2directentry(2*entry_cycle),       0);
  QT_CHECK_EQUAL(cache.addr2directentry(4*GB-entry_cycle),    0);
  QT_CHECK_EQUAL(cache.addr2directentry(4*GB-1),            511);
}

QT_TEST(set_assoc_max_entries)
{
  MainMemory main_mem;
  size_t direct_entries = 512;
  size_t associativity = 4;
  size_t line_size_bytes = 64;
  size_t entry_cycle = line_size_bytes/sizeof(int) * direct_entries;
  size_t num_ticks = 0;
  Cache cache("L1", &main_mem, direct_entries, associativity, line_size_bytes);
  QT_CHECK_EQUAL(cache.get_num_valid_entries(), 0);
  for(size_t cnt1=0; cnt1<associativity+4; cnt1++) {
    for(size_t cnt2=0; cnt2<direct_entries; cnt2++) {
      Addr offset = cnt1*entry_cycle + cnt2*line_size_bytes/sizeof(int);
      assert(offset<globalmem_size);
      cache.line_get((Addr)&globalmem[offset], LINE_SHR, num_ticks, data);
    }
  }
  QT_CHECK_EQUAL(cache.get_num_valid_entries(), direct_entries*associativity);
}
QT_TEST(set_assoc_add_remove)
{
  MainMemory main_mem;
  size_t direct_entries = 512;
  size_t line_size_bytes = 64;
  size_t associativity = 4;
  size_t num_ticks = 0;
  Cache cache("L1", &main_mem, direct_entries, associativity, line_size_bytes);
  const Addr addr = (Addr)&globalmem[0];
	QT_CHECK_EQUAL(false, cache.is_line_present(addr));
  cache.line_get(addr, LINE_SHR, num_ticks, data);
	QT_CHECK_EQUAL(true, cache.is_line_present(addr));
  cache.line_evict(addr);
	QT_CHECK_EQUAL(false, cache.is_line_present(addr));
}

QT_TEST(set_assoc_dump)
{
  MainMemory main_mem;
  size_t direct_entries = 4;
  size_t line_size_bytes = 64;
  size_t associativity = 2;
  size_t entry_cycle = line_size_bytes/sizeof(int) * direct_entries;
  size_t num_ticks = 0;
  Cache cache("L1", &main_mem, direct_entries, associativity, line_size_bytes);
  QT_CHECK_EQUAL(0, cache.get_num_valid_entries());
  for(size_t cnt1=0; cnt1<associativity; cnt1++) {
    for(size_t cnt2=0; cnt2<direct_entries; cnt2++) {
      Addr offset = cnt1*entry_cycle + cnt2*line_size_bytes/sizeof(int);
      assert(offset<globalmem_size);
      cache.line_get((Addr)&globalmem[offset], LINE_SHR, num_ticks, data);
    }
  }
  QT_CHECK_EQUAL(direct_entries*associativity, cache.get_num_valid_entries());
  std::cout << "Test printout of cache contents:" << std::endl;
  std::cout << cache << std::endl;
}

QT_TEST(cache_hierarchy_simplest)
{
  //std::cout << "running test: " << mTestName << std::endl;
  // Processor
  // L1 cache
  // L2 cache
  // Main Memory

  Addr addr_space = 8*GB;
  size_t mem_access_cost = 500;
  size_t L1_direct_entries = 4;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  size_t L1_entry_cycle = L1_line_size_bytes * L1_direct_entries;
  size_t L2_direct_entries = 128;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1("L1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);
  const Addr A_addr = (Addr)&globalmem[0];
  size_t num_ticks = 0;
  // get address into cache, check the cost
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  QT_CHECK_EQUAL(L1.is_line_present(A_addr), true);
  QT_CHECK_EQUAL(L2.is_line_present(A_addr), true);
  num_ticks = 0;
  // check the cost of re-accessing the value
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks);
  num_ticks = 0;
  // get new address into L1 cache, it should go to the same set as the first one
  L1.line_get(A_addr+L1_entry_cycle, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  // get new address into L1 cache, it should go to the same set as the first one
  // this will effectively *evict* the line from the L1 cache
  // notice, however, that the line should still be in L2 cache, due to its higher associativity
  L1.line_get(A_addr+2*L1_entry_cycle, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  // let's check if this is OK:
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);

  num_ticks = 0;
  L1.line_get(A_addr, LINE_MOD, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);
  QT_CHECK_EQUAL(L1.is_line_present(A_addr), true);
  QT_CHECK_EQUAL(L2.is_line_present(A_addr), true);
  main_mem.reset();
  QT_CHECK_EQUAL(L1.is_line_present(A_addr), false);
  QT_CHECK_EQUAL(L2.is_line_present(A_addr), false);
  num_ticks = 0;
  L1.line_get(A_addr, LINE_MOD, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  QT_CHECK_EQUAL(L1.is_line_present(A_addr), true);
  QT_CHECK_EQUAL(L2.is_line_present(A_addr), true);
  QT_CHECK_EQUAL(L1.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1.is_writer(A_addr), true);
  QT_CHECK_EQUAL(L2.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L2.is_writer(A_addr), true);
}

QT_TEST(cache_hierarchy_big_L1)
{
  //std::cout << "running test: " << mTestName << std::endl;
  // Processor
  // L1 cache
  // L2 cache
  // Main Memory

  ///////////////////////////////////////////////
  // test scenario: victim cache (L2) purpose?
  ///////////////////////////////////////////////
  Addr addr_space = 8*GB;
  size_t mem_access_cost = 500;
  size_t L1_direct_entries = 128;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  size_t L1_entry_cycle = L1_line_size_bytes * L1_direct_entries;
  size_t L2_direct_entries = 64;
  size_t L2_line_size_bytes = 64;
  size_t L2_associativity = 4;
  size_t L2_hit_cost_ticks = 50;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1("L1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);
  const Addr A_addr = (Addr)&globalmem[0];
  size_t num_ticks = 0;
  // get address into cache, check the cost
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  // check the cost of re-accessing the value
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks);
  num_ticks = 0;
  // get new address into L1 cache, it should go to the same set as the first one
  L1.line_get(A_addr+L1_entry_cycle, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  // get new address into L1 cache, it should go to the same set as the first one
  // this will effectively *evict* the line from the L1 cache
  // notice, however, that the line should still be in L2 cache, due to its higher associativity
  L1.line_get(A_addr+2*L1_entry_cycle, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  // let's check if this is OK:
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);
}

QT_TEST(cache_hierarchy_larger_line_size)
{
  //std::cout << "running test: " << mTestName << std::endl;
  // Processor
  // L1 cache
  // L2 cache
  // Main Memory

  ///////////////////////////////////////////////
  // test scenario: L2 has larger line size (than L1)
  ///////////////////////////////////////////////
  const Addr addr_space = 8*GB;
  const size_t mem_access_cost = 500;
  const size_t L2_direct_entries = 1;
  const size_t L2_line_size_bytes = 128;
  const size_t L2_associativity = 1;
  const size_t L2_hit_cost_ticks = 50;
  const size_t L1_direct_entries = 1;
  const size_t L1_line_size_bytes = 64;
  const size_t L1_associativity = 1;
  const size_t L1_hit_cost_ticks = 3;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1("L1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);

  const Addr A_addr = (Addr)&globalmem[0];
  size_t num_ticks = 0;
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  L1.line_get(A_addr+L1_line_size_bytes, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  L1.line_get(A_addr+L2_line_size_bytes, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  // A_addr was evicted from L2 cache and should also be evicted from L1 cache
  // as well as A_addr+L1_line_size_bytes, which is just a part of the line
  // evicted from the L2 cache
  num_ticks = 0;
  L1.line_get(A_addr+L1_line_size_bytes, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
}

QT_TEST(cache_hierarchy_dir_state_changes)
{
  // Main Memory
  // L2 cache
  // L1 cache
  // Processor

  ///////////////////////////////////////////////
  // test scenario: changing DIR state from LINE_SHR to LINE_EXC to LINE_MOD and back
  ///////////////////////////////////////////////
  Addr addr_space = 8*GB;
  size_t mem_access_cost = 500;
  size_t L1_direct_entries = 4;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  //size_t L1_entry_cycle = L1_line_size_bytes * L1_direct_entries;
  size_t L2_direct_entries = 128;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  size_t num_ticks = 0;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1("L1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);
  const Addr A_addr = (Addr)&globalmem[0];
  // get address into cache, check the cost
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  // more rights!
  L1.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  // more rights!
  L1.line_get(A_addr, LINE_MOD, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks);
  num_ticks = 0;
  // less rights...
  L1.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks);
  num_ticks = 0;
  // less rights...
  L1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks);
  num_ticks = 0;
}


QT_TEST(is_cache_private)
{
  // Main Memory
  //     L2
  // L1      L1
  // P0      P1

  Addr addr_space = 8*GB;
  size_t mem_access_cost = 500;
  size_t L1_direct_entries = 4;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  //size_t L1_entry_cycle = L1_line_size_bytes * L1_direct_entries;
  size_t L2_direct_entries = 128;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  //size_t num_ticks = 0;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  QT_CHECK_EQUAL(L2._is_private_cache, true);
  Cache L1P0("L1P0", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  QT_CHECK_EQUAL(L2._is_private_cache, true);
  QT_CHECK_EQUAL(L1P0._is_private_cache, true);
  Cache L1P1("L1P1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  QT_CHECK_EQUAL(L2._is_private_cache, false);
  QT_CHECK_EQUAL(L1P0._is_private_cache, true);
  QT_CHECK_EQUAL(L1P1._is_private_cache, true);
}

QT_TEST(cache_hierarchy_2proc_shared_L2)
{
  // Main Memory
  //     L2
  // L1      L1
  // P0      P1

  Addr addr_space = 8*GB;
  size_t mem_access_cost = 500;
  size_t L1_direct_entries = 4;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  //size_t L1_entry_cycle = L1_line_size_bytes * L1_direct_entries;
  size_t L2_direct_entries = 128;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  size_t num_ticks = 0;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1P0("L1P0", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P1("L1P1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  const Addr A_addr = (Addr)&globalmem[0];
  ///////////////////////////////////////////////
  // test scenario: line shared use
  ///////////////////////////////////////////////
  // get address into cache, check the cost
  L1P0.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(mem_access_cost+L2_hit_cost_ticks+L1_hit_cost_ticks, num_ticks);
  num_ticks = 0;
  L1P1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(L2_hit_cost_ticks+L1_hit_cost_ticks, num_ticks);
  num_ticks = 0;
  ///////////////////////////////////////////////
  // test scenario: line ping-ponging
  ///////////////////////////////////////////////
  L1P0.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  L1P1.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  L1P0.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
  L1P1.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks+L1_hit_cost_ticks);
  num_ticks = 0;
}

QT_TEST(cache_hierarchy_32proc_L2_L3)
{
  // L3 shared by all L2; L2 shared by 4xL1 caches
  // ************************
  //          Main Memory
  //               L3
  //       L2           ...
  // L1  L1  L1  L1     ...
  // P0  P1  P2  P3     ...

  Addr addr_space = 8*GB;
  size_t mem_access_cost = 500;
  size_t L1_direct_entries = 256;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  //size_t L1_entry_cycle = L1_line_size_bytes * L1_direct_entries;

  size_t L2_direct_entries = 1024;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 12;
  
  size_t L3_direct_entries = 16*1024;
  size_t L3_line_size_bytes = 128;
  size_t L3_associativity = 8;
  size_t L3_hit_cost_ticks = 32;

  size_t num_ticks = 0;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L3("L3", &main_mem, L3_direct_entries, L3_associativity, L3_line_size_bytes, L3_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L2c0("L2c0", &L3, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L2c1("L2c1", &L3, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L2c2("L2c2", &L3, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L2c3("L2c3", &L3, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L2c4("L2c4", &L3, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L2c5("L2c5", &L3, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L2c6("L2c6", &L3, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L2c7("L2c7", &L3, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P0 ("L1P0",  &L2c0, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P1 ("L1P1",  &L2c0, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P2 ("L1P2",  &L2c0, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P3 ("L1P3",  &L2c0, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P4 ("L1P4",  &L2c1, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P5 ("L1P5",  &L2c1, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P6 ("L1P6",  &L2c1, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P7 ("L1P7",  &L2c1, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P8 ("L1P8",  &L2c2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P9 ("L1P9",  &L2c2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P10("L1P10", &L2c2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P11("L1P11", &L2c2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P12("L1P12", &L2c3, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P13("L1P13", &L2c3, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P14("L1P14", &L2c3, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P15("L1P15", &L2c3, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P16("L1P16", &L2c4, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P17("L1P17", &L2c4, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P18("L1P18", &L2c4, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P19("L1P19", &L2c4, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P20("L1P20", &L2c5, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P21("L1P21", &L2c5, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P22("L1P22", &L2c5, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P23("L1P23", &L2c5, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P24("L1P24", &L2c6, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P25("L1P25", &L2c6, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P26("L1P26", &L2c6, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P27("L1P27", &L2c6, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P28("L1P28", &L2c7, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P29("L1P29", &L2c7, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P30("L1P30", &L2c7, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P31("L1P31", &L2c7, L1_direct_entries, L1_associativity, L2_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  const Addr A_addr = (Addr)&globalmem[0];
  //const Addr B_addr = 200000000;
  ///////////////////////////////////////////////
  // test scenario: line shared use
  ///////////////////////////////////////////////
  // get address into cache, check the cost
  L1P0.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(mem_access_cost + L3_hit_cost_ticks + L2_hit_cost_ticks + L1_hit_cost_ticks, num_ticks);
  num_ticks = 0;
  L1P10.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(L3_hit_cost_ticks + L2_hit_cost_ticks + L1_hit_cost_ticks, num_ticks);
  num_ticks = 0;
  ///////////////////////////////////////////////
  // test scenario: line ping-ponging
  ///////////////////////////////////////////////
  L1P0.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L3_hit_cost_ticks + L2_hit_cost_ticks + L1_hit_cost_ticks);
  num_ticks = 0;
  L1P17.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L3_hit_cost_ticks + L2_hit_cost_ticks + L1_hit_cost_ticks);
  num_ticks = 0;
  L1P0.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L3_hit_cost_ticks + L2_hit_cost_ticks + L1_hit_cost_ticks);
  num_ticks = 0;
  L1P17.line_get(A_addr, LINE_EXC, num_ticks, data);
  QT_CHECK_EQUAL(num_ticks, L3_hit_cost_ticks + L2_hit_cost_ticks + L1_hit_cost_ticks);
  num_ticks = 0;
}


#include "processor.h"

QT_TEST(processor_invalidation)
{
  // Main Memory
  //     L2
  // L1      L1
  // P0      P1

  Addr addr_space = 4*GB;
  size_t mem_access_cost = 500;
  size_t L2_direct_entries = 512;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  size_t L1_direct_entries = 128;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1P0("L1P0", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Cache L1P1("L1P1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  Processor P0("P0", &L1P0);
  Processor P1("P1", &L1P1);

  memset(globalmem, 0, globalmem_size*sizeof(int));
  for(size_t i=0; i<globalmem_size; i++) {
    if (globalmem[i]!=0) NVLOG("mem[%lx]=%d\n", (Addr)&globalmem[i], globalmem[i]);
  }
  
  size_t ticksP0 = 0, ticksP1 = 0;
  int vals[4] = {0, 0, 0, 0};
  {
    Addr addr1P0 = (Addr)&globalmem[64];
    Addr addr1P1 = (Addr)&globalmem[11];
    Addr addr2P0 = (Addr)&globalmem[175];
    Addr addr2P1 = (Addr)&globalmem[64];

    P0.read(addr1P0, vals[0], ticksP0);
    P0.write(addr1P0, ++vals[0], ticksP0);
    P1.read(addr1P1, vals[1], ticksP1);
    P1.write(addr1P1, ++vals[1], ticksP1);

    P0.read(addr2P0, vals[2], ticksP0);
    P0.write(addr2P0, --vals[2], ticksP0);
    P1.read(addr2P1, vals[3], ticksP1);
    P1.write(addr2P1, --vals[3], ticksP1);

    main_mem.reset();
    int control_sum=0;
    for(size_t i=0; i<globalmem_size; i++) {
      control_sum += globalmem[i];
    }
    if (control_sum!=0) {
      P0.dump(std::cout);
      P1.dump(std::cout);
      for(size_t i=0; i<globalmem_size; i++) {
        if (globalmem[i]!=0) NVLOG("mem[%lx]=%d\n", (Addr)&globalmem[i], globalmem[i]);
      }
    }
    QT_CHECK_EQUAL(control_sum, 0);
  }

  {
    memset(globalmem, 0, globalmem_size*sizeof(int));

    Addr addr1P0 = (Addr)&globalmem[5];
    Addr addr1P1 = (Addr)&globalmem[10];
    Addr addr2P0 = (Addr)&globalmem[88];
    Addr addr2P1 = (Addr)&globalmem[88];

    P0.read(addr1P0, vals[0], ticksP0);
    P0.write(addr1P0, ++vals[0], ticksP0);
    P1.read(addr1P1, vals[1], ticksP1);
    P1.write(addr1P1, ++vals[1], ticksP1);

    P0.read(addr2P0, vals[2], ticksP0);
    P0.write(addr2P0, --vals[2], ticksP0);
    P1.read(addr2P1, vals[3], ticksP1);
    P1.write(addr2P1, --vals[3], ticksP1);

    main_mem.reset();
    int control_sum=0;
    for(size_t i=0; i<globalmem_size; i++) {
      control_sum += globalmem[i];
    }
    if (control_sum!=0) {
      //P0.dump(std::cout);
      //P1.dump(std::cout);
      for(size_t i=0; i<globalmem_size; i++) {
        if (globalmem[i]!=0) NVLOG("mem[%lx]=%d\n", (Addr)&globalmem[i], globalmem[i]);
      }
    }
    QT_CHECK_EQUAL(control_sum, 0);
  }

  {
    memset(globalmem, 0, globalmem_size*sizeof(int));

    Addr addr1P0 = (Addr)&globalmem[131];
    Addr addr1P1 = (Addr)&globalmem[71];
    Addr addr2P0 = (Addr)&globalmem[71];
    Addr addr2P1 = (Addr)&globalmem[11];

    P0.read(addr1P0, vals[0], ticksP0);
    P0.write(addr1P0, ++vals[0], ticksP0);
    P1.read(addr1P1, vals[1], ticksP1);
    P1.write(addr1P1, ++vals[1], ticksP1);

    P0.read(addr2P0, vals[2], ticksP0);
    P0.write(addr2P0, --vals[2], ticksP0);
    P1.read(addr2P1, vals[3], ticksP1);
    P1.write(addr2P1, --vals[3], ticksP1);

    main_mem.reset();
    int control_sum=0;
    for(size_t i=0; i<globalmem_size; i++) {
      control_sum += globalmem[i];
    }
    if (control_sum!=0) {
      //P0.dump(std::cout);
      //P1.dump(std::cout);
      for(size_t i=0; i<globalmem_size; i++) {
        if (globalmem[i]!=0) NVLOG("mem[%lx]=%d\n", (Addr)&globalmem[i], globalmem[i]);
      }
    }
    QT_CHECK_EQUAL(control_sum, 0);
  }
}

QT_TEST(writeback_writethrough)
{
  // Main Memory
  // L2 cache
  // L1 cache
  // Processor

  ///////////////////////////////////////////////
  //
  // test writeback and writethrough implementations
  //
  ///////////////////////////////////////////////
  Addr addr_space = 8*GB;
  size_t mem_access_cost = 500;
  size_t L2_direct_entries = 128;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  size_t L1_direct_entries = 32;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  size_t num_ticks = 0;
  {
    // test writeBACK
    MainMemory main_mem(addr_space, mem_access_cost);
    Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
    Cache L1("L1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);
    
    // NOT THE BEST WAY TO TEST, BUT WILL WORK FOR NOW...
    const Addr A_addr = (Addr)&globalmem[0];
    num_ticks = 0;
    Addr L1_line_addr = floor(A_addr, L1._line_size_bytes);
    Addr L2_line_addr = floor(A_addr, L2._line_size_bytes);
    size_t L1_direct_entry = L1.addr2directentry(L1_line_addr);
    size_t L2_direct_entry = L2.addr2directentry(L2_line_addr);
    // get address into L1 and check internal cache states
    L1.line_get(L1_line_addr, LINE_MOD, num_ticks, data);
    Line *overflow_line; bool set_overflow = false;
    QT_CHECK_EQUAL(L1._entries[L1_direct_entry].get(L1_line_addr, set_overflow, overflow_line)->state, LINE_MOD);
    QT_CHECK_EQUAL(L2._entries[L2_direct_entry].get(L2_line_addr, set_overflow, overflow_line)->state, LINE_EXC);
  }
  {
    // test writeTHROUGH
    MainMemory main_mem(addr_space, mem_access_cost);
    Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, !IS_WRITEBACK_CACHE);
    Cache L1("L1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
    
    const Addr A_addr = (Addr)&globalmem[0];
    num_ticks = 0;
    Addr L1_line_addr = floor(A_addr, L1._line_size_bytes);
    Addr L2_line_addr = floor(A_addr, L2._line_size_bytes);
    size_t L1_direct_entry = L1.addr2directentry(L1_line_addr);
    size_t L2_direct_entry = L2.addr2directentry(L2_line_addr);
    // get address into L1 and check internal cache states
    L1.line_get(L1_line_addr, LINE_MOD, num_ticks, data);
    Line *overflow_line; bool set_overflow = false;
    QT_CHECK_EQUAL(L1._entries[L1_direct_entry].get(L1_line_addr, set_overflow, overflow_line)->state, LINE_MOD);
    QT_CHECK_EQUAL(L2._entries[L2_direct_entry].get(L2_line_addr, set_overflow, overflow_line)->state, LINE_MOD);
  }
}

QT_TEST(is_set_unset_rw_basic)
{
  Addr addr_space = 4*GB;
  size_t mem_access_cost = 500;
  size_t L2_direct_entries = 512;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  size_t L1_direct_entries = 128;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1P0("L1P0", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1P1("L1P1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);

  const Addr A_addr = (Addr)&globalmem[0];
  size_t latency = 0;
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);

  L1P0.line_get(A_addr, LINE_SHR, latency, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);

  L1P0.line_evict(A_addr);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);

  main_mem.reset();
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);

  L1P1.line_get(A_addr, LINE_SHR, latency, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);

  L1P1.line_evict(A_addr);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);

  L1P1.line_get(A_addr, LINE_MOD, latency, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), true); // every writer is reader at the same time
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), true);

  L1P1.line_evict(A_addr);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);
}

QT_TEST(line_writer_to_sharer)
{
  Addr addr_space = 4*GB;
  size_t mem_access_cost = 500;
  size_t L2_direct_entries = 128;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  size_t L1_direct_entries = 4;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1P0("L1P0", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1P1("L1P1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, IS_WRITEBACK_CACHE);
  const Addr A_addr = (Addr)&globalmem[0];
  size_t num_ticks = 0;
  // get address into cache, check the cost
  L1P0.line_get(A_addr, LINE_MOD, num_ticks, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), true);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);
  num_ticks = 0;
  L1P1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);
  // this is the latency:
  // Go from L1P1 to L2 (L2 hit latency)
  // Go from L2 to L1P0 (L2 hit latency+L1 hit latency)
  // repeat for the second part of the line, as L2 holds bigger lines (L2 hit latency)
  // and finally L1P1 hit latency
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks + L2_hit_cost_ticks + (L2_line_size_bytes/L1_line_size_bytes)*(L2_hit_cost_ticks)+L1_hit_cost_ticks); // access + writeback
  num_ticks = 0;
  L1P1.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks);
  num_ticks = 0;
  L1P1.line_get(A_addr, LINE_MOD, num_ticks, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), false);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), true);
  QT_CHECK_EQUAL(num_ticks, L2_hit_cost_ticks + L1_hit_cost_ticks);
  num_ticks = 0;
  L1P0.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks + L2_hit_cost_ticks + (L2_line_size_bytes/L1_line_size_bytes)*(L2_hit_cost_ticks)+L1_hit_cost_ticks); // access + writeback
  num_ticks = 0;
  L1P0.line_get(A_addr, LINE_SHR, num_ticks, data);
  QT_CHECK_EQUAL(L1P0.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P0.is_writer(A_addr), false);
  QT_CHECK_EQUAL(L1P1.is_reader(A_addr), true);
  QT_CHECK_EQUAL(L1P1.is_writer(A_addr), false);
  QT_CHECK_EQUAL(num_ticks, L1_hit_cost_ticks);
  num_ticks = 0;
}

void cache_tests_runall()
{
	QT_RUN_TESTS;
}
