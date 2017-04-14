/**
 * cache_tag_array.cpp
 * Implementation of the cache tag array model.
 * MIPT-MIPS Assignment 5.
 * Ladin Oleg.
 */

/* C libraries. */
#include <cstdlib>

/* C++ libraries. */
#include <iostream>

/* Simulator modules. */
#include "cache_tag_array.h"

CacheTagArray::CacheTagArray( unsigned int size_in_bytes,
                              unsigned int ways,
                              unsigned short block_size_in_bytes,
                              unsigned short addr_size_in_bits) :
                              Log(false),                       //cache should not say anything but error info
                              size_in_bytes( size_in_bytes),
                              ways( ways),
                              block_size_in_bytes( block_size_in_bytes),
                              addr_size_in_bits( addr_size_in_bits)
{
    /* Check is it possiable to create the cache. */
    checkArgs( size_in_bytes, ways, block_size_in_bytes, addr_size_in_bits);
    /* Allocate memory for cache sets and LRU module. */
    set = new CacheSet* [ ways];
    for ( unsigned int i = 0; i < ways; ++i)
    {
        set[ i] = new CacheSet [ size_in_bytes / ( ways * block_size_in_bytes)];
    }
    lru = new LRUInfo( ways, size_in_bytes / ( ways * block_size_in_bytes));
}

CacheTagArray::~CacheTagArray()
{
    /* Free memory used by cache sets and LRU module. */
    for ( unsigned int i = 0; i < ways; ++i)
    {
        delete [] set[ i];
    }
    delete [] set;
    delete lru;
}

bool CacheTagArray::read( uint64 addr, unsigned int* way)
{
    unsigned int set_num = getSetNum( addr);
    uint64 tag_num = getTagNum( addr);
    for ( unsigned int i = 0; i < ways; ++i) // search into each way
    {
        if ( set[ i][ set_num].line == tag_num) // check tag
        {
            if ( set[ i][ set_num].is_valid) // check validaty
            {
                lru->update( set_num, i); // update LRU info
                if ( way != nullptr)
                    *way = i;

                return true; // hit
            }
            return false; // miss, dirty data (or empty)
        }
    }
    return false; // miss (no data)
}

void CacheTagArray::write( uint64 addr, unsigned int* way)
{
    unsigned int set_num = getSetNum( addr);
    unsigned int way_num = lru->update( set_num); // get l.r.u. way
    if ( way != nullptr)
        *way = way_num;

    set[ way_num][ set_num].line = getTagNum( addr); // write it
    set[ way_num][ set_num].is_valid = true; // this set is valid now
}

void CacheTagArray::checkArgs( unsigned int size_in_bytes,
                               unsigned int ways,
                               unsigned short block_size_in_bytes,
                               unsigned short addr_size_in_bits)
{
    /* All args are not less than zero because of "unsigned" keyword. */
    if ( ( size_in_bytes == 0) ||
         ( ways == 0) ||
         ( block_size_in_bytes == 0) ||
         ( addr_size_in_bits == 0))
    {
        serr << "ERROR: Wrong arguments! All arguments should be greater "
                  << "than zero." << critical;
    }
    /*
     * It also checks "size_in_bytes < block_size_in_bytes" and "size_in_bytes
     * < ways".
     */
    if ( size_in_bytes / ways < block_size_in_bytes)
    {
        serr << "ERROR: Wrong arguments! Size of each way should be not "
                  << "less than size of block (size in bytes of cache should "
                  << "be not less than number of ways and size of block in "
                  << "bytes)." << critical;
    }
    /*
     * It also checks "size_in_bytes % block_size_in_bytes != 0" and
     * "size_in_bytes % ways != 0".
     */
    if ( ( size_in_bytes % ( block_size_in_bytes * ways)) != 0)
    {
        serr << "ERROR: Wrong arguments! Size of cache should be a "
                  << "multiple of block size in bytes and number of ways."
                  << critical;
    }
    /* The next two use: "2^a=b"<=>"b=100...000[2]"<=>"(b&(b-1))=0". */
    if ( ( ( size_in_bytes / ( ways * block_size_in_bytes)) &
           ( size_in_bytes / ( ways * block_size_in_bytes) - 1)) != 0)
    {
        serr << "ERROR: Wrong arguments! Number of sets should be a power"
                  << " of 2." << critical;
    }
    if ( ( block_size_in_bytes & ( block_size_in_bytes - 1)) != 0)
    {
        serr << "ERROR: Wrong arguments! Block size should be a power of "
                  << "2." << critical;
    }
}

unsigned int CacheTagArray::getSetNum( uint64 addr)
{
    /* Cut "logbin(block_size_in_bytes)" bits from the end. */
    int set_num = addr / block_size_in_bytes;
    /* Get bits that are responsible for set number. */
    set_num &= ( ( size_in_bytes / ( ways * block_size_in_bytes)) - 1);
    return set_num;
}

uint64 CacheTagArray::getTagNum( uint64 addr)
{
    /* Cut "logbin(block_size_in_bytes)" bits from the end. */
    return ( addr / block_size_in_bytes);
}
