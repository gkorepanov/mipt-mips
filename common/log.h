/**
 * log.h - Header of log class
 * includes 2 methods to show warrnings and errors
 * @author Pavel Kryukov
 * Copyright 2017 MIPT-MIPS team
 */

#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <ostream>


#define RED          "\033[31m"
#define GREEN        "\033[32m"
#define DCOLOR       "\033[39m"

class LogOstream
{
    const bool enable;
    std::ostream& stream;

public:
    struct Critical { };

    LogOstream(bool value, std::ostream& _out) : enable(value), stream(_out) { }

    friend LogOstream& operator<<(LogOstream&, const Critical&) {
         exit( EXIT_FAILURE);
    }

    LogOstream& operator<<(std::ostream& (*F)(std::ostream&)) {
        if ( enable)
            F(stream);
        return *this;
    }

    template<typename T>
    LogOstream& operator<<(const T& v) {
        if ( enable) {
            stream << v;
        }

        return *this;
    }
};

class Log
{
public:
    LogOstream sout;
    LogOstream serr;
    LogOstream::Critical critical;
    std::string separator;

    Log( bool value) :
        sout( value, std::cout),
        serr( true, std::cerr),
        separator( std::string( "\n") + std::string( 80, '*') + std::string( "\n"))
    {}
};


#endif /* LOG_H */
