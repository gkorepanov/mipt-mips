/*
 * perf_sim.cpp - mips performance simulator
 * Copyright 2017 MIPT-MIPS
 */

// MIPT-MIPS modules
#include "perf_sim.h"

//##############################################################################
//#                                SIMULATOR                                   #
//##############################################################################
PerfMIPS::PerfMIPS( Config& handler) :
    Log( handler.disassembly_on),
    instrs_to_run( handler.num_steps),

    /* units initialisation */
    memory( handler.binary_filename),
    bp ( handler.btb_size, handler.btb_ways),

    /* stages */
    fetch_stage    ( *this, handler.disassembly_on, memory.startPC()),
    decode_stage   ( *this, handler.disassembly_on),
    execute_stage  ( *this, handler.disassembly_on),
    memory_stage   ( *this, handler.disassembly_on),
    writeback_stage( *this, handler.disassembly_on)
{
    checker.init( handler.binary_filename);

    /* init ports of all types */
    Port<InstructionFetch::IfIdData>::init();
    Port<FuncInstr>::init();
    Port<bool>::init();
    Port<addr_t>::init();
}

void PerfMIPS::run() {
    while ( executed_instrs < instrs_to_run)
    {
        fetch_stage.clock();
        decode_stage.clock();
        execute_stage.clock();
        memory_stage.clock();
        writeback_stage.clock();

        ++cycle;
        if ( cycle - last_writeback_cycle >= 1000)
            serr << RED << "Deadlock was detected. The process will be aborted."
                 << DCOLOR << std::endl << critical;

        sout << "Executed instructions: " << executed_instrs
             << std::endl << std::endl;
    }

    auto ipc = ( (double) executed_instrs) / cycle;
    sout << separator << "IPC: " << ipc << separator << std::endl;
}

void PerfMIPS::check( const FuncInstr& instr)
{
    std::ostringstream perf_dump_s;
    perf_dump_s << instr << std::endl;
    std::string perf_dump = perf_dump_s.str();

    std::ostringstream checker_dump_s;
    checker.step(checker_dump_s);
    std::string checker_dump = checker_dump_s.str();

    if (checker_dump != perf_dump)
    {
        serr << "****************************" << std::endl
             << "Mismatch: " << std::endl
             << "Checker output: " << checker_dump
             << "PerfSim output: " << perf_dump
             << critical;
    }
}



//##############################################################################
//#                                  STAGES                                    #
//##############################################################################
void PerfMIPS::InstructionFetch::operate()
{
    /* creating structure to be sent to decode stage */
    IfIdData data;

    /* branch misprediction */
    bool is_flush = false;
    flushport_from_mem->read( &is_flush, sim.cycle);

    if ( is_flush)
        targetport_from_mem->read( &PC, sim.cycle); // updating PC

    /* fetching instruction */
    data.raw = sim.memory.read( PC);

    /* saving predictions and updating PC according to them */
    data.PC = PC;
    if ( sim.bp.predictTaken( PC))
    {
        data.predicted_taken = true;
        data.predicted_target = sim.bp.getTarget( PC);
    }
    else
    {
        data.predicted_taken = false;
        data.predicted_target = PC + 4;
    }
        
    /* sending to decode */
    dataport_to_id->write( data, sim.cycle);

    if ( isStall())
    {
        sout << RED << "bubble (stall)" << DCOLOR << std::endl;
        return; // if stall, do not update PC this time
    }

    /* updating PC according to prediction */
    PC = data.predicted_target;

    /* log */
    sout << GREEN << std::hex << "0x" << data.raw << DCOLOR << std::endl;
}


