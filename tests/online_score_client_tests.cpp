// ============================================================
// online_score_client_tests.cpp — OnlineScoreClient tests.
// Part of: angry::tests
//
// Verifies leaderboard/score client behavior:
//   * Score submission success/failure handling
//   * Retry behavior on transient failures
//   * Leaderboard JSON parsing and status mapping
//   * Token/non-token request path expectations
// ============================================================

#include "data/online_score_client.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <stdexcept>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

// #=# Test Helpers & Mock Server #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

bool can_bind_loopback_socket()
{
    const int fd = ::socket( AF_INET, SOCK_STREAM, 0 );
    if ( fd < 0 )
    {
        return false;
    }

    int reuse = 1;
    (void)::setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
    addr.sin_port = 0;
    const bool ok = ::bind( fd, reinterpret_cast<const sockaddr*>( &addr ), sizeof( addr ) ) == 0;
    ::close( fd );
    return ok;
}

class LocalMockHttpServer
{
public:
    struct ScriptedResponse
    {
        int statusCode = 200;
        std::string body = "{}";
    };

    explicit LocalMockHttpServer( std::vector<ScriptedResponse> scriptedResponses )
        : responses_( std::move( scriptedResponses ) )
    {
        listenFd_ = ::socket( AF_INET, SOCK_STREAM, 0 );
        if ( listenFd_ < 0 )
        {
            throw std::runtime_error( "socket() failed" );
        }

        int reuse = 1;
        (void)::setsockopt( listenFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof( reuse ) );

        sockaddr_in addr {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
        addr.sin_port = 0;
        if ( ::bind( listenFd_, reinterpret_cast<const sockaddr*>( &addr ), sizeof( addr ) ) != 0 )
        {
            throw std::runtime_error( "bind() failed" );
        }
        if ( ::listen( listenFd_, 8 ) != 0 )
        {
            throw std::runtime_error( "listen() failed" );
        }

        sockaddr_in boundAddr {};
        socklen_t boundLen = sizeof( boundAddr );
        if ( ::getsockname( listenFd_, reinterpret_cast<sockaddr*>( &boundAddr ), &boundLen ) != 0 )
        {
            throw std::runtime_error( "getsockname() failed" );
        }
        port_ = ntohs( boundAddr.sin_port );

        running_.store( true );
        worker_ = std::thread( [this]() { this->serve_loop(); } );
    }

    ~LocalMockHttpServer()
    {
        stop();
    }

    LocalMockHttpServer( const LocalMockHttpServer& ) = delete;
    LocalMockHttpServer& operator=( const LocalMockHttpServer& ) = delete;

    int port() const
    {
        return port_;
    }

    std::string base_url() const
    {
        return "http://127.0.0.1:" + std::to_string( port_ );
    }

    int request_count() const
    {
        return requestCount_.load();
    }

    std::vector<std::string> request_targets() const
    {
        std::lock_guard<std::mutex> lock( requestsMutex_ );
        return requestTargets_;
    }

    std::vector<std::string> raw_requests() const
    {
        std::lock_guard<std::mutex> lock( requestsMutex_ );
        return rawRequests_;
    }

private:
    void stop()
    {
        const bool wasRunning = running_.exchange( false );
        if ( wasRunning && listenFd_ >= 0 )
        {
            ::shutdown( listenFd_, SHUT_RDWR );
            ::close( listenFd_ );
            listenFd_ = -1;
        }

        if ( worker_.joinable() )
        {
            worker_.join();
        }
    }

    static const char* reason_phrase( int statusCode )
    {
        if ( statusCode >= 200 && statusCode < 300 )
            return "OK";
        if ( statusCode >= 400 && statusCode < 500 )
            return "Client Error";
        if ( statusCode >= 500 && statusCode < 600 )
            return "Server Error";
        return "Status";
    }

    void serve_loop()
    {
        while ( running_.load() )
        {
            sockaddr_in clientAddr {};
            socklen_t clientLen = sizeof( clientAddr );
            const int clientFd = ::accept(
                listenFd_, reinterpret_cast<sockaddr*>( &clientAddr ), &clientLen );
            if ( clientFd < 0 )
            {
                if ( running_.load() )
                {
                    continue;
                }
                break;
            }

            char buffer[4096];
            const ssize_t readBytes = ::recv( clientFd, buffer, sizeof( buffer ) - 1, 0 );
            if ( readBytes > 0 )
            {
                buffer[readBytes] = '\0';
                const std::string req( buffer );
                const std::size_t firstSpace = req.find( ' ' );
                const std::size_t secondSpace =
                    ( firstSpace == std::string::npos ) ? std::string::npos : req.find( ' ', firstSpace + 1 );
                if ( firstSpace != std::string::npos && secondSpace != std::string::npos )
                {
                    std::lock_guard<std::mutex> lock( requestsMutex_ );
                    requestTargets_.push_back(
                        req.substr( firstSpace + 1, secondSpace - firstSpace - 1 ) );
                    rawRequests_.push_back( req );
                }
            }

            const int idx = requestCount_.fetch_add( 1 );
            const ScriptedResponse& scripted =
                responses_[static_cast<std::size_t>( std::min( idx, static_cast<int>( responses_.size() - 1 ) ) )];

            const std::string payload = scripted.body;
            const std::string headers =
                "HTTP/1.1 " + std::to_string( scripted.statusCode ) + " " + reason_phrase( scripted.statusCode ) + "\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: " + std::to_string( payload.size() ) + "\r\n"
                "Connection: close\r\n\r\n";

            const std::string response = headers + payload;
            (void)::send( clientFd, response.data(), response.size(), 0 );
            ::shutdown( clientFd, SHUT_RDWR );
            ::close( clientFd );
        }
    }

    std::vector<ScriptedResponse> responses_;
    int listenFd_ = -1;
    int port_ = -1;
    std::thread worker_;
    std::atomic<bool> running_ {false};
    std::atomic<int> requestCount_ {0};

    mutable std::mutex requestsMutex_;
    std::vector<std::string> requestTargets_;
    std::vector<std::string> rawRequests_;
};

}  // namespace

