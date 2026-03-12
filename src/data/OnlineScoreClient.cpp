#include "OnlineScoreClient.hpp"

#include "logger.hpp"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace angry
{

using json = nlohmann::json;

namespace
{

constexpr int kBackendTimeoutMs = 3000;
constexpr int kBackendMaxAttempts = 3;
constexpr int kBackendRetryDelayMs = 220;
constexpr const char* kDefaultBackendUrl = "http://84.201.138.107:8080";
constexpr const char* kBackendUrlEnvVar = "ANGRY_BACKEND_URL";

std::string resolveBackendUrl( std::string baseUrl )
{
    if ( !baseUrl.empty() )
    {
        return baseUrl;
    }

    const char* envUrl = std::getenv( kBackendUrlEnvVar );
    if ( envUrl != nullptr && envUrl[0] != '\0' )
    {
        return std::string( envUrl );
    }

    return std::string( kDefaultBackendUrl );
}

bool isHttpOk( long statusCode )
{
    return statusCode >= 200 && statusCode < 300;
}

bool shouldRetryRequest( const cpr::Response& response )
{
    if ( response.error.code != cpr::ErrorCode::OK )
    {
        return true;
    }

    // Retry only on transient HTTP failures.
    return response.status_code == 408
           || response.status_code == 429
           || ( response.status_code >= 500 && response.status_code <= 599 );
}

template <typename RequestFn>
cpr::Response performRequestWithRetry( const char* opName, RequestFn&& requestFn )
{
    cpr::Response response;

    for ( int attempt = 1; attempt <= kBackendMaxAttempts; ++attempt )
    {
        response = requestFn();

        const bool ok = ( response.error.code == cpr::ErrorCode::OK )
                        && isHttpOk( response.status_code );
        if ( ok )
        {
            return response;
        }

        const bool canRetry = attempt < kBackendMaxAttempts && shouldRetryRequest( response );
        if ( response.error.code != cpr::ErrorCode::OK )
        {
            Logger::error(
                "OnlineScoreClient::{} attempt {}/{} failed: network error={} ({})",
                opName,
                attempt,
                kBackendMaxAttempts,
                static_cast<int>( response.error.code ),
                response.error.message );
        }
        else
        {
            Logger::error(
                "OnlineScoreClient::{} attempt {}/{} failed: http status={}",
                opName,
                attempt,
                kBackendMaxAttempts,
                response.status_code );
        }

        if ( !canRetry )
        {
            return response;
        }

        std::this_thread::sleep_for( std::chrono::milliseconds( kBackendRetryDelayMs ) );
    }

    return response;
}

}  // namespace

OnlineScoreClient::OnlineScoreClient(std::string baseUrl)
    : baseUrl_( resolveBackendUrl( std::move( baseUrl ) ) )
{
    Logger::info( "OnlineScoreClient backend URL: {}", baseUrl_ );
}

bool OnlineScoreClient::submitScore(
    const std::string& playerName,
    int levelId,
    int score,
    int stars)
{
    Logger::info(
        "OnlineScoreClient::submitScore(playerName, ...) is legacy. "
        "Use submitScoreWithToken(token, ...) for JWT backend." );
    Logger::info( "Submitting score to backend..." );

    const json body = {
        {"playerName", playerName},
        {"levelId", levelId},
        {"score", score},
        {"stars", stars},
    };

    const cpr::Response response = performRequestWithRetry(
        "submitScore",
        [&]()
        {
            return cpr::Post(
                cpr::Url{baseUrl_ + "/scores"},
                cpr::Header{{"Content-Type", "application/json"}},
                cpr::Body{body.dump()},
                cpr::Timeout{kBackendTimeoutMs});
        });

    if ( response.error.code != cpr::ErrorCode::OK )
    {
        Logger::error( "OnlineScoreClient::submitScore failed after retries." );
        return false;
    }

    if ( !isHttpOk( response.status_code ) )
    {
        Logger::error(
            "OnlineScoreClient::submitScore failed: final http status={}",
            response.status_code );
        return false;
    }

    Logger::info( "OnlineScoreClient::submitScore success." );
    return true;
}

bool OnlineScoreClient::submitScoreWithToken(
    const std::string& token,
    int levelId,
    int score,
    int stars)
{
    if ( token.empty() )
    {
        Logger::info( "User is not logged in, skipping online score submission" );
        return false;
    }

    Logger::info( "Submitting score with token" );

    const json body = {
        {"levelId", levelId},
        {"score", score},
        {"stars", stars},
    };

    const cpr::Response response = performRequestWithRetry(
        "submitScoreWithToken",
        [&]()
        {
            return cpr::Post(
                cpr::Url{baseUrl_ + "/scores"},
                cpr::Header{
                    {"Content-Type", "application/json"},
                    {"Authorization", "Bearer " + token},
                },
                cpr::Body{body.dump()},
                cpr::Timeout{kBackendTimeoutMs});
        });

    if ( response.error.code != cpr::ErrorCode::OK )
    {
        Logger::error( "OnlineScoreClient::submitScoreWithToken failed after retries." );
        return false;
    }

    if ( !isHttpOk( response.status_code ) )
    {
        Logger::error(
            "OnlineScoreClient::submitScoreWithToken failed: final http status={}",
            response.status_code );
        return false;
    }

    Logger::info( "OnlineScoreClient::submitScoreWithToken success." );
    return true;
}

LeaderboardFetchResult OnlineScoreClient::fetchLeaderboardWithStatus(int levelId)
{
    LeaderboardFetchResult result;

    const cpr::Response response = performRequestWithRetry(
        "fetchLeaderboard",
        [&]()
        {
            return cpr::Get(
                cpr::Url{baseUrl_ + "/leaderboard"},
                cpr::Parameters{{"levelId", std::to_string(levelId)}},
                cpr::Timeout{kBackendTimeoutMs});
        });

    if ( response.error.code != cpr::ErrorCode::OK )
    {
        Logger::error( "OnlineScoreClient::fetchLeaderboard failed after retries." );
        result.status = LeaderboardFetchStatus::Unavailable;
        return result;
    }

    if ( !isHttpOk( response.status_code ) )
    {
        Logger::error(
            "OnlineScoreClient::fetchLeaderboard failed: final http status={}",
            response.status_code );
        result.status = LeaderboardFetchStatus::Unavailable;
        return result;
    }

    const json data = json::parse( response.text, nullptr, false );
    if ( data.is_discarded() )
    {
        Logger::error( "OnlineScoreClient::fetchLeaderboard failed: invalid JSON payload." );
        result.status = LeaderboardFetchStatus::InvalidResponse;
        return result;
    }

    if ( !data.is_array() )
    {
        Logger::error( "OnlineScoreClient::fetchLeaderboard failed: expected JSON array." );
        result.status = LeaderboardFetchStatus::InvalidResponse;
        return result;
    }

    result.entries.reserve( data.size() );
    for ( const json& item : data )
    {
        if ( !item.is_object() )
        {
            Logger::info(
                "OnlineScoreClient::fetchLeaderboard: skipped non-object leaderboard item." );
            continue;
        }

        LeaderboardEntry entry;
        entry.playerName = item.value( "playerName", "" );
        entry.score = item.value( "score", 0 );
        entry.stars = item.value( "stars", 0 );
        result.entries.push_back( std::move( entry ) );
    }

    result.status = result.entries.empty()
                        ? LeaderboardFetchStatus::Empty
                        : LeaderboardFetchStatus::Ok;
    Logger::info( "Leaderboard loaded: {} entries.", result.entries.size() );
    return result;
}

std::vector<LeaderboardEntry> OnlineScoreClient::fetchLeaderboard(int levelId)
{
    return fetchLeaderboardWithStatus( levelId ).entries;
}

}  // namespace angry
