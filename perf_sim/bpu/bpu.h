/*
 * bpu.h - the branch prediction unit for MIPS
 * @author George Korepanov <gkorepanov.gk@gmail.com>
 * Copyright 2017 MIPT-MIPS
 */

// MIPT_MIPS modules
#include "perf_sim/mem/cache_tag_array.h"

/* The branch prediction unit keeps the fixed amount of entries.
 * Entry contains the information regarding the certain branch ip:
 * the target adress, the state.
 *
 * The number of ways in fact determines the associativity of the
 * cache.
 */

/*
 * In BTB, each branch_ip refers to BPEntry, which size may vary
 * depending on number of bits storing the state (e.g. WEAKLY NOT TAKEN)
 * and the size of target address.
 *
 * However, the CacheTagArray is designed as a data cache, i.e.
 * it implies that each address coresponds to one byte in memory.
 * Thus we use CacheTagArray with "strange" paramaters to be used as
 * BTB. In particular, block size is 1 byte (block "contains only one
 * memory byte"), and the size in bytes equals to that of in entries.
 */



/* for the sake of semantics */
typedef addr_t uint64;

/* plain bool rename for conviniency */
enum Direction {
    NOT_TAKEN = 0,
    TAKEN = 1
};


class BP {
private:
    /* the amount of bits used to store the state,
     * for instance, the bimodal BTB uses only 2 bits.
     * Thus, maximum value of `state` var is (2^prediction_bits - 1).
     */
    const unsigned short prediction_bits;

    /* state >= mean_state -- TAKEN
     * state  < mean_state -- NOT_TAKEN
     */
    const unsigned short mean_state;

    /* default state is the most weak NOT_TAKEN
     * (as we do not know the target),
     * default_state = mean - 1
     */
    const unsigned short default_state;

    class BPEntry {
    private:
        unsigned short state;
        addr_t target;
    public:
        BPEntry() :
            state( default_state)
        {}

        /* prediction */
        Direction getDirection() const
        {
            return state & mean_state;
        }

        addr_t    getTarget()    const
        {
            return target;
        }


        /* update */
        void update( Direction actual, addr_t target);
    };


private:
    CacheTagArray tags;
    std::vector<std::vector<BPEntry>> data;

public:
    BP( unsigned int   size_in_entries,
        unsigned int   ways,
        unsigned short branch_ip_size_in_bits = 32);

    addr_t getPC( addr_t PC);
    void update( Direction actual, addr_t target, addr_t branch_ip);
};



BP::BP( unsigned int   size_in_entries,
        unsigned int   ways,
        unsigned short prediction_bits,
        unsigned short branch_ip_size_in_bits = 32) :
    prediction_bits( prediction_bits),
    mean_state( 1 << ( prediction_bits - 1)),
    default_state( mean_state - 1),
    tags( size_in_entries,
          ways,
          1,
          branch_ip_size_in_bits),

    /* initializing data array with default values */
    data( size_in_entries / ways,
          vector<BPEntry>( ways, BPEntry()))
{}
