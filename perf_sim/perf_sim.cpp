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
    /* latches initialisation */
    if_id( *this),
    id_ex( *this),
    ex_mem( *this),
    mem_wb( *this),
    /* stages naming and connection */
    fetch_stage    ( *this, "fetch   ", handler.disassembly_on, nullptr, &if_id  ),
    decode_stage   ( *this, "decode  ", handler.disassembly_on, &if_id,  &id_ex  ),
    execute_stage  ( *this, "execute ", handler.disassembly_on, &id_ex,  &ex_mem ),
    memory_stage   ( *this, "memory  ", handler.disassembly_on, &ex_mem, &mem_wb ),
    writeback_stage( *this, "wb      ", handler.disassembly_on, &mem_wb, nullptr),
    /* units initialisation */
    memory( handler.binary_filename),
    bp ( handler.btb_size, handler.btb_ways),
    /* define program counter */
    PC ( memory.startPC())
{
    checker.init( handler.binary_filename);

    /* init ports of all types */
    Port<IfId::Data>::init();
    Port<IdEx::Data>::init();
    Port<FuncInstr>::init();
    Port<bool>::init();
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
            serr << "Deadlock was detected. The process will be aborted.\n\n" << critical;

        sout << "Executed instructions: " << executed_instrs << std::endl << std::endl;
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
    /* creating strcuture to be sent to decode stage */
    IfId::Data data;

    /* fetching instruction */
    data.raw = sim.memory.read(sim.PC);

    /* saving predcitions and updating PC according to them */
    data.PC = sim.PC;
    if ( sim.bp.predictTaken( sim.PC))
    {
        data.predicted_taken = true;
        data.predicted_target = sim.bp.getTarget( sim.PC);
    }
    else
    {
        data.predicted_taken = false;
        data.predicted_target = sim.PC + 4;
    }
        /* updating PC according to prediction */
        sim.PC = data.predicted_target;

    /* sending to decode */
    sim.if_id.write_data_port->write( data, sim.cycle);

    /* log */
    sout << GREEN << std::hex << "0x" << data.raw << DCOLOR << std::endl;
}


void PerfMIPS::InstructionDecode::operate()
{
    /* TODO: remove this comment */
    // Port<IfId::Data>::lost( sim.cycle);

    if ( !is_anything_to_decode)
        /* acquiring data from fetch */
        is_anything_to_decode = sim.if_id.read_data_port->read( &data, sim.cycle);

    /* TODO: remove this comment */
    //sout << "instr: " << std::hex << data.raw;

    if ( !is_anything_to_decode)
    {
        sout << RED << "bubble\n" << DCOLOR;
        return;
    }

    FuncInstr instr( data.raw, data.PC);

    /* TODO: replace all this code by introducing Forwarding unit */
    if ( sim.rf.check( instr.get_src1_num()) &&
         sim.rf.check( instr.get_src2_num()) &&
         sim.rf.check( instr.get_dst_num())) // no data hazard
    {
        sim.rf.read_src1( instr);
        sim.rf.read_src2( instr);
        sim.rf.invalidate( instr.get_dst_num());

        IdEx::Data data_to_send;
        data_to_send.instr = instr;
        data_to_send.predicted_target = data.predicted_target;
        data_to_send.target_not_taken = data.PC + 4;
        data_to_send.predicted_taken  = data.predicted_taken;

        is_anything_to_decode = 0; // successfully decoded

        sim.id_ex.write_data_port->write( data_to_send, sim.cycle);

        /* log */
        sout << GREEN << instr << DCOLOR << std::endl;
    } else // data hazard, stalling pipeline
    {
        sendStall();
        sout << RED << "bubble (data hazard)\n" << DCOLOR;
    }
}

void PerfMIPS::Execute::operate()
{
    IdEx::Data data;
    if ( !sim.id_ex.read_data_port->read( &data, sim.cycle))
    {
        sout << RED << "bubble\n" << DCOLOR;
        return;
    }

    data.instr.execute();

    sim.ex_mem.write_data_port->write( data, sim.cycle);

    sout << GREEN << data.instr << DCOLOR << std::endl;
}


void PerfMIPS::MemoryAccess::operate()
{
    ExMem::Data data;
    if ( !sim.ex_mem.read_data_port->read( &data, sim.cycle))
    {
        sout << RED << "bubble\n" << DCOLOR;
        return;
    }

    /* acquiring real information */
    bool actually_taken = data.instr.isJump() && data.instr.jumpExecuted();
    addr_t real_target = data.instr.get_new_PC();

    /* updating BTB */
    sim.bp.update( actually_taken, data.target_not_taken - 4, real_target);

    /* branch misprediction unit */
    if ( (actually_taken != data.predicted_taken) ||
         ( (actually_taken == data.predicted_taken) &&
           (real_target    != data.predicted_target)))
    {
        /* flushing the pipeline */

        /* flushing data ports */
        sim.if_id.read_data_port->flush();
        sim.id_ex.read_data_port->flush();
        sim.ex_mem.read_data_port->flush();

        /* flushing stall ports */
        sim.if_id.read_stall_port->flush();
        sim.id_ex.read_stall_port->flush();
        sim.ex_mem.read_stall_port->flush();
        sim.mem_wb.read_stall_port->flush();

        /* updating the PC with actual value */
        sim.PC = real_target;

        /* delete internal data from stages if there was some */
        sim.decode_stage.flushData();

        sout << RED << "misprediction\n" << DCOLOR;
        return;
    }

    FuncInstr& instr = data.instr;
    /* load/store */
    if ( instr.is_load())
        instr.set_v_dst( sim.memory.read( instr.get_mem_addr(), instr.get_mem_size()));
    else if ( instr.is_store())
        sim.memory.write( instr.get_v_src2(), instr.get_mem_addr(), instr.get_mem_size());

    sim.mem_wb.write_data_port->write( data.instr, sim.cycle);

    sout << GREEN << instr << DCOLOR << std::endl;
}

void PerfMIPS::Writeback::operate()
{
    FuncInstr instr;
    if ( !sim.mem_wb.read_data_port->read( &instr, sim.cycle))
    {
        sout << RED << "bubble\n" << DCOLOR;
        return;
    }

    sim.rf.write_dst( instr);

    sout << GREEN << instr << DCOLOR << std::endl;

    sim.check( instr);

    ++sim.executed_instrs;
    sim.last_writeback_cycle = sim.cycle;
}
