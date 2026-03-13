#include "data/auth_client.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{

bool canBindLoopbackSocket()
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
        worker_ = std::thread( [this]() { serveLoop(); } );
    }

    ~LocalMockHttpServer()
    {
        stop();
    }

    std::string baseUrl() const
    {
        return "http://127.0.0.1:" + std::to_string( port_ );
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

    static const char* reasonPhrase( int statusCode )
    {
        if ( statusCode >= 200 && statusCode < 300 )
            return "OK";
        if ( statusCode >= 400 && statusCode < 500 )
            return "Client Error";
        if ( statusCode >= 500 && statusCode < 600 )
            return "Server Error";
        return "Status";
    }

    void serveLoop()
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
            (void)::recv( clientFd, buffer, sizeof( buffer ) - 1, 0 );

            const int idx = requestCount_.fetch_add( 1 );
            const ScriptedResponse& scripted =
                responses_[static_cast<std::size_t>(
                    std::min( idx, static_cast<int>( responses_.size() - 1 ) ) )];

            const std::string headers =
                "HTTP/1.1 " + std::to_string( scripted.statusCode ) + " "
                + reasonPhrase( scripted.statusCode ) + "\r\n"
                + "Content-Type: application/json\r\n"
                + "Content-Length: " + std::to_string( scripted.body.size() ) + "\r\n"
                + "Connection: close\r\n\r\n";
            const std::string response = headers + scripted.body;

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
};

}  // namespace

TEST( AuthClient, RegisterSuccessReturnsPositiveResult )
{
    if ( !canBindLoopbackSocket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server(
        {LocalMockHttpServer::ScriptedResponse{201, R"({"ok":true})"}} );
    angry::AuthClient client( server.baseUrl() );

    const angry::AuthResult result = client.registerUser( "alex", "123456" );
    EXPECT_TRUE( result.success );
    EXPECT_EQ( result.username, "alex" );
    EXPECT_TRUE( result.errorMessage.empty() );
}

TEST( AuthClient, LoginSuccessReturnsTokenAndUsername )
{
    if ( !canBindLoopbackSocket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server(
        {LocalMockHttpServer::ScriptedResponse{
            200, R"({"token":"jwt-token-123","username":"alex"})"}} );
    angry::AuthClient client( server.baseUrl() );

    const angry::AuthResult result = client.loginUser( "alex", "123456" );
    EXPECT_TRUE( result.success );
    EXPECT_EQ( result.token, "jwt-token-123" );
    EXPECT_EQ( result.username, "alex" );
    EXPECT_TRUE( result.errorMessage.empty() );
}

TEST( AuthClient, LoginWrongPasswordFailsGracefully )
{
    if ( !canBindLoopbackSocket() )
    {
        GTEST_SKIP() << "Loopback bind is unavailable in this environment.";
    }

    LocalMockHttpServer server(
        {LocalMockHttpServer::ScriptedResponse{
            401, R"({"error":"invalid username or password"})"}} );
    angry::AuthClient client( server.baseUrl() );

    const angry::AuthResult result = client.loginUser( "alex", "bad-pass" );
    EXPECT_FALSE( result.success );
    EXPECT_TRUE( result.token.empty() );
    EXPECT_TRUE( result.username.empty() );
    EXPECT_FALSE( result.errorMessage.empty() );
}

TEST( AuthClient, LoginServerUnavailableFailsGracefully )
{
    angry::AuthClient client( "http://127.0.0.1:1" );
    const angry::AuthResult result = client.loginUser( "alex", "123456" );
    EXPECT_FALSE( result.success );
    EXPECT_TRUE( result.token.empty() );
    EXPECT_TRUE( result.username.empty() );
    EXPECT_FALSE( result.errorMessage.empty() );
}

