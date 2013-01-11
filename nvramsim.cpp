/*
 * Collect an address trace and estimate the execution time of an application.
 * When the buffer gets full, process the trace and empty it.
 * Repeat the process until the application finishes.
 *
 */

#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <map>
#include <set>
#include "pin.H"
#include "portability.H"


REG memref_log_ptr;

KNOB<string> KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "memtrace.out", "output file");

int main_PID = 0;

uint64_t num_instr = 0;
uint64_t num_memrefs = 0;

struct MemRefs
{
    struct OneMemRef {
        ADDRINT effective_addr;
        BOOL read;
        OneMemRef(ADDRINT _eff_addr, BOOL _read) {
            effective_addr = _eff_addr;
            read = _read;
        }
    };
    vector<OneMemRef> memrefs_list;
    MemRefs() {
        memrefs_list.reserve(10 * 1000);
    }
    void Reset() {
//        cerr << "Recorded " << memrefs_list.size() << " instructions\n";
        memrefs_list.clear();
    }

    static void PIN_FAST_ANALYSIS_CALL recordMemRef(MemRefs *memrefs, ADDRINT effective_addr, BOOL read) {
        memrefs->memrefs_list.push_back(OneMemRef(effective_addr, read));
    }
};

struct ToInstrument
{
    struct OneInstr {
        INS ins;
        IARG_TYPE effective_addr_placeholder;
        OneInstr(INS _ins, IARG_TYPE _eff_addr_ph) {
            ins = _ins;
            effective_addr_placeholder = _eff_addr_ph;
        }

        void OneInstrInstrument()
        {
            INS_InsertCall(ins, IPOINT_BEFORE, AFUNPTR(MemRefs::recordMemRef),
                           IARG_FAST_ANALYSIS_CALL,
                           IARG_REG_VALUE, memref_log_ptr,
                           IARG_ADDRINT, effective_addr_placeholder,
                           IARG_BOOL, true,
                           IARG_END);
        }
    };
    vector<OneInstr> ins_list;
    ToInstrument() {
        ins_list.reserve(1000);
    }

    void prepareInstrumentation(INS ins, IARG_TYPE itype) {
        ins_list.push_back(OneInstr(ins, itype));
    }

    void InstrumentAll() {
        for (vector<OneInstr>::iterator it = ins_list.begin(); it != ins_list.end(); ++it)
        {
            it->OneInstrInstrument();
        }
    }
};

VOID CountInstr(UINT32 numInstInBbl)
{
//    if (!num_instr)
//        fprintf(stderr, "NVRAMSIM: instr increment by %u: thread_id %d, pid %d\n", numInstInBbl, PIN_ThreadId(), PIN_GetPid());
    num_instr += numInstInBbl;
}




void InstrumentTrace(TRACE trace, void *)
{
    ToInstrument mem_instr;
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        const uint64_t num_instr_bbl = BBL_NumIns(bbl);
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            if (INS_IsMemoryRead(ins))
            {
                mem_instr.prepareInstrumentation(ins, IARG_MEMORYREAD_EA);
            }
            if (INS_IsMemoryWrite(ins))
            {
                mem_instr.prepareInstrumentation(ins, IARG_MEMORYWRITE_EA);
            }
            if (INS_HasMemoryRead2(ins))
            {
                mem_instr.prepareInstrumentation(ins, IARG_MEMORYREAD2_EA);
            }
        }
        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)CountInstr, IARG_UINT32, num_instr_bbl, IARG_END);
    }

    mem_instr.InstrumentAll();
}

VOID stats_print()
{
    fprintf(stderr, "Execution stats:\n");
    fprintf(stderr, "  total %lu instructions\n", num_instr);
    fprintf(stderr, "  total %lu memory references\n", num_memrefs);
    double exec_time = double(0.42*num_instr + 2*num_memrefs) / (2*1024*1024*1024LLU);
    fprintf(stderr, "Estimated execution time on an in-order processor at 2GHz with 1 IPC:\n");
    fprintf(stderr, "  %4.2lf seconds\n", exec_time);
}

VOID ThreadStart(THREADID tid, CONTEXT *ctxt, INT32 flags, VOID *v)
{
    fprintf(stderr, "NVRAMSIM: thread start; num_instr %lu, num_memrefs %lu\n", num_instr, num_memrefs);
    if (!main_PID) main_PID = PIN_GetPid();
    MemRefs * memrefs = new MemRefs();
    PIN_SetContextReg(ctxt, memref_log_ptr, reinterpret_cast<ADDRINT>(memrefs));
}

VOID ThreadFini(THREADID tid, const CONTEXT *ctxt, INT32 code, VOID *v)
{
    fprintf(stderr, "NVRAMSIM: thread %u finish (pid %u)\n", PIN_ThreadId(), PIN_GetPid());
    fprintf(stderr, "NVRAMSIM: finish: num_instr %lu, num_memrefs %lu\n", num_instr, num_memrefs);
    MemRefs *memrefs = reinterpret_cast<MemRefs *>(PIN_GetContextReg(ctxt, memref_log_ptr));
    num_memrefs += memrefs->memrefs_list.size();
    memrefs->Reset();
    delete memrefs;
}

INT32 Usage()
{
    puts("\nThis tool estimates the execution time, using a simple memory model\n");
    puts(KNOB_BASE::StringKnobSummary().c_str());
    return -1;
}

VOID SyscallBefore(THREADID tid, CONTEXT *ctxt, SYSCALL_STANDARD scStd,
                  VOID *arg)
{
    MemRefs *memrefs = reinterpret_cast<MemRefs *>(PIN_GetContextReg(ctxt, memref_log_ptr));
    num_memrefs += memrefs->memrefs_list.size();
    memrefs->Reset();
}

VOID AppExit(INT32 code, VOID *v)
{
    if (PIN_GetPid() != main_PID)
        return;
//    callStack.DumpStack(&cerr);
    stats_print();
}





PIN_LOCK lock;
pid_t parent_pid;

VOID BeforeFork(THREADID threadid, const CONTEXT* ctxt, VOID * arg)
{
    GetLock(&lock, threadid+1);
    cerr << "TOOL: Before fork." << endl;
    ReleaseLock(&lock);
    parent_pid = PIN_GetPid();
}

int main(int argc, char * argv[])
{
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }
    PIN_InitSymbols();

    memref_log_ptr = PIN_ClaimToolRegister();

    if (! (REG_valid(memref_log_ptr)) )
    {
        std::cerr << "Cannot allocate register: memref_log_ptr\n";
        std::cerr << std::flush;
        return 1;
    }

    TRACE_AddInstrumentFunction(InstrumentTrace, 0);
    PIN_AddForkFunction(FPOINT_BEFORE, BeforeFork, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddSyscallEntryFunction(SyscallBefore, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(AppExit, 0);

    PIN_StartProgram();

    return 0;
}














