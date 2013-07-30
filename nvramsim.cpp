/*
 * Collect an address trace and estimate the execution time of an application.
 * When the buffer gets full, process the trace and empty it.
 * Repeat the process until the application finishes.
 *
 */
/*
 * A memory trace (Ip of memory accessing instruction and address of memory access - see
 * struct MEMREF) is collected by inserting Pin buffering API code into the application code,
 * via calls to INS_InsertFillBuffer. This analysis code writes a MEMREF into the
 * buffer being filled, and calls the registered BufferFull function (see call to
 * PIN_DefineTraceBuffer which defines the buffer and registers the BufferFull function)
 * when the buffer becomes full.
 * The BufferFull function processes the buffer and returns it to Pin to be filled again.
 *
 * Each application thread has it's own buffer - so multiple application threads do NOT
 * block each other on buffer accesses
 *
 * This tool is similar to memtrace_simple, but uses the Pin Buffering API
 */

//#include <assert.h>
#include <string.h>
#include <sstream>
//#include <iostream>
//#include <fstream>
//#include <map>
//#include <set>

#include "cache-sim/cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "pin.H"
#include "portability.H"
using namespace std;

// Latencies in number of cycles
const size_t L1Latency = 2;
const size_t L2Latency = 16;
const size_t DDRLatency = 80; // 40 ns at 2GHz
const size_t PCMLatency = 4000; // 2 us at 2GHz

// 1024x8 = 8 MB
// 2048x8 =  16 MB
// 4096x8 =  32 MB
// 8192x8 =  64 MB
// 16348x8 = 128 MB
// 32768x8 = 256 MB
// 1024x16 = 16 MB
// 2048x16 = 32 MB
// 4096x16 = 64 MB
// 8192x16 = 128 MB
// 16348x16 = 256 MB
const size_t DDR_size_MB = 128;  // please change this parameter!
const size_t DDR_line_bytes = 1024;
const size_t DDR_associativity = 8; // number of ways; probably should be fixed
const size_t DDR_sets = (DDR_size_MB*1024*1024)/DDR_line_bytes * (128/DDR_associativity);

// 1024x8 = 512 KB
// 2048x8 =  1 MB
// 4096x8 =  2 MB
// 8192x8 =  4 MB
// 16348x8 = 8 MB   <===
// 32768x8 = 16 MB
// 1024x16 = 1 MB
// 2048x16 = 2 MB
// 4096x16 = 4 MB
// 8192x16 = 8 MB
// 8192x16 = 8 MB
const size_t L2_sets = 16*1024;
const size_t L2_ways = 8; // the associativity in each set
const size_t L2_line_bytes = 64;

// 256x2 = 32 KB
// 128x4 = 32 KB
// 512x2 = 64 KB
// 256x4 = 64 KB
// 1024x2 = 128 KB
// 512x4 = 128 KB  <===
// 2048x2 = 256 KB
// 1024x4 = 256 KB
// 2048x4 = 512 KB
// 4096x2 = 512 KB
// 8192x2 = 1 MB
// 4096x4 = 1 MB
const size_t L1_sets = 512;
const size_t L1_ways = 4; // the associativity in each set
const size_t L1_line_bytes = 64;

MainMemory PCM(addr_space, PCMLatency);
Cache DDR( "DDR",             // string with cache instance name
	  &PCM,               // parent memory
	  DDR_sets,
	  DDR_associativity,
	  L2_line_bytes,
	  DDRLatency,
	  IS_WRITEBACK_CACHE
	  );
Cache L2( "L2",             // string with cache instance name
	  &DDR,             // parent memory
	  L2_sets,
	  L2_ways,
	  L2_line_bytes,
	  L2Latency,
	  IS_WRITEBACK_CACHE
	  );
Cache L1( "L1",             // string with cache instance name
	  &L2,              // parent layer in the memory hierarchy
	  L1_sets,
	  L1_ways,
	  L1_line_bytes,
	  L1Latency,
	  IS_WRITEBACK_CACHE
	  );

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "memtrace.out", "output file");
KNOB<UINT32> KnobNumPagesInBuffer(KNOB_MODE_WRITEONCE, "pintool", "num_pages_in_buffer", "256", "number of pages in buffer");


uint64_t num_instr = 0;
uint64_t num_memrefs = 0;
uint64_t cycles_memref = 0;
char base_directory[1024];

std::stringstream cmdline;

