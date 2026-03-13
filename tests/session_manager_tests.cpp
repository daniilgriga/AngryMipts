#include "data/session_manager.hpp"

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
           / ( "angry_mipts_session_test_" + std::to_string( now ) + ".json" );
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

    const std::string pathString() const
    {
        return path_.string();
    }

    const std::filesystem::path& path() const
    {
        return path_;
    }

private:
    std::filesystem::path path_;
};

}  // namespace

TEST( SessionManager, SaveThenLoadRestoresSession )
{
    TempSessionFile tempFile;

    angry::SessionManager writer( tempFile.pathString() );
    writer.setSession( "token-abc", "alex" );
    writer.saveSession();

    angry::SessionManager reader( tempFile.pathString() );
    reader.loadSession();

    EXPECT_TRUE( reader.isLoggedIn() );
    EXPECT_EQ( reader.token(), "token-abc" );
    EXPECT_EQ( reader.username(), "alex" );
}

TEST( SessionManager, MissingFileMeansEmptySession )
{
    TempSessionFile tempFile;

    angry::SessionManager sm( tempFile.pathString() );
    sm.loadSession();

    EXPECT_FALSE( sm.isLoggedIn() );
    EXPECT_TRUE( sm.token().empty() );
    EXPECT_TRUE( sm.username().empty() );
}

TEST( SessionManager, BrokenJsonResultsInEmptySession )
{
    TempSessionFile tempFile;
    {
        std::ofstream out( tempFile.path() );
        out << "{ not valid json";
    }

    angry::SessionManager sm( tempFile.pathString() );
    sm.loadSession();

    EXPECT_FALSE( sm.isLoggedIn() );
    EXPECT_TRUE( sm.token().empty() );
    EXPECT_TRUE( sm.username().empty() );
}

TEST( SessionManager, ClearSessionRemovesFileAndState )
{
    TempSessionFile tempFile;

    angry::SessionManager sm( tempFile.pathString() );
    sm.setSession( "token-xyz", "mipt" );
    sm.saveSession();

    ASSERT_TRUE( std::filesystem::exists( tempFile.path() ) );
    ASSERT_TRUE( sm.isLoggedIn() );

    sm.clearSession();

    EXPECT_FALSE( std::filesystem::exists( tempFile.path() ) );
    EXPECT_FALSE( sm.isLoggedIn() );
    EXPECT_TRUE( sm.token().empty() );
    EXPECT_TRUE( sm.username().empty() );
}

