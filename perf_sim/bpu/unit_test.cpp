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

    // Check default cache miss behaviour
    addr_t PC = 12;
    ASSERT_EQ( bp.predictTaken(PC), 0);

    PC = 16;
    ASSERT_EQ( bp.predictTaken(PC), 0);

    PC = 20;
    ASSERT_EQ( bp.predictTaken(PC), 0);

    PC = 12;
    ASSERT_EQ( bp.predictTaken(PC), 0);
}

TEST( Main, PredictingBits)
{
    BP bp( 128, 16, 2);
    addr_t PC = 12;
    addr_t target = 28;

    // Teaching
    bp.update( true, PC, target);
    ASSERT_EQ( bp.predictTaken(PC), 1);
    ASSERT_EQ( bp.getTarget(PC), target);

    bp.update( true, PC, target);
    ASSERT_EQ( bp.predictTaken(PC), 1);
    ASSERT_EQ( bp.getTarget(PC), target);

    // "Over" - teaching
    bp.update( true, PC, target);
    ASSERT_EQ( bp.predictTaken(PC), 1);
    ASSERT_EQ( bp.getTarget(PC), target);

    bp.update( true, PC, target);
    ASSERT_EQ( bp.predictTaken(PC), 1);
    ASSERT_EQ( bp.getTarget(PC), target);

    // "Un" - teaching
    bp.update( false, PC);
    ASSERT_EQ( bp.predictTaken(PC), 1);
    ASSERT_EQ( bp.getTarget(PC), target);

    // Strong "un" - teaching
    bp.update( false, PC);
    bp.update( false, PC);
    bp.update( false, PC);
    ASSERT_EQ( bp.predictTaken(PC), 0);

    bp.update( false, PC);
    ASSERT_EQ( bp.predictTaken(PC), 0);

    bp.update( false, PC);
    ASSERT_EQ( bp.predictTaken(PC), 0);

    bp.update( false, PC);
    ASSERT_EQ( bp.predictTaken(PC), 0);

    // Teaching again
    bp.update( true, PC, target);
    ASSERT_EQ( bp.predictTaken(PC), 0);

    bp.update( true, PC, target);
    ASSERT_EQ( bp.predictTaken(PC), 1);
    ASSERT_EQ( bp.getTarget(PC), target);
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

    // Checking some random PC and PCConst
    addr_t PC = 4;
    ASSERT_EQ( bp.predictTaken(PC), 0);
    ASSERT_EQ( bp.predictTaken(PCconst), 1);
    ASSERT_EQ( bp.getTarget(PCconst), target);
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
    ASSERT_EQ( bp.predictTaken(PC), 1);
    ASSERT_EQ( bp.getTarget(PC), target);

    bp.update( true, PC, target);
    ASSERT_EQ( bp.predictTaken(PC), 0);


    // Tricking it
    bp.update( true, PC, target);

    // 11 is not studied yet, thus should return NT
    ASSERT_EQ( bp.predictTaken(PC), 0);

}


int main( int argc, char* argv[])
{
    ::testing::InitGoogleTest( &argc, argv);
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";
    return RUN_ALL_TESTS();
}
