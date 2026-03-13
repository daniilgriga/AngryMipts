#include "data/account_service.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{

std::filesystem::path makeTempSessionPath()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path()
           / ( "angry_mipts_account_service_" + std::to_string( now ) + ".json" );
}

class TempSessionFile
{
public:
    TempSessionFile()
        : path_( makeTempSessionPath() )
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

    std::string pathString() const
    {
        return path_.string();
    }

private:
    std::filesystem::path path_;
};

}  // namespace

TEST( AccountService, LoadSessionExposesLoggedInState )
{
    TempSessionFile temp;
    {
        std::ofstream out( temp.path() );
        out << R"({"token":"jwt-token","username":"alex"})";
    }

    angry::AccountService service( temp.pathString(), "http://127.0.0.1:1" );
    service.loadSession();

    EXPECT_TRUE( service.isLoggedIn() );
    EXPECT_EQ( service.token(), "jwt-token" );
    EXPECT_EQ( service.username(), "alex" );
}

TEST( AccountService, LogoutClearsSessionFileAndState )
{
    TempSessionFile temp;
    {
        std::ofstream out( temp.path() );
        out << R"({"token":"jwt-token","username":"alex"})";
    }

    angry::AccountService service( temp.pathString(), "http://127.0.0.1:1" );
    service.loadSession();
    ASSERT_TRUE( service.isLoggedIn() );

    service.logout();

    EXPECT_FALSE( std::filesystem::exists( temp.path() ) );
    EXPECT_FALSE( service.isLoggedIn() );
    EXPECT_TRUE( service.token().empty() );
    EXPECT_TRUE( service.username().empty() );
}

TEST( AccountService, SubmitScoreWithoutLoginSkipsOnlineSubmission )
{
    TempSessionFile temp;
    angry::AccountService service( temp.pathString(), "http://127.0.0.1:1" );
    service.loadSession();
    ASSERT_FALSE( service.isLoggedIn() );

    const bool ok = service.submitScoreIfLoggedIn( 1, 123, 1 );
    EXPECT_FALSE( ok );
}

TEST( AccountService, LoginServerUnavailableDoesNotPersistSession )
{
    TempSessionFile temp;
    angry::AccountService service( temp.pathString(), "http://127.0.0.1:1" );

    const angry::AuthResult result = service.login( "alex", "123456" );
    EXPECT_FALSE( result.success );

    service.loadSession();
    EXPECT_FALSE( service.isLoggedIn() );
    EXPECT_FALSE( std::filesystem::exists( temp.path() ) );
}