void PerfMIPS::InstructionDecode::operate()
{

     /* branch misprediction */
    bool is_flush = false;
    flushport_from_mem->read( &is_flush, sim.cycle);

    if ( is_flush)
    {
        /* ignoring the upcoming instruction as it is invalid */
        dataport_from_if->read( &data, sim.cycle);

        is_anything_to_decode = false;
        sout << RED << "flush\n" << DCOLOR;
        return;
    }

    /* TODO: remove this comment */
    // Port<IfId::Data>::lost( sim.cycle);

    if ( !is_anything_to_decode)
        /* acquiring data from fetch */
        is_anything_to_decode = dataport_from_if->read( &data, sim.cycle);

    /* TODO: remove this comment */
    //sout << "instr: " << std::hex << data.raw;

    if ( !is_anything_to_decode)
    {
        sout << RED << "bubble\n" << DCOLOR;
        return;
    }

    FuncInstr instr( data.raw, 
                     data.PC,
                     data.predicted_taken,
                     data.predicted_target);

    /* TODO: replace all this code by introducing Forwarding unit */
    if ( sim.rf.check( instr.get_src1_num()) &&
         sim.rf.check( instr.get_src2_num()) &&
         sim.rf.check( instr.get_dst_num())) // no data hazard
    {
        sim.rf.read_src1( instr);
        sim.rf.read_src2( instr);
        sim.rf.invalidate( instr.get_dst_num());

        is_anything_to_decode = 0; // successfully decoded

        dataport_to_ex->write( instr, sim.cycle);

        /* log */
        sout << GREEN << instr << DCOLOR << std::endl;
    }
    else // data hazard, stalling pipeline
    {
        sendStall(); // sending stall to decode
        sout << RED << "bubble (data hazard)\n" << DCOLOR;
    }
}

void PerfMIPS::Execute::operate()
{
    FuncInstr instr;

     /* branch misprediction */
    bool is_flush = false;
    flushport_from_mem->read( &is_flush, sim.cycle);

    if ( is_flush)
    {
        /* ignoring the upcoming instruction as it is invalid */
        dataport_from_id->read( &instr, sim.cycle);
        sout << RED << "flush\n" << DCOLOR;
        return;
    }

    if ( !dataport_from_id->read( &instr, sim.cycle))
    {
        sout << RED << "bubble\n" << DCOLOR;
        return;
    }

    instr.execute();

    dataport_to_mem->write( instr, sim.cycle);

    /* log */
    sout << GREEN << instr << DCOLOR << std::endl;
}


void PerfMIPS::MemoryAccess::operate()
{
    FuncInstr instr;

    /* branch misprediction */
    bool is_flush = false;
    flushport_from_mem->read( &is_flush, sim.cycle);

    if ( is_flush)
    {
        /* ignoring the upcoming instruction as it is invalid */
        dataport_from_ex->read( &instr, sim.cycle);
        sout << RED << "flush\n" << DCOLOR;
        return;
    }

    if ( !dataport_from_ex->read( &instr, sim.cycle))
    {
        sout << RED << "bubble\n" << DCOLOR;
        return;
    }

    /* acquiring real information */
    bool actually_taken = instr.isJump() && instr.jumpExecuted();
    addr_t real_target = instr.get_new_PC();

    /* updating BTB */
    sim.bp.update( actually_taken, instr.get_PC(), real_target);

    /* branch misprediction unit */
    if ( instr.misprediction())
    {
        /* flushing the pipeline */
        flushport_to_if-> write( true, sim.cycle);
        flushport_to_id-> write( true, sim.cycle);
        flushport_to_ex-> write( true, sim.cycle);
        flushport_to_mem->write( true, sim.cycle);

        /* sending valid PC to fetch stage */
        targetport_to_if->write( real_target, sim.cycle);

        /* if the register was marked invalid in decode stage, revert it */
        sim.rf.validate( instr.get_dst_num());

        sout << RED << "misprediction\n" << DCOLOR;
        return;
    }

    /* load/store */
    if ( instr.is_load())
    {
        instr.set_v_dst( sim.memory.read( instr.get_mem_addr(), 
                                          instr.get_mem_size()));
    }
    else if ( instr.is_store())
    {
        sim.memory.write( instr.get_v_src2(),
                          instr.get_mem_addr(),
                          instr.get_mem_size());
    }

    dataport_to_wb->write( instr, sim.cycle);

    /* log */
    sout << GREEN << instr << DCOLOR << std::endl;
}

void PerfMIPS::Writeback::operate()
{
    FuncInstr instr;
    if ( !dataport_from_mem->read( &instr, sim.cycle))
    {
        sout << RED << "bubble\n" << DCOLOR;
        return;
    }

    sim.rf.write_dst( instr);

    /* log */
    sout << GREEN << instr << DCOLOR << std::endl;

    /* perform checks */
    sim.check( instr);

    /* update sim cycles info */
    ++sim.executed_instrs;
    sim.last_writeback_cycle = sim.cycle;
}
