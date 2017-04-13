/*
 * perf_sim.h - MIPS performance simulator
 * Copyright 2017 MIPT-MIPS
 */

#ifndef PERF_SIM_H
#define PERF_SIM_H


// Generic C++ modules
#include <iostream>
#include <sstream>
#include <iomanip>


// MIPT-MIPS modules
#include "common/log.h"
#include "common/types.h"

#include "func_sim/func_instr/func_instr.h"
#include "func_sim/func_memory/func_memory.h"
#include "func_sim/func_sim.h"

#include "perf_sim/bpu/bpu.h"
#include "perf_sim_rf.h"
#include "ports.h"

#include "config.h"

class PerfMIPS : protected Log
{

//##############################################################################
//#                                 LATCHES                                    #
//##############################################################################
private:
    static const uint32 PORT_LATENCY = 1;
    static const uint32 PORT_FANOUT = 1;
    static const uint32 PORT_BW = 1;

     /* base class for all latches */
    class Latch
    {
        /* main simulator reference */
        PerfMIPS& sim;

    public:
        /* ports for sending stall information */
        std::unique_ptr<WritePort<bool>> write_stall_port;
        std::unique_ptr<ReadPort <bool>> read_stall_port;

        /* TODO: include data ports into base class (need to
         * create base port class for that, as ports are templated)
         */

        inline bool isStall()
        {
            if ( read_stall_port == nullptr)
                return false;

            bool is_stall = false;
            read_stall_port->read( &is_stall, sim.cycle);

            return is_stall;
        }

        inline void sendStall()
        {
            if ( write_stall_port != nullptr)
                write_stall_port->write( true, sim.cycle);
        }

        Latch(PerfMIPS& sim, WritePort<bool>* stall_wp = nullptr, ReadPort<bool>* stall_rp = nullptr) :
            sim ( sim),
            write_stall_port( stall_wp),
            read_stall_port( stall_rp)
        {}
    };


    /* latches between stages */
    class IfId : public Latch
    {
    public:
        struct Data {
            bool predicted_taken;    // Predicted direction
            addr_t predicted_target; // PC, predicted by BPU
            addr_t PC;               // current PC
            uint32 raw;              // fetched instruction code
        };

        std::unique_ptr<WritePort<Data>> write_data_port;
        std::unique_ptr<ReadPort <Data>> read_data_port;

        IfId(PerfMIPS& sim) :
            Latch ( sim,
                    new WritePort<bool>("DECODE_2_FETCH_STALL", PORT_BW, PORT_FANOUT),
                    new ReadPort <bool>("DECODE_2_FETCH_STALL", PORT_LATENCY)),
            write_data_port ( make_write_port<Data>("FETCH_2_DECODE", PORT_BW, PORT_FANOUT)),
            read_data_port  ( make_read_port <Data>("FETCH_2_DECODE", PORT_LATENCY))
        {}
    };

    class IdEx : public Latch
    {
    public:
        struct Data {
            bool predicted_taken;    // Predicted direction
            addr_t predicted_target; // PC, predicted by BPU
            addr_t target_not_taken; // PC which would be present if target was not taken
            FuncInstr instr;         // decoded instruction
        };

        std::unique_ptr<WritePort<Data>> write_data_port;
        std::unique_ptr<ReadPort <Data>> read_data_port;

        IdEx(PerfMIPS& sim) :
            Latch ( sim,
                    new WritePort<bool>("EXECUTE_2_DECODE_STALL", PORT_BW, PORT_FANOUT),
                    new ReadPort <bool>("EXECUTE_2_DECODE_STALL", PORT_LATENCY)),
            write_data_port    ( make_write_port<Data>("DECODE_2_EXECUTE", PORT_BW, PORT_FANOUT)),
            read_data_port     ( make_read_port <Data>("DECODE_2_EXECUTE", PORT_LATENCY))
        {}
    };

    class ExMem : public Latch
    {
    public:
        /* Data is the same as in IdEx latch */
        using Data = IdEx::Data;

        std::unique_ptr<WritePort<Data>> write_data_port;
        std::unique_ptr<ReadPort <Data>> read_data_port;

