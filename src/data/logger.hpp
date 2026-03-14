// ============================================================
// logger.hpp — Lightweight timestamped logging interface.
// Part of: angry::data
//
// Declares shared logging utility used across modules:
//   * INFO/ERROR output channels
//   * Minimal "{}" placeholder formatting helper
//   * Header-only templated formatting entry points
//   * Backend log sink implemented in logger.cpp
// ============================================================

#pragma once
#include <sstream>
#include <string>
#include <utility>

namespace angry
{

// Provides tiny formatting-aware logging API without external
// dependencies; suitable for gameplay/runtime diagnostics.
class Logger
{
public:
    static void info( const std::string& message );
    static void error( const std::string& message );

    template <typename... Args>
    static void info( const std::string& format, Args&&... args )
    {
        log( "INFO", formatMessage( format, std::forward<Args>( args )... ) );
    }

    template <typename... Args>
    static void error( const std::string& format, Args&&... args )
    {
        log( "ERROR", formatMessage( format, std::forward<Args>( args )... ) );
    }

private:
    static void log( const std::string& level, const std::string& message );

    static std::string formatMessage( const std::string& format ) { return format; }

    template <typename T, typename... Args>
    static std::string formatMessage( const std::string& format, T&& value, Args&&... args )
    {
        std::string output = format;
        const std::size_t placeholderPos = output.find( "{}" );
        if ( placeholderPos == std::string::npos )
        {
            output += " [extra=" + toString( std::forward<T>( value ) ) + "]";
        }
        else
        {
            output.replace( placeholderPos, 2, toString( std::forward<T>( value ) ) );
        }

        return formatMessage( output, std::forward<Args>( args )... );
    }

    template <typename T>
    static std::string toString( T&& value )
    {
        std::ostringstream stream;
        stream << std::forward<T>( value );
        return stream.str();
    }
};

}  // namespace angry
