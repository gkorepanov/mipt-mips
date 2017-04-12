/*
 * bpu.h - the branch prediction unit for MIPS
 * @author George Korepanov <gkorepanov.gk@gmail.com>
 * Copyright 2017 MIPT-MIPS
 */

// C++ generic modules
#include <vector>

// MIPT_MIPS modules
#include "perf_sim/mem/cache_tag_array.h"

/* The branch prediction unit keeps the fixed amount of entries.
 * Entry contains the information regarding the certain branch ip:
 * the target address, the state.
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
 * it implies that each address corresponds to one byte in memory.
 * Thus we use CacheTagArray with "strange" parameters
 * to make it usable as BTB.
 * In particular, block size is 1 byte (block "contains only one
 * memory byte"), and the size in bytes equals to that of in entries.
 */



/* for the sake of semantics */
typedef uint64 addr_t;

class BP {
private:
    /* the amount of bits used to store the state,
     * for instance, the bimodal BTB uses only 2 bits.
     * Thus, maximum value of `state` var is
     * STATE_MAX = (2^prediction_bits - 1).
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
        BP& bp;
        unsigned short state;
        addr_t _target;
    public:
        BPEntry( BP& bp) :
            bp( bp),
            state( bp.default_state)
        {}

        /* prediction */
        bool isTaken() const
        {
            return state & bp.mean_state;
        }

        addr_t target() const
        {
            return _target;
        }


        /* update */
        void update( bool is_actually_taken, addr_t target);
    };


private:
    addr_t set_mask;

    std::vector<std::vector<BPEntry>> data;
    CacheTagArray tags;

    inline unsigned int getSetNum( addr_t addr)
    {
        return addr & set_mask;
    }

public:
    BP( unsigned int   size_in_entries,
        unsigned int   ways,
        unsigned short prediction_bits,
        unsigned short branch_ip_size_in_bits = 32);

    addr_t getPC( addr_t PC);
    void update( bool is_actually_taken, addr_t branch_ip, addr_t target = 0);
};
