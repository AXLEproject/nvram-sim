
#include "cache_tests.h"
#include "cache.h"
#include "processor.h"
#include <ctime>
#include <cstdlib>

#include <iostream>

#include "logger.h"
static Logger nvlogger("trace.txt");

void nvlog_flush() {
  nvlogger.Flush();
}

void nvlog(char *data, int size) {
  cache_nvlogger.Log((unsigned char *)data, size);
  cache_nvlogger.Flush();
}

Fault
rw_array_silent(Addr addr, uint32_t len, uint8_t *data, const bool is_write)
{
    if (is_write) {
        //NVLOG("P%d WRITE %d bytes\t0x%lx -> 0x%lx\n", cpuid, len, (Addr)data, addr);
        memcpy((uint8_t*)addr, (uint8_t*)data, len);
    } else {
        //NVLOG("P%d READ %d bytes\t0x%lx -> 0x%lx\n", cpuid, len, addr, (Addr)data);
        memcpy((uint8_t*)data, (uint8_t*)addr, len);
    }
    return NoFault;
}

char some_temp_string[1024];
char *strbuf=some_temp_string;
uint8_t linestates[3] = {LINE_SHR, LINE_EXC, LINE_MOD};

void run_stresstest_simple()
{
  std::cout << "Running simple random address test..." << std::endl;
  Addr addr_space = 4*GB;
  size_t mem_access_cost = 500;
  size_t L2_direct_entries = 64;
  size_t L2_line_size_bytes = 128;
  size_t L2_associativity = 8;
  size_t L2_hit_cost_ticks = 50;
  size_t L1_direct_entries = 128;
  size_t L1_line_size_bytes = 64;
  size_t L1_associativity = 2;
  size_t L1_hit_cost_ticks = 3;
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1("L1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);
  srand((unsigned)time(0));
  size_t ticks = 0;

#ifdef WINTIME
  long int before = GetTickCount();
#else
  clock_t start, finish;
  start = clock();
#endif

  uint8_t *data;
  size_t iterations = 100*K;
  for (size_t i=0; i<iterations; i++)
  {
    Addr addr = (Addr)&globalmem[rand()%globalmem_size];
    uint8_t line_state_req = linestates[rand()%3];
    L1.line_get(addr, line_state_req, ticks, data);
    assert(ticks>0);
    ticks = 0;
  }

#ifdef WINTIME
  long int after = GetTickCount();
  std::cout << "Execution Time: " << (after-before) << " ms." << std::endl;
#else
  finish = clock();
  double exec_time_ms = ((double)(finish - start))*1000/CLOCKS_PER_SEC;
  std::cout << "Execution Time: " << exec_time_ms << " ms." << std::endl;
  std::cout << "that makes: " << ((double)(iterations/exec_time_ms)) << " cache queries/ms." << std::endl;
#endif
}

void run_stresstest_2proc()
{
  std::cout << "Running cache hierarchy correctness test..." << std::endl;

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

  for(size_t i=0; i<globalmem_size; i++) {
    globalmem[i]=0;
  }

#ifdef WINTIME
  long int before = GetTickCount();
#else
  clock_t start, finish;
  start = clock();
#endif
  
  size_t iterations = 100*K;
  size_t ticksP0 = 0, ticksP1 = 0;
  for (size_t i=0; i<iterations; i++)
  {
    //int control_sum=0;
    //for(size_t i=0; i<globalmem_size; i++) {
    //  control_sum += globalmem[i];
    //}
    //assert(control_sum==0);
    Addr addr1P0 = (Addr)&globalmem[rand()%globalmem_size];
    Addr addr2P0 = (Addr)&globalmem[rand()%globalmem_size];
    Addr addr1P1 = (Addr)&globalmem[rand()%globalmem_size];
    Addr addr2P1 = (Addr)&globalmem[rand()%globalmem_size];
    int vals[4] = {0, 0, 0, 0};
    P0.read(addr1P0, vals[0], ticksP0);
    P0.write(addr1P0, ++vals[0], ticksP0);
    P1.read(addr1P1, vals[1], ticksP1);
    P1.write(addr1P1, ++vals[1], ticksP1);

    P0.read(addr2P0, vals[2], ticksP0);
    P0.write(addr2P0, --vals[2], ticksP0);
    P1.read(addr2P1, vals[3], ticksP1);
    P1.write(addr2P1, --vals[3], ticksP1);

    //main_mem.reset();
    //for(size_t i=0; i<globalmem_size; i++) {
    //  control_sum += globalmem[i];
    //}
    //if (control_sum!=0) {
      //P0.dump(std::cout);
      //P1.dump(std::cout);
    //}
    //assert(control_sum==0);
    //std::cout << std::endl;
  };

#ifdef WINTIME
  long int after = GetTickCount();
  std::cout << "Execution Time: " << (after-before) << " ms." << std::endl;
#else
  finish = clock();
  double exec_time_ms = ((double)(finish - start))*1000/CLOCKS_PER_SEC;
  std::cout << "Execution Time: " << exec_time_ms << " ms." << std::endl;
  std::cout << "that makes: " << ((double)(iterations/exec_time_ms)) << " cache queries/ms." << std::endl;
#endif

  main_mem.reset();
  int control_sum=0;
  for(size_t i=0; i<globalmem_size; i++) {
    control_sum += globalmem[i];
  }
  std::cout << "Control sum: " << control_sum << " (should be 0)" << std::endl;
}

void run_timingtest_simple()
{
  const Addr addr_space = 4*GB;
  const size_t mem_access_cost = 10000;
  const size_t L2_direct_entries = 4;
  const size_t L2_line_size_bytes = 128;
  const size_t L2_associativity = 2;
  const size_t L2_hit_cost_ticks = 1000;
  const size_t L1_direct_entries = 2;
  const size_t L1_line_size_bytes = 64;
  const size_t L1_associativity = 1;
  const size_t L1_hit_cost_ticks = 10;
  //const size_t elem_per_L1_cl = L1_line_size_bytes / sizeof(int);
  MainMemory main_mem(addr_space, mem_access_cost);
  Cache L2("L2", &main_mem, L2_direct_entries, L2_associativity, L2_line_size_bytes, L2_hit_cost_ticks, IS_WRITEBACK_CACHE);
  Cache L1("L1", &L2, L1_direct_entries, L1_associativity, L1_line_size_bytes, L1_hit_cost_ticks, !IS_WRITEBACK_CACHE);

  size_t ticks = 0;
  uint8_t *data;

  for (size_t warmup=0; warmup<2; warmup++) {
    for (size_t i=1; i<globalmem_size; i*=2) {
      // WARMUP period START
      for (size_t warm=0; warm<warmup; warm++) {
        for (size_t j=0; j<i; j++) {
          L1.line_get((Addr)&globalmem[j], LINE_SHR, ticks, data);
        };
      }
      ticks = 0;
      // WARMUP period END
      for (size_t j=0; j<i; j++) {
        L1.line_get((Addr)&globalmem[j], LINE_SHR, ticks, data);
      }
      printf("warmup_iter: %2lu\tarray_size: %8lu\ttotal_ticks: %8lu\tticks/locations: %6.2f\n", warmup, i, ticks, (float)ticks/i);
      ticks=0;
      main_mem.dump_stats();
      main_mem.reset(); // reset all caches and main memory
    }
  }
}

void dump_stats_test()
{
  Addr addr_space = 4*GB;
  size_t mem_access_cost = 500;
  size_t L2_direct_entries = 512;
  size_t L2_line_size_bytes = 64;
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

  size_t iterations = 10*K;
  size_t ticks = 0;
  const size_t rnd_space = globalmem_size;
  uint8_t *data;
  for (size_t i=0; i<iterations; i++)
  {
    Addr addr1 = (Addr)&globalmem[rand()%rnd_space];
    Addr addr2 = (Addr)&globalmem[rand()%rnd_space];
    L1P0.line_get(addr1, linestates[rand()%3], ticks, data);
    L1P1.line_get(addr2, linestates[rand()%3], ticks, data);
  }
  // dump execution (hit/miss) statistics
  main_mem.dump_stats("A test of statistics printing");
}

#include "sigsegv.h"

int *globalmem;
bool shutdown_started=false;

/// Segmentation fault signal handler.
void
segfaultHandler(int sigtype)
{
    nvlog_flush();
    print_backtrace(sigtype);
    exit(-1);
}

bool do_nvlog=true;

int main()
{
  signal(SIGSEGV, segfaultHandler);
  
  int *globalmem_orig = (int*)malloc((globalmem_size+128)*sizeof(int));
  globalmem = globalmem_orig+128; // avoid accessing unallocated memory
	cache_tests_runall();
	run_stresstest_simple();
  run_stresstest_2proc();
  //run_timingtest_simple();
  //dump_stats_test();
  free(globalmem_orig);
}