VOID stats_print()
{
    double exec_time = double(0.42*num_instr + cycles_memref) / (2*1024*1024*1024LLU);
    char fname_stats[sizeof(base_directory)+255];
    char *pos = strcpy(fname_stats, base_directory) + strlen(base_directory);
    *pos = '/';
    pos++;
    snprintf(pos, sizeof(fname_stats) - (pos - fname_stats), "nvramsim_stats_%d.txt", PIN_GetPid());
    fprintf(stderr, "NVRAMSIM: process %d is saving statistics to file '%s'\n", PIN_GetPid(), fname_stats);
    FILE *fstats = fopen(fname_stats, "wb");
    fprintf(fstats, "Command line,Instructions,Total memory references," \
	    "Avg cycles/mem ref,PCM read KB,PCM 64B reads,PCM 128B reads," \
	    "PCM write KB,PCM 64B writes,PCM 128B writes," \
	    "Estimated exec. time at 2GHz\n");
    fprintf(fstats, "\"%s\",%lu,%lu,%6.2lf,%lu,%lu,%lu,%lu,%lu,%lu,%4.2lf\n",
	    cmdline.str().c_str(),
	    num_instr, num_memrefs, double(cycles_memref)/num_memrefs, PCM.stats.hits_rd /* each read is for 1KB */,
	    PCM.stats.hits_rd*DDR_line_bytes/64, /* when PCM is in 64B blocks */
	    PCM.stats.hits_rd*DDR_line_bytes/128, /* when PCM is in 128B blocks */
	    PCM.stats.hits_wr, /* each write is for 1KB */
	    PCM.stats.hits_wr*DDR_line_bytes/64, /* when PCM is in 64B blocks */
	    PCM.stats.hits_wr*DDR_line_bytes/128, /* when PCM is in 128B blocks */
	    exec_time);
    fprintf(fstats, "\n==== Verbose description ====\n");
    fprintf(fstats, "Executed command: %s\n", cmdline.str().c_str());
    fprintf(fstats, "Process ID: %d\n", PIN_GetPid());
    fprintf(fstats, "Instructions: %lu (0.42 Cycles per Instruction; compile-time fixed)\n", num_instr);
    fprintf(fstats, "Total memory references: %lu (%6.2lf Cycles per Memory Reference; workload-dependent)\n", num_memrefs, double(cycles_memref)/num_memrefs);
    fprintf(fstats, "PCM reads: %lu KB. 64B reqs %lu 128B reqs: %lu\n",
	    PCM.stats.hits_rd, PCM.stats.hits_rd*DDR_line_bytes/64,
	    PCM.stats.hits_rd*DDR_line_bytes/128);
    fprintf(fstats, "PCM writes: %lu KB. 64B reqs %lu 128B reqs: %lu\n",
	    PCM.stats.hits_wr, PCM.stats.hits_wr*DDR_line_bytes/64,
	    PCM.stats.hits_wr*DDR_line_bytes/128);
    fprintf(fstats, "Estimated execution time on an in-order processor at 2GHz: %4.2lf seconds\n", exec_time);
    fclose(fstats);
    PCM.dump_stats();
}

/*
 * Struct of memory reference written to the buffer
 */
struct MEMREF
{
    ADDRINT ea;
    BOOL read;
};

// The buffer ID returned by the one call to PIN_DefineTraceBuffer
BUFFER_ID bufId;

// the Pin TLS slot that an application-thread will use to hold the APP_THREAD_REPRESENTITVE
// object that it owns
TLS_KEY appThreadRepresentitiveKey;

UINT32 totalBuffersFilled = 0;
UINT64 totalElementsProcessed = 0;

/*
 *
 * APP_THREAD_REPRESENTITVE
 *
 * Each application thread, creates an object of this class and saves it in it's Pin TLS
 * slot (appThreadRepresentitiveKey).
 */
class APP_THREAD_REPRESENTITVE
{
public:
	APP_THREAD_REPRESENTITVE(THREADID tid) {
		_numBuffersFilled = 0;
		_numElementsProcessed = 0;
	}
	~APP_THREAD_REPRESENTITVE() {}

	VOID ProcessBuffer(VOID *buf, UINT64 numElements);
	UINT32 NumBuffersFilled() {return _numBuffersFilled;}

	UINT32 NumElementsProcessed() {return _numElementsProcessed;}
private:
	UINT32 _numBuffersFilled;
	UINT32 _numElementsProcessed;
};

VOID APP_THREAD_REPRESENTITVE::ProcessBuffer(VOID *buf, UINT64 numElements)
{
	_numBuffersFilled++;

	struct MEMREF * memref=(struct MEMREF*)buf;
	uint8_t *data;
	for(UINT64 i=0; i<numElements; i++, memref++)
	{
//		if (memref->read)
//			cerr << "Recorded read @" << (void*)memref->ea << "\n";
//		else
//			cerr << "Recorded write @" << (void*)memref->ea << "\n";
		L1.line_get(memref->ea, (memref->read)?LINE_SHR:LINE_MOD, cycles_memref, data);
		num_memrefs++;
	}
	_numElementsProcessed += (UINT32)numElements;
}


VOID CountInstr(UINT32 numInstInBbl)
{
	num_instr += numInstInBbl;
}


/*
 * Insert code to write data to a thread-specific buffer for instructions
 * that access memory.
 */