        ExMem(PerfMIPS& sim) :
            Latch ( sim,
                    new WritePort<bool>("MEMORY_2_EXECUTE_STALL", PORT_BW, PORT_FANOUT),
                    new ReadPort <bool>("MEMORY_2_EXECUTE_STALL", PORT_LATENCY)),
            write_data_port  ( make_write_port<Data>("EXECUTE_2_MEMORY", PORT_BW, PORT_FANOUT)),
            read_data_port   ( make_read_port <Data>("EXECUTE_2_MEMORY", PORT_LATENCY))
        {}
    };

    class MemWb : public Latch
    {
    public:
        std::unique_ptr<WritePort<FuncInstr>> write_data_port;
        std::unique_ptr<ReadPort <FuncInstr>> read_data_port;

        MemWb(PerfMIPS& sim) :
            Latch ( sim,
                    new WritePort<bool>("WRITEBACK_2_MEMORY_STALL", PORT_BW, PORT_FANOUT),
                    new ReadPort <bool>("WRITEBACK_2_MEMORY_STALL", PORT_LATENCY)),
            write_data_port  ( make_write_port<FuncInstr>("MEMORY_2_WRITEBACK", PORT_BW, PORT_FANOUT)),
            read_data_port   ( make_read_port <FuncInstr>("MEMORY_2_WRITEBACK", PORT_LATENCY))
        {}
    };


//##############################################################################
//#                                  STAGES                                    #
//##############################################################################
private:
    /* base abstract class defining interface */
    class Stage : protected Log {
    protected:
        /* main simulator reference */
        PerfMIPS& sim;

    private:
        /* name displayed in log */
        std::string name;

        /* references to latches */
        Latch* previous;
        Latch* next;

    protected:
        inline bool isStall()
        {
            if ( next != nullptr)
                return next->isStall();

            return false;
        }

        inline void sendStall()
        {
            if ( previous != nullptr )
                previous->sendStall();
        }

    public:
        Stage( PerfMIPS& sim, std::string name, bool log, Latch* previous, Latch* next) :
            Log( log),
            sim( sim),
            name( name),
            previous( previous),
            next( next)
        {}

        /* main function */
        inline void clock() {
            /* logging */
            sout << name << "cycle " << std::dec << sim.cycle << ":";

            if ( isStall())
            {
                sout << RED << "bubble (stall)" << DCOLOR << std::endl;
                //sendStall();
                return;
            }
            /* call virtual function, stage-specific */
            operate();
        }
        
        virtual void operate() = 0;
    };


    /* stages */
    class InstructionFetch : public Stage
    {
    public:
        using Stage::Stage;
        void operate();
    };


    class InstructionDecode : public Stage
    {
        /* need to save variables for cases of data hazards */
        IfId::Data data;

        bool is_anything_to_decode;

    public:
        /* flush for branch misprediction */
        inline void flushData() {
            is_anything_to_decode = false;
        }

        using Stage::Stage;
        void operate();
    };

    class Execute : public Stage
    {
    public:
        using Stage::Stage;
        void operate();
    };

    class MemoryAccess : public Stage
    {
    public:
        using Stage::Stage;
        void operate();
    };

    class Writeback : public Stage
    {
    public:
        using Stage::Stage;
        void operate();
    };


private:
    cycles_t cycle = 0;
    cycles_t executed_instrs = 0;
    /* to handle deadlocks */
    cycles_t last_writeback_cycle = 0;
    cycles_t instrs_to_run;

    /* latches */
    IfId if_id;
    IdEx id_ex;
    ExMem ex_mem;
    MemWb mem_wb;

    /* stages */
    InstructionFetch  fetch_stage;
    InstructionDecode decode_stage;
    Execute           execute_stage;
    MemoryAccess      memory_stage;
    Writeback         writeback_stage;

    /* additional modules */
    RF rf;               // registers
    FuncMemory memory;   // memory array
    BP bp;               // branch prediction unit

    /* program counter */
    addr_t PC;

    /* functional simulator to compare output */
    MIPS checker;

public:
    void check( const FuncInstr& instr);

    PerfMIPS( Config& handler);
    void run();
};

#endif
