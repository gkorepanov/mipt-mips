/*
 * bpu.cpp - the branch prediction unit for MIPS
 * @author George Korepanov <gkorepanov.gk@gmail.com>
 * Copyright 2017 MIPT-MIPS
 */

// MIPT-MIPS modules
 #include "bpu.h"

void BP::BPEntry::update( bool is_actually_taken, addr_t target)
{
    /* TODO:
     * determine whether in case of target change we should
     * reset the state or update it according the actual Direction
     */
    if ( _target != target)
    {
        state = default_state;
        _target = target;
        return;
    }

    state += ( is_actually_taken ? 1 : -1);

    /* Handy implemetation of saturation arithmetics. It's quite simple
     * and just keeps `state` in range [0, STATE_MAX]
     */
    if ( state & ( mean_state << 1))
        state = ( ~state) & ( ( mean_state << 1) - 1);

    return;
}


BP::BP( unsigned int   size_in_entries,
        unsigned int   ways,
        unsigned short prediction_bits,
        unsigned short branch_ip_size_in_bits = 32) :
    prediction_bits( prediction_bits),
    mean_state( 1 << ( prediction_bits - 1)),
    default_state( mean_state - 1),
    set_mask( ( size_in_entries / ways ) - 1),
    data( ways, vector<BPEntry>( size_in_entries / ways, BPEntry())),
    tags( size_in_entries,
          ways,
          1,
          branch_ip_size_in_bits)
{}


addr_t BP::getPC( addr_t PC) {
    unsigned int way;
    if ( tags.read( PC, &way)) // hit
    {
        BPEntry predict = data[ way][ getSetNum( branch_ip)];

        if ( predict.isTaken())  // predicted taken
            return predict.target();
    }

    return PC + 4;
}


void BP::update( Direction actual, addr_t target, addr_t branch_ip)
{
    unsigned int way;
    tags.write( branch_ip, &way);
    data[ way][ getSetNum( branch_ip)].update( actual, target);
}
