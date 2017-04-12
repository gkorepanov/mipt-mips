/*
 * bpu.cpp - the branch prediction unit for MIPS
 * @author George Korepanov <gkorepanov.gk@gmail.com>
 * Copyright 2017 MIPT-MIPS
 */

// MIPT-MIPS modules
 #include "bpu.h"

void BP::BPEntry::reset()
{
    std::fill( state_table.begin(), state_table.end(), bp.default_state);
    current_pattern = bp.default_pattern;
}
void BP::BPEntry::update( bool is_actually_taken, addr_t target)
{
    /* TODO:
     * determine whether in case of target change we should
     * reset the state or update it according the actual direction
     */

     // For now the decision is just to update the target and reset information
    if ( is_actually_taken && ( _target != target))
    {
        reset();
        _target = target;
    }

    current_pattern = ( ( current_pattern << 1) & bp.pattern_mask) + ( is_actually_taken ? 1 : 0);

    unsigned short& state = state_table[ current_pattern];
    state += ( is_actually_taken ? 1 : -1);

    /* Handy implemetation of saturation arithmetics. It's quite simple
     * and just keeps `state` in range [0, STATE_MAX]
     */
    if ( state & ( bp.mean_state << 1))
        state = ( ~state) & ( ( bp.mean_state << 1) - 1);

    return;
}


BP::BP( unsigned int   size_in_entries,
        unsigned int   ways,
        unsigned short prediction_bits,
        unsigned short prediction_level,
        unsigned short branch_ip_size_in_bits) :
    prediction_bits( prediction_bits),
    mean_state( 1ull << ( prediction_bits - 1)),
    default_state( mean_state - 1),
    prediction_level( prediction_level),
    default_pattern( 0),
    pattern_mask( ( 1ull << prediction_level) - 1),
    set_mask( ( size_in_entries / ways ) - 1),
    data( ways, std::vector<BPEntry>( size_in_entries / ways, BPEntry( *this))),
    tags( size_in_entries,
          ways,
          1,
          branch_ip_size_in_bits)
{}


addr_t BP::getPC( addr_t PC) {
    unsigned int way;
    if ( tags.read( PC, &way)) // hit
    {
        BPEntry predict = data[ way][ getSetNum( PC)];

        if ( predict.isTaken())  // predicted taken
            return predict.target();
    }

    return PC + 4;
}


void BP::update( bool is_actually_taken, addr_t branch_ip, addr_t target)
{
    unsigned int way;
    tags.write( branch_ip, &way);
    data[ way][ getSetNum( branch_ip)].update( is_actually_taken, target);
}
