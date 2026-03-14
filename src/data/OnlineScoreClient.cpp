// ============================================================
// OnlineScoreClient.cpp — Online score client implementation.
// Part of: angry::data
//
// Implements network interactions with leaderboard backend:
//   * Resolves backend URL from arg/env/default
//   * Executes requests with retry on transient failures
//   * Submits scores (legacy and JWT-authenticated variants)
//   * Fetches/parses leaderboard data with status mapping
// ============================================================

#include "OnlineScoreClient.hpp"

#include "logger.hpp"
#include "platform/http.hpp"

#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdlib>
#include <thread>
#include <mutex>

namespace angry
{

using json = nlohmann::json;

// #=# Local Helpers & Constants #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

namespace
{

constexpr int kBackendTimeoutMs = 3000;
#ifdef __EMSCRIPTEN__
constexpr int kBackendMaxAttempts = 1;
constexpr int kBackendRetryDelayMs = 0;
#else
constexpr int kBackendMaxAttempts = 3;
constexpr int kBackendRetryDelayMs = 220;
#endif
constexpr const char* kDefaultBackendUrl = "http://84.201.138.107:8080";
constexpr const char* kBackendUrlEnvVar = "ANGRY_BACKEND_URL";

std::string resolve_backend_url( std::string baseUrl )
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

bool should_retry_request( const platform::http::Response& response )
{
    if ( response.network_error )
    {
        return true;
    }

    // Retry only on transient HTTP failures.
    return response.status_code == 408
           || response.status_code == 429
           || ( response.status_code >= 500 && response.status_code <= 599 );
}

template <typename RequestFn>
platform::http::Response perform_request_with_retry( const char* opName, RequestFn&& requestFn )
{
    platform::http::Response response;

    for ( int attempt = 1; attempt <= kBackendMaxAttempts; ++attempt )
    {
        response = requestFn();

        const bool ok = !response.network_error
                        && platform::http::is_http_ok( response );
        if ( ok )
        {
            return response;
        }

        const bool canRetry = attempt < kBackendMaxAttempts && should_retry_request( response );
        if ( response.network_error )
        {
            Logger::error(
                "OnlineScoreClient::{} attempt {}/{} failed: network error: {}",
                opName,
                attempt,
                kBackendMaxAttempts,
                response.error_message );
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

        if constexpr ( kBackendRetryDelayMs > 0 )
        {
            std::this_thread::sleep_for( std::chrono::milliseconds( kBackendRetryDelayMs ) );
        }
    }

    return response;
}

}  // namespace

// #=# Construction #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

OnlineScoreClient::OnlineScoreClient(std::string baseUrl)
    : baseUrl_( resolve_backend_url( std::move( baseUrl ) ) )
{
    static std::once_flag logBackendUrlOnce;
    std::call_once(
        logBackendUrlOnce,
        [this]()
        {
            Logger::info( "OnlineScoreClient backend URL: {}", baseUrl_ );
        } );
}

// #=# Score Submission API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

bool OnlineScoreClient::submit_score(
    const std::string& playerName,
    int levelId,
    int score,
    int stars)
{
    Logger::info(
        "OnlineScoreClient::submit_score(playerName, ...) is legacy. "
        "Use submit_score_with_token(token, ...) for JWT backend." );
    Logger::info( "Submitting score to backend..." );

    const json body = {
        {"playerName", playerName},
        {"levelId", levelId},
        {"score", score},
        {"stars", stars},
    };

    const platform::http::Response response = perform_request_with_retry(
        "submit_score",
        [&]()
        {
            return platform::http::post(
                baseUrl_ + "/scores",
                body.dump(),
                platform::http::Headers {
                    {"Content-Type", "application/json"},
                },
                kBackendTimeoutMs );
        });

    if ( response.network_error )
    {
        Logger::error( "OnlineScoreClient::submit_score failed after retries." );
        return false;
    }

    if ( !platform::http::is_http_ok( response ) )
    {
        Logger::error(
            "OnlineScoreClient::submit_score failed: final http status={}",
            response.status_code );
        return false;
    }

    Logger::info( "OnlineScoreClient::submit_score success." );
    return true;
}

bool OnlineScoreClient::submit_score_with_token(
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

    const platform::http::Response response = perform_request_with_retry(
        "submit_score_with_token",
        [&]()
        {
            return platform::http::post(
                baseUrl_ + "/scores",
                body.dump(),
                platform::http::Headers {
                    {"Content-Type", "application/json"},
                    {"Authorization", "Bearer " + token},
                },
                kBackendTimeoutMs );
        });

    if ( response.network_error )
    {
        Logger::error( "OnlineScoreClient::submit_score_with_token failed after retries." );
        return false;
    }

    if ( !platform::http::is_http_ok( response ) )
    {
        Logger::error(
            "OnlineScoreClient::submit_score_with_token failed: final http status={}",
            response.status_code );
        return false;
    }

    Logger::info( "OnlineScoreClient::submit_score_with_token success." );
    return true;
}

// #=# Leaderboard API #=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

LeaderboardFetchResult OnlineScoreClient::fetch_leaderboard_with_status(int levelId)
{
    LeaderboardFetchResult result;

    const platform::http::Response response = perform_request_with_retry(
        "fetch_leaderboard",
        [&]()
        {
            return platform::http::get(
                baseUrl_ + "/leaderboard",
                platform::http::QueryParams {
                    {"levelId", std::to_string( levelId )},
                },
                platform::http::Headers {},
                kBackendTimeoutMs );
        });

    if ( response.network_error )
    {
        Logger::error( "OnlineScoreClient::fetch_leaderboard failed after retries." );
        result.status = LeaderboardFetchStatus::Unavailable;
        return result;
    }

    if ( !platform::http::is_http_ok( response ) )
    {
        Logger::error(
            "OnlineScoreClient::fetch_leaderboard failed: final http status={}",
            response.status_code );
        result.status = LeaderboardFetchStatus::Unavailable;
        return result;
    }

    const json data = json::parse( response.body, nullptr, false );
    if ( data.is_discarded() )
    {
        Logger::error( "OnlineScoreClient::fetch_leaderboard failed: invalid JSON payload." );
        result.status = LeaderboardFetchStatus::InvalidResponse;
        return result;
    }

    if ( data.is_null() )
    {
        result.status = LeaderboardFetchStatus::Empty;
        return result;
    }

    if ( !data.is_array() )
    {
        Logger::error( "OnlineScoreClient::fetch_leaderboard failed: expected JSON array." );
        result.status = LeaderboardFetchStatus::InvalidResponse;
        return result;
    }

    result.entries.reserve( data.size() );
    for ( const json& item : data )
    {
        if ( !item.is_object() )
        {
            Logger::info(
                "OnlineScoreClient::fetch_leaderboard: skipped non-object leaderboard item." );
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

std::vector<LeaderboardEntry> OnlineScoreClient::fetch_leaderboard(int levelId)
{
    return fetch_leaderboard_with_status( levelId ).entries;
}

}  // namespace angry
