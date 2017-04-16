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
#include <common/log.h>
#include <common/types.h>

#include <func_sim/func_instr/func_instr.h>
#include <func_sim/func_memory/func_memory.h>
#include <func_sim/func_sim.h>

#include "bpu/bpu.h"
#include "perf_sim_rf.h"
#include "ports.h"

#include "config.h"

class PerfMIPS : protected Log
{
    static const uint32 PORT_LATENCY = 1;
    static const uint32 PORT_FANOUT = 1;
    static const uint32 PORT_BW = 1;

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

    public:
        Stage( PerfMIPS& sim,
               std::string name,
               bool log) :
            Log( log),
            sim( sim),
            name( name)
        {}

        /* main function */
        inline void clock() {
            /* logging */
            sout << name << "cycle   " << std::dec << sim.cycle << ":";

            /* call virtual function, stage-specific */
            operate();
        }

        virtual void operate() = 0;
    };


    /* stages */
    class InstructionFetch : public Stage
    {
    public:
        /* the struture of data sent from fetch to decode stage */
        struct IfIdData {
            bool predicted_taken;    // Predicted direction
            addr_t predicted_target; // PC, predicted by BPU
            addr_t PC;               // current PC
            uint32 raw;              // fetched instruction code
        };

    private:
        addr_t PC;

        /* generic data ports */
        std::unique_ptr<WritePort<IfIdData>> dataport_to_id;

        /* for receiving stall signal */
        std::unique_ptr<ReadPort <bool>> stallport_from_id;

        /* for branch misprediction unit */
        std::unique_ptr<ReadPort <bool>>    flushport_from_mem;
        std::unique_ptr<ReadPort <addr_t>> targetport_from_mem;

        inline bool isStall()
        {
            bool is_stall = false;
            stallport_from_id->read( &is_stall, sim.cycle);

            return is_stall;
        }

    public:
        InstructionFetch( PerfMIPS& sim, bool log, addr_t PC) :
            Stage( sim, "fetch   ", log),
            PC( PC),

            /* data ports */
            dataport_to_id      ( make_write_port<IfIdData> 
                                ( "IF_2_ID_DATA",   PORT_BW, PORT_FANOUT)),
            /* stall ports */
            stallport_from_id   ( make_read_port <bool>     
                                ( "ID_2_IF_STALL",  PORT_LATENCY)),
            /* BMU ports */
            flushport_from_mem  ( make_read_port <bool>     
                                ( "MEM_2_IF_FLUSH", PORT_LATENCY)),
            targetport_from_mem ( make_read_port <addr_t>      
                                ( "MEM_2_IF_TARGET", PORT_LATENCY))
        {} 

        void operate();
    };


    class InstructionDecode : public Stage
    {
        using IfIdData = InstructionFetch::IfIdData;

        /* need to save variables for cases of data hazards */
        IfIdData data;
        bool data_hazard = false;

        /* generic data ports */
        std::unique_ptr<ReadPort <IfIdData>> dataport_from_if;
        std::unique_ptr<WritePort<FuncInstr>>  dataport_to_ex;

        /* for sending stall signal */
        std::unique_ptr<WritePort<bool>> stallport_to_if;

        /* for branch misprediction unit */
        std::unique_ptr<ReadPort <bool>> flushport_from_mem;

        inline void sendStall() { stallport_to_if->write( true, sim.cycle); }

    public:
        InstructionDecode( PerfMIPS& sim, bool log) :
            Stage( sim, "decode  ", log),

            /* data ports */
            dataport_from_if    ( make_read_port <IfIdData>  
                                ( "IF_2_ID_DATA",    PORT_LATENCY)),
            dataport_to_ex      ( make_write_port<FuncInstr> 
                                ( "ID_2_EX_DATA",    PORT_BW, PORT_FANOUT)),
            /* stall ports */
            stallport_to_if     ( make_write_port<bool>      
                                ( "ID_2_IF_STALL",   PORT_BW, PORT_FANOUT)),
            /* BMU ports */
            flushport_from_mem  ( make_read_port <bool>      
                                ( "MEM_2_ID_FLUSH",  PORT_LATENCY))

        {}

        void operate();
    };

    class Execute : public Stage
    {
        /* generic data ports */
        std::unique_ptr<ReadPort <FuncInstr>> dataport_from_id;
        std::unique_ptr<WritePort<FuncInstr>>  dataport_to_mem;

        /* for branch misprediction unit */
        std::unique_ptr<ReadPort <bool>> flushport_from_mem;

    public:
        Execute( PerfMIPS& sim, bool log) :
            Stage( sim, "execute ", log),

            /* data ports */
            dataport_from_id    ( make_read_port <FuncInstr> 
                                ( "ID_2_EX_DATA",    PORT_LATENCY)),
            dataport_to_mem     ( make_write_port<FuncInstr> 
                                ( "EX_2_MEM_DATA",   PORT_BW, PORT_FANOUT)),
            /* BMU ports */
            flushport_from_mem  ( make_read_port <bool>      
                                ( "MEM_2_EX_FLUSH",  PORT_LATENCY))       
        {}

        void operate();
    };

    class MemoryAccess : public Stage
    {
        /* generic data ports */
        std::unique_ptr<ReadPort <FuncInstr>> dataport_from_ex;
        std::unique_ptr<WritePort<FuncInstr>>   dataport_to_wb;

        /* for branch misprediction unit */
        std::unique_ptr<ReadPort <bool>> flushport_from_mem;

        std::unique_ptr<WritePort<bool>>    flushport_to_id;
        std::unique_ptr<WritePort<bool>>    flushport_to_if;
        std::unique_ptr<WritePort<bool>>    flushport_to_ex;
        std::unique_ptr<WritePort<bool>>   flushport_to_mem;

        std::unique_ptr<WritePort<addr_t>> targetport_to_if;


    public:
        MemoryAccess( PerfMIPS& sim, bool log) :
            Stage( sim, "memory  ", log),

            /* data ports */
            dataport_from_ex    ( make_read_port <FuncInstr> 
                                ( "EX_2_MEM_DATA",   PORT_LATENCY)),
            dataport_to_wb      ( make_write_port<FuncInstr>
                                ( "MEM_2_WB_DATA",   PORT_BW, PORT_FANOUT)),
            /* BMU ports */
            flushport_from_mem  ( make_read_port <bool>
                                ( "MEM_2_MEM_FLUSH", PORT_LATENCY)),
            flushport_to_id     ( make_write_port <bool>     
                                ( "MEM_2_IF_FLUSH",  PORT_BW, PORT_FANOUT)),
            flushport_to_if     ( make_write_port <bool>     
                                ( "MEM_2_ID_FLUSH",  PORT_BW, PORT_FANOUT)),
            flushport_to_ex     ( make_write_port <bool>     
                                ( "MEM_2_EX_FLUSH",  PORT_BW, PORT_FANOUT)),
            flushport_to_mem    ( make_write_port <bool>     
                                ( "MEM_2_MEM_FLUSH", PORT_BW, PORT_FANOUT)),
            targetport_to_if    ( make_write_port <addr_t>   
                                ( "MEM_2_IF_TARGET", PORT_BW, PORT_FANOUT))
        {}

        void operate();
    };

    class Writeback : public Stage
    {
        std::unique_ptr<ReadPort<FuncInstr>> dataport_from_mem;

    public:
        Writeback( PerfMIPS& sim, bool log) :
            Stage( sim, "wb     ", log),

            /* data ports */
            dataport_from_mem   ( make_read_port <FuncInstr>
                                ( "MEM_2_WB_DATA",   PORT_LATENCY))
        {}

        void operate();
    };

//##############################################################################
//#                                SIMULATOR                                   #
//##############################################################################

private:
    cycles_t cycle = 0;
    cycles_t executed_instrs = 0;
    /* to handle deadlocks */
    cycles_t last_writeback_cycle = 0;
    cycles_t instrs_to_run;

    /* additional modules */
    FuncMemory memory;   // memory array
    BP bp;               // branch prediction unit
    RF rf;               // registers

    /* stages */
    InstructionFetch  fetch_stage;
    InstructionDecode decode_stage;
    Execute           execute_stage;
    MemoryAccess      memory_stage;
    Writeback         writeback_stage;

    /* functional simulator to compare output */
    MIPS checker;

public:
    void check( const FuncInstr& instr);

    PerfMIPS( Config& handler);
    void run();
};

#endif
