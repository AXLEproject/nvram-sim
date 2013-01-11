/*BEGIN_LEGAL
Intel Open Source License

Copyright (c) 2002-2012 Intel Corporation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*
 * Collect an address trace in a lightweight, thread-safe, buffer
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


#include "pin.H"
#include <iostream>
#include <stdio.h>
#include <stddef.h>

/* Struct for holding memory references.  Rather than having two separate
 * buffers for loads and stores, we just use one struct that includes a
 * flag for type.
 */
struct MEMREF
{
    ADDRINT pc;
    ADDRINT address;
    UINT32 size;
    UINT32 load;
};

//FILE *outfile_trace;
FILE *outfile_stats;

BUFFER_ID bufId;
//PIN_LOCK fileLock;
TLS_KEY buf_key;

UINT64 insCount = 0;        // number of executed instructions
UINT64 memRdCount = 0;      // number of memory loads
UINT64 memWrCount = 0;      // number of memory stores

#define NUM_BUF_PAGES 8192


///* ===================================================================== */
//// Command line switches
///* ===================================================================== */
//KNOB<string> KnobStatsFilename(KNOB_MODE_WRITEONCE,  "pintool",
//                               "o", "stats_nvramsim.txt", "specify file name for nvramsim output");

//KNOB<string> KnobTraceFilename(KNOB_MODE_WRITEONCE, "pintool",
//                               "t", "memops_trace", "output file");

//KNOB<BOOL> KnobEmitTrace(KNOB_MODE_WRITEONCE, "pintool",
//                         "emit", "1", "emit a trace in the output file");


/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool demonstrates the basic use of the buffering API." << endl << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}

VOID CountInstructions(UINT64 numInstInBbl)
{
    insCount += numInstInBbl;
}

VOID Trace(TRACE trace, VOID *v){

    UINT32 refSize;

    for(BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl=BBL_Next(bbl)) {

        UINT32 numInstr = BBL_NumIns(bbl);

        for(INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins=INS_Next(ins)) {

            if(INS_IsMemoryRead(ins)){

                refSize = INS_MemoryReadSize(ins);

                INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                    IARG_INST_PTR, offsetof(struct MEMREF, pc),
                    IARG_MEMORYREAD_EA, offsetof(struct MEMREF, address),
                    IARG_UINT32, refSize, offsetof(struct MEMREF, size),
                    IARG_UINT32, 1, offsetof(struct MEMREF, load),
                    IARG_END);

            }
            if(INS_HasMemoryRead2(ins)){

                refSize = INS_MemoryReadSize(ins);

                INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                    IARG_INST_PTR, offsetof(struct MEMREF, pc),
                    IARG_MEMORYREAD2_EA, offsetof(struct MEMREF, address),
                    IARG_UINT32, refSize, offsetof(struct MEMREF, size),
                    IARG_UINT32, 1, offsetof(struct MEMREF, load),
                    IARG_END);

            }
            if(INS_IsMemoryWrite(ins)){

                refSize = INS_MemoryWriteSize(ins);

                INS_InsertFillBuffer(ins, IPOINT_BEFORE, bufId,
                    IARG_INST_PTR, offsetof(struct MEMREF, pc),
                    IARG_MEMORYWRITE_EA, offsetof(struct MEMREF, address),
                    IARG_UINT32, refSize, offsetof(struct MEMREF, size),
                    IARG_UINT32, 0, offsetof(struct MEMREF, load),
                    IARG_END);
            }
        }

        BBL_InsertCall(bbl, IPOINT_ANYWHERE, (AFUNPTR)CountInstructions, IARG_UINT32, numInstr, IARG_END);

    }
}

/*!
 * Called when a buffer fills up, or the thread exits, so we can process it or pass it off
 * as we see fit.
 * @param[in] id		buffer handle
 * @param[in] tid		id of owning thread
 * @param[in] ctxt		application context when the buffer filled
 * @param[in] buf		actual pointer to buffer
 * @param[in] numElements	number of records
 * @param[in] v			callback value
 * @return  A pointer to the buffer to resume filling.
 */
VOID * BufferFull(BUFFER_ID bid, THREADID tid, const CONTEXT *ctxt, VOID *buf,
                  UINT64 numElements, VOID *v)
{
//    GetLock(&fileLock, 1);

    ASSERTX(buf == PIN_GetThreadData(buf_key, tid));

    struct MEMREF* reference=(struct MEMREF*)buf;
    UINT64 i;

    for(i=0; i<numElements; i++, reference++){
//        fprintf(outfile_trace, "%lx %lx %u %u\n", (unsigned long)reference->pc, (unsigned long)reference->address,
//                reference->size, reference->load);
        // FIXME make it thread-safe
        memRdCount += reference->load;
        memWrCount += !reference->load;
    }
//    fflush(outfile_trace);
//    ReleaseLock(&fileLock);

    return buf;
}

/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID *v)
{
//    GetLock(&fileLock, 1);
//    fclose(outfile_trace);
//    printf("trace outfile closed\n");

//    string statsFileName = KnobStatsFilename.Value();
//    if (!statsFileName.empty()) { stats_out = new std::ofstream(statsFileName.c_str()); }
    outfile_stats = fopen("nvramsim_stats.txt", "w");
    if(outfile_stats){
        unsigned long exec_time = insCount + 2*memRdCount + 2*memWrCount; // FIXME
        fprintf(outfile_stats, "nvramsim analysis results: \n");
        fprintf(outfile_stats, "  Number of instructions: %lu\n", insCount);
        fprintf(outfile_stats, "  Number of memory reads: %lu\n", memRdCount);
        fprintf(outfile_stats, "  Number of memory writes: %lu\n", memWrCount);
        fprintf(outfile_stats, "  Estimated execution time (in cycles): %lu\n", exec_time);
    } else {
        cerr << "ERROR: Couldn't open nvramsim_stats.txt" << endl;
        return;
    }

//    ReleaseLock(&fileLock);
}

void ThreadStart(THREADID tid, CONTEXT * context, int flags, void * v)
{
    // We check that we got the right thing in the buffer full callback
    PIN_SetThreadData(buf_key, PIN_GetBufferPointer(context, bufId), tid);
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments,
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char *argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    // Initialize the memory reference buffer
    bufId = PIN_DefineTraceBuffer(sizeof(struct MEMREF), NUM_BUF_PAGES,
                                  BufferFull, 0);

    if(bufId == BUFFER_ID_INVALID){
        cerr << "Error allocating initial buffer" << endl;
        return -1;
    }

//    outfile_trace = fopen("nvramsim_trace.out", "w");
//    if(!outfile_trace){
//        cerr << "Couldn't open nvramsim_trace.out" << endl;
//        return -1;
//    }

//    InitLock(&fileLock);
//    cerr <<  "===============================================" << endl;
//    cerr <<  "This application is instrumented by nvramsim" << endl;
//    if (!KnobStatsFilename.Value().empty())
//    {
//        cerr << "See file " << KnobStatsFilename.Value() << " for analysis results" << endl;
//    }
//    cerr <<  "===============================================" << endl;

    // add an instrumentation function
    TRACE_AddInstrumentFunction(Trace, 0);

    // Register function to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    buf_key = PIN_CreateThreadDataKey(0);
    PIN_AddThreadStartFunction(ThreadStart, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}































