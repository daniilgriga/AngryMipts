// ============================================================
// account_service_tests.cpp — AccountService unit tests.
// Part of: angry::tests
//
// Covers account facade behavior:
//   * Session bootstrap from persisted file
//   * Logout side effects on file and in-memory state
//   * Auth-failure invariants for token persistence
//   * Network-independent service expectations
// ============================================================

#include "data/account_service.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

// #=# Test Helpers #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

std::filesystem::path make_temp_session_path()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
           / ( "angry_mipts_account_service_" + std::to_string( now ) + ".json" );
}

class TempSessionFile
{
public:
    TempSessionFile()
        : path_( make_temp_session_path() )
    {
    }

    ~TempSessionFile()
    {
        std::error_code ec;
        std::filesystem::remove( path_, ec );
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

    std::string path_string() const
    {
        return path_.string();
    }

private:
    std::filesystem::path path_;
};

}  // namespace

// #=# Test Cases #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

TEST( AccountService, LoadSessionExposesLoggedInState )
{
    TempSessionFile temp_file;
    {
        std::ofstream out( temp_file.path() );
        out << R"({"token":"jwt-token","username":"alex"})";
    }

    angry::AccountService service( temp_file.path_string(), "http://127.0.0.1:1" );
    service.load_session();

    EXPECT_TRUE( service.is_logged_in() );
    EXPECT_EQ( service.token(), "jwt-token" );
    EXPECT_EQ( service.username(), "alex" );
}

TEST( AccountService, LogoutClearsSessionFileAndState )
{
    TempSessionFile temp_file;
    {
        std::ofstream out( temp_file.path() );
        out << R"({"token":"jwt-token","username":"alex"})";
    }

    angry::AccountService service( temp_file.path_string(), "http://127.0.0.1:1" );
    service.load_session();
    ASSERT_TRUE( service.is_logged_in() );

    service.logout();

    EXPECT_FALSE( std::filesystem::exists( temp_file.path() ) );
    EXPECT_FALSE( service.is_logged_in() );
    EXPECT_TRUE( service.token().empty() );
    EXPECT_TRUE( service.username().empty() );
}

TEST( AccountService, SubmitScoreWithoutLoginSkipsOnlineSubmission )
{
    TempSessionFile temp_file;
    angry::AccountService service( temp_file.path_string(), "http://127.0.0.1:1" );
    service.load_session();
    ASSERT_FALSE( service.is_logged_in() );

    const bool ok = service.submit_score_if_logged_in( 1, 123, 1 );
    EXPECT_FALSE( ok );
}

TEST( AccountService, LoginServerUnavailableDoesNotPersistSession )
{
    TempSessionFile temp_file;
    angry::AccountService service( temp_file.path_string(), "http://127.0.0.1:1" );

    const angry::AuthResult result = service.login( "alex", "123456" );
    EXPECT_FALSE( result.success );

    service.load_session();
    EXPECT_FALSE( service.is_logged_in() );
    EXPECT_FALSE( std::filesystem::exists( temp_file.path() ) );
}
