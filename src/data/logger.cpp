// ============================================================
// logger.cpp — Timestamped logging sink implementation.
// Part of: angry::data
//
// Implements process-wide logger behavior:
//   * Produces local timestamp for each log message
//   * Serializes concurrent writes via global mutex
//   * Routes INFO to clog and ERROR to cerr
//   * Supports Logger public API from logger.hpp
// ============================================================

#include "data/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace angry
{

// #=# Local Helpers #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

namespace
{

std::mutex gLogMutex;

std::string currentTimeStamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t( now );
    std::tm localTime{};
#if defined( _WIN32 )
    localtime_s( &localTime, &nowTime );
#else
    localtime_r( &nowTime, &localTime );
#endif

    std::ostringstream out;
    out << std::put_time( &localTime, "%Y-%m-%d %H:%M:%S" );
    return out.str();
}

}  // namespace

// #=# Public API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

void Logger::info( const std::string& message )
{
    log( "INFO", message );
}

void Logger::error( const std::string& message )
{
    log( "ERROR", message );
}

void Logger::log( const std::string& level, const std::string& message )
{
    std::lock_guard<std::mutex> guard( gLogMutex );

    std::ostream& output = ( level == "ERROR" ) ? std::cerr : std::clog;
    output << "[" << currentTimeStamp() << "] [" << level << "] " << message << "\n";
}

}  // namespace angry