// #=# Test Cases #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

TEST( OnlineScoreClient, SubmitScoreReturnsTrueOn2xx )
{
    if ( !can_bind_loopback_socket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server( {LocalMockHttpServer::ScriptedResponse{201, R"({"ok":true})"}} );
    angry::OnlineScoreClient client( server.base_url() );

    const bool ok = client.submit_score( "Alice", 7, 1234, 3 );
    EXPECT_TRUE( ok );
    EXPECT_EQ( server.request_count(), 1 );

    const auto targets = server.request_targets();
    ASSERT_FALSE( targets.empty() );
    EXPECT_NE( targets.front().find( "/scores" ), std::string::npos );
}

TEST( OnlineScoreClient, FetchLeaderboardParsesValidArray )
{
    if ( !can_bind_loopback_socket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server(
        {LocalMockHttpServer::ScriptedResponse{
            200,
            R"([{"playerName":"Alice","score":1200,"stars":3},{"playerName":"Bob","score":900,"stars":2}])"}} );
    angry::OnlineScoreClient client( server.base_url() );

    const auto entries = client.fetch_leaderboard( 3 );
    ASSERT_EQ( entries.size(), 2u );
    EXPECT_EQ( entries[0].playerName, "Alice" );
    EXPECT_EQ( entries[0].score, 1200 );
    EXPECT_EQ( entries[0].stars, 3 );
    EXPECT_EQ( entries[1].playerName, "Bob" );
    EXPECT_EQ( entries[1].score, 900 );
    EXPECT_EQ( entries[1].stars, 2 );

    const auto targets = server.request_targets();
    ASSERT_FALSE( targets.empty() );
    EXPECT_NE( targets.front().find( "/leaderboard?levelId=3" ), std::string::npos );
}

TEST( OnlineScoreClient, FetchLeaderboardReturnsEmptyOnInvalidJson )
{
    if ( !can_bind_loopback_socket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server( {LocalMockHttpServer::ScriptedResponse{200, "not-a-json"}} );
    angry::OnlineScoreClient client( server.base_url() );

    const auto entries = client.fetch_leaderboard( 2 );
    EXPECT_TRUE( entries.empty() );
    EXPECT_EQ( server.request_count(), 1 );
}

TEST( OnlineScoreClient, SubmitScoreRetriesOn5xxAndFails )
{
    if ( !can_bind_loopback_socket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server( {
        LocalMockHttpServer::ScriptedResponse{503, R"({"error":"temporary"})"},
        LocalMockHttpServer::ScriptedResponse{503, R"({"error":"temporary"})"},
        LocalMockHttpServer::ScriptedResponse{503, R"({"error":"temporary"})"},
    } );
    angry::OnlineScoreClient client( server.base_url() );

    const bool ok = client.submit_score( "Retry", 1, 10, 1 );
    EXPECT_FALSE( ok );
    EXPECT_EQ( server.request_count(), 3 );
}

TEST( OnlineScoreClient, FetchLeaderboardRetriesOn5xxAndFails )
{
    if ( !can_bind_loopback_socket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server( {
        LocalMockHttpServer::ScriptedResponse{500, R"({"error":"temporary"})"},
        LocalMockHttpServer::ScriptedResponse{500, R"({"error":"temporary"})"},
        LocalMockHttpServer::ScriptedResponse{500, R"({"error":"temporary"})"},
    } );
    angry::OnlineScoreClient client( server.base_url() );

    const auto entries = client.fetch_leaderboard( 5 );
    EXPECT_TRUE( entries.empty() );
    EXPECT_EQ( server.request_count(), 3 );
}

TEST( OnlineScoreClient, SubmitScoreReturnsFalseOnConnectionFailure )
{
    // Port 1 is expected to be closed in normal user environments; this should fail fast.
    angry::OnlineScoreClient client( "http://127.0.0.1:1" );
    const bool ok = client.submit_score( "NoServer", 1, 100, 1 );
    EXPECT_FALSE( ok );
}

TEST( OnlineScoreClient, SubmitScoreWithTokenSendsBearerHeader )
{
    if ( !can_bind_loopback_socket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server( {LocalMockHttpServer::ScriptedResponse{201, R"({"ok":true})"}} );
    angry::OnlineScoreClient client( server.base_url() );

    const bool ok = client.submit_score_with_token( "jwt-token-xyz", 7, 4000, 2 );
    EXPECT_TRUE( ok );
    EXPECT_EQ( server.request_count(), 1 );

    const auto targets = server.request_targets();
    ASSERT_FALSE( targets.empty() );
    EXPECT_NE( targets.front().find( "/scores" ), std::string::npos );

    const auto raws = server.raw_requests();
    ASSERT_FALSE( raws.empty() );
    EXPECT_NE( raws.front().find( "Authorization: Bearer jwt-token-xyz" ), std::string::npos );
}

TEST( OnlineScoreClient, SubmitScoreWithTokenReturnsFalseOnEmptyToken )
{
    angry::OnlineScoreClient client( "http://127.0.0.1:1" );
    const bool ok = client.submit_score_with_token( "", 1, 100, 1 );
    EXPECT_FALSE( ok );
}
