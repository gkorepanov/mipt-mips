/*
 * config.cpp - implementation of Config class
 * Copyright 2017 MIPT-MIPS
 */

/* Simulator modules */
#include "config.h"

/* Generic C++ */
#include <iostream>

/* boost - program option parsing */
#include <boost/program_options.hpp>

/* constructor and destructor */
Config::Config()
{

}

Config::~Config()
{

}


/* basic method */
int Config::handleArgs( int argc, char** argv)
{
    namespace po = boost::program_options;

    po::options_description description( "Allowed options");

    description.add_options()
        ( "binary,b",      po::value<std::string>( this->binary_filename.get_ptr())->required(),
          "input binary file")

        ( "numsteps,n",    po::value<cycles_t>( this->num_steps.get_ptr())->required(),
          "number of instructions to run")

        ( "btb-size,s",      po::value<unsigned int>( this->btb_size.get_ptr())->default_value( 128),
          "size of BTB cache in entries")

        ( "btb-ways,w",      po::value<unsigned int>( this->btb_ways.get_ptr())->default_value( 4),
          "number of ways in BTB cache (defines associativity)")

        /* by default, the silent mode is on */
        ( "disassembly,d",     po::bool_switch( this->disassembly_on.get_ptr())->default_value( false),
          "print disassembly")

        /* by default, functional simulator is run */
        ( "functional-only,f", po::bool_switch( this->functional_only.get_ptr())->default_value( false),
          "run functional simulation only")

        ( "help,h",
          "print this help message");

    po::positional_options_description posDescription;
    posDescription.add( "binary", 1).add( "numsteps", 2);
    po::variables_map vm;

    try
    {
        po::store(po::command_line_parser(argc, argv).
                                    options(description).
                                    positional(posDescription).
                                    run(),
                                    vm);


        /* parsing help */
        if ( vm.count( "help"))
        {
            std::cout << "Functional and performance simulators for MIPS-based CPU."
                      << std::endl << std::endl
                      << description << std::endl;
            std::exit( EXIT_SUCCESS);
        }

        /* calling notify AFTER parsing help, as otherwise
         * absent required args will cause errors
         */
        po::notify(vm);
    }
    catch ( const std::exception& e)
    {
        std::cerr << argv[0] << ": " << e.what()
                  << std::endl << std::endl
                  << description << std::endl;
        std::exit( EXIT_SUCCESS);
    }

    return 0;
}