VOID Trace(TRACE trace, VOID *v)
{
	// Insert a call to record the effective address.
	for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl=BBL_Next(bbl))
	{
		const uint64_t num_instr_bbl = BBL_NumIns(bbl);
		for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins=INS_Next(ins))
		{
			// Log every memory references of the instruction
			if (INS_IsMemoryRead(ins))
			{
				INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
						     IARG_MEMORYREAD_EA,
						     offsetof(struct MEMREF, ea),
						     IARG_BOOL, true,
						     offsetof(struct MEMREF, read),
						     IARG_END);
			}
			if (INS_IsMemoryWrite(ins))
			{
				INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
						     IARG_MEMORYWRITE_EA,
						     offsetof(struct MEMREF, ea),
						     IARG_BOOL, false,
						     offsetof(struct MEMREF, read),
						     IARG_END);
			}
			if (INS_HasMemoryRead2(ins))
			{
				INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
						     IARG_MEMORYREAD2_EA,
						     offsetof(struct MEMREF, ea),
						     IARG_BOOL, true,
						     offsetof(struct MEMREF, read),
						     IARG_END);
			}
		}
		BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)CountInstr, IARG_UINT32, num_instr_bbl, IARG_END);
	}
}


/**************************************************************************
 *
 *  Callback Routines
 *
 **************************************************************************/

/*!
 * Called when a buffer fills up, or the thread exits, so we can process it or pass it off
 * as we see fit.
 * @param[in] id		buffer handle
 * @param[in] tid		id of owning thread
 * @param[in] ctxt		application context
 * @param[in] buf		actual pointer to buffer
 * @param[in] numElements	number of records
 * @param[in] v			callback value
 * @return  A pointer to the buffer to resume filling.
 */
VOID * BufferFull(BUFFER_ID id, THREADID tid, const CONTEXT *ctxt, VOID *buf,
		  UINT64 numElements, VOID *v)
{
	APP_THREAD_REPRESENTITVE * appThreadRepresentitive = static_cast<APP_THREAD_REPRESENTITVE*>( PIN_GetThreadData( appThreadRepresentitiveKey, tid ) );
	appThreadRepresentitive->ProcessBuffer(buf, numElements);
	return buf;
}



VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
	// There is a new APP_THREAD_REPRESENTITVE for every thread.
	APP_THREAD_REPRESENTITVE * appThreadRepresentitive = new APP_THREAD_REPRESENTITVE(tid);

	// A thread will need to look up its APP_THREAD_REPRESENTITVE, so save pointer in TLS
	PIN_SetThreadData(appThreadRepresentitiveKey, appThreadRepresentitive, tid);
}


VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
	APP_THREAD_REPRESENTITVE * appThreadRepresentitive = static_cast<APP_THREAD_REPRESENTITVE*>(PIN_GetThreadData(appThreadRepresentitiveKey, tid));
	totalBuffersFilled += appThreadRepresentitive->NumBuffersFilled();
	totalElementsProcessed +=  appThreadRepresentitive->NumElementsProcessed();

	delete appThreadRepresentitive;

	PIN_SetThreadData(appThreadRepresentitiveKey, 0, tid);
}

VOID Fini(INT32 code, VOID *v)
{
	stats_print();
	printf ("totalBuffersFilled %u  totalElementsProcessed %14.0f\n", (totalBuffersFilled),
		static_cast<double>(totalElementsProcessed));
}

INT32 Usage()
{
	puts("\nThis tool estimates the execution time, using a simple memory model\n");
	puts(KNOB_BASE::StringKnobSummary().c_str());
	return -1;
}



int main(int argc, char * argv[])
{
	// Initialize PIN library. Print help message if -h(elp) is specified
	// in the command line or the command line is invalid
	if( PIN_Init(argc,argv) )
	{
		return Usage();
	}
	PIN_InitSymbols();

	if (!getcwd(base_directory, sizeof(base_directory)))
		perror("getcwd() error");

	{
		// get everything in the command line after '--'
		bool is_cmd = false;
		for (int i=0; i<argc; i++) {
			if (strcmp(argv[i], "--") == 0)
				is_cmd = true;
			else {
				if (is_cmd)
					cmdline << argv[i] << " ";
			}
		}
	}

	bufId = PIN_DefineTraceBuffer(sizeof(struct MEMREF), KnobNumPagesInBuffer,
				      BufferFull, 0);

	if(bufId == BUFFER_ID_INVALID)
	{
		printf ("Error: could not allocate initial buffer\n");
		return 1;
	}

	// Initialize thread-specific data not handled by buffering api.
	appThreadRepresentitiveKey = PIN_CreateThreadDataKey(0);

	// add an instrumentation function
	TRACE_AddInstrumentFunction(Trace, 0);

	// add callbacks
	PIN_AddThreadStartFunction(ThreadStart, 0);
	PIN_AddThreadFiniFunction(ThreadFini, 0);
	PIN_AddFiniFunction(Fini, 0);

	// Start the program, never returns
	PIN_StartProgram();

	return 0;
}

