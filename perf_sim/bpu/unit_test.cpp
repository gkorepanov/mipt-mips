// generic C
#include <cassert>
#include <cstdlib>

// Google Test library
#include <gtest/gtest.h>

// MIPT-MIPS modules
#include "bpu.h"


TEST( Initialization, WrongParameters)
{
    // Check failing with wrong input values
    ASSERT_EXIT( BP( 100, 20, 2),  ::testing::ExitedWithCode( EXIT_FAILURE), "ERROR.*");
    ASSERT_EXIT( BP( 120, 20, 40), ::testing::ExitedWithCode( EXIT_FAILURE), "ERROR.*");
    ASSERT_EXIT( BP( 128, 14, 1),  ::testing::ExitedWithCode( EXIT_FAILURE), "ERROR.*");
}

TEST( HitAndMiss, Miss)
{
    BP bp( 128, 16, 2);
    addr_t PC = 12;

    // Check default cache miss behaviour
    ASSERT_EQ( bp.getPC( PC), PC + 4);
    PC = 16;
    ASSERT_EQ( bp.getPC( PC), PC + 4);
    PC = 20;
    ASSERT_EQ( bp.getPC( PC), PC + 4);
    PC = 12;
    ASSERT_EQ( bp.getPC( PC), PC + 4);
}

TEST( Main, PredictingBits)
{
    BP bp( 128, 16, 2);
    addr_t PC = 12;
    addr_t target = 28;

    // Teaching
    bp.update( true, PC, target);
    ASSERT_EQ( bp.getPC( PC), target);
    ASSERT_EQ( bp.getPC( PC), target);
    bp.update( true, PC, target);
    ASSERT_EQ( bp.getPC( PC), target);

    // "Over" - teaching
    bp.update( true, PC, target);
    ASSERT_EQ( bp.getPC( PC), target);
    bp.update( true, PC, target);
    ASSERT_EQ( bp.getPC( PC), target);

    // "Un" - teaching
    bp.update( false, PC);
    ASSERT_EQ( bp.getPC( PC), target);

    // Strong "un" - teaching

    bp.update( false, PC);
    bp.update( false, PC);
    bp.update( false, PC);
    ASSERT_EQ( bp.getPC( PC), PC + 4);

    bp.update( false, PC);
    ASSERT_EQ( bp.getPC( PC), PC + 4);

    bp.update( false, PC);
    ASSERT_EQ( bp.getPC( PC), PC + 4);

    bp.update( false, PC);
    ASSERT_EQ( bp.getPC( PC), PC + 4);

    // Teaching again
    bp.update( true, PC, target);
    ASSERT_EQ( bp.getPC( PC), PC + 4);

    bp.update( true, PC, target);
    ASSERT_EQ( bp.getPC( PC), target);

}


TEST( Overload, LRU)
{
    BP bp( 128, 16, 2);
    const addr_t PCconst = 16;
    addr_t target = 48;

    // Trying to make it forget the PCconst
    for ( int i = 0; i < 1000; i++)
    {
        bp.update( false, i);
        if ( !( i % 50))
            bp.update( true, PCconst, target);
    }

    addr_t PC = 4;
    ASSERT_EQ( bp.getPC( PC), PC + 4);
    ASSERT_EQ( bp.getPC( PCconst), target);
}


TEST( Multilevel, Level2)
{
    BP bp( 128, 16, 2, 2);
    addr_t PC = 16;
    addr_t target = 48;

    // Teaching
    bp.update( true, PC, target);
    bp.update( false, PC);
    bp.update( true, PC, target);
    bp.update( false, PC);
    bp.update( true, PC, target);
    bp.update( false, PC);
    bp.update( true, PC, target);
    bp.update( false, PC);
    bp.update( true, PC, target);
    bp.update( false, PC);
    bp.update( true, PC, target);
    bp.update( false, PC);

    // Checking sequnce
    ASSERT_EQ( bp.getPC( PC), target);
    bp.update( true, PC, target);
    ASSERT_EQ( bp.getPC( PC), PC + 4);

    // Tricking it
    bp.update( true, PC, target);

    // 11 is not studied yet, thus should return NT
    ASSERT_EQ( bp.getPC( PC), PC + 4);
}


int main( int argc, char* argv[])
{
    ::testing::InitGoogleTest( &argc, argv);
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    return RUN_ALL_TESTS();
}
