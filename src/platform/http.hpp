#pragma once

#include <string>
#include <utility>
#include <vector>

#ifndef __EMSCRIPTEN__
#include <cpr/cpr.h>
#else
#include <emscripten.h>
#include <emscripten/fetch.h>
#include <cctype>
#include <cstdio>
#include <cstring>
#endif

namespace platform::http
{

using Header = std::pair<std::string, std::string>;
using QueryParam = std::pair<std::string, std::string>;
using Headers = std::vector<Header>;
using QueryParams = std::vector<QueryParam>;

struct Response
{
    int status_code = 0;
    std::string body;
    bool network_error = false;
    std::string error_message;
};

inline bool is_http_ok( const Response& response )
{
    return response.status_code >= 200 && response.status_code < 300;
}

#ifndef __EMSCRIPTEN__

inline Response post( const std::string& url,
                      const std::string& body,
                      const Headers& headers,
                      int timeout_ms )
{
    cpr::Header cpr_headers;
    for ( const auto& [key, value] : headers )
    {
        cpr_headers[key] = value;
    }

    const cpr::Response raw = cpr::Post(
        cpr::Url {url},
        cpr_headers,
        cpr::Body {body},
        cpr::Timeout {timeout_ms} );

    Response response;
    response.status_code = static_cast<int> ( raw.status_code );
    response.body = raw.text;
    response.network_error = raw.error.code != cpr::ErrorCode::OK;
    if ( response.network_error )
    {
        response.error_message = raw.error.message;
    }
    return response;
}

inline Response get( const std::string& url,
                     const QueryParams& query_params,
                     const Headers& headers,
                     int timeout_ms )
{
    cpr::Parameters cpr_query;
    for ( const auto& [key, value] : query_params )
    {
        cpr_query.Add( cpr::Parameter {key, value} );
    }

    cpr::Header cpr_headers;
    for ( const auto& [key, value] : headers )
    {
        cpr_headers[key] = value;
    }

    const cpr::Response raw = cpr::Get(
        cpr::Url {url},
        cpr_query,
        cpr_headers,
        cpr::Timeout {timeout_ms} );

    Response response;
    response.status_code = static_cast<int> ( raw.status_code );
    response.body = raw.text;
    response.network_error = raw.error.code != cpr::ErrorCode::OK;
    if ( response.network_error )
    {
        response.error_message = raw.error.message;
    }
    return response;
}

#else

inline std::string url_encode_component( const std::string& value )
{
    static constexpr char kHex[] = "0123456789ABCDEF";

    std::string encoded;
    encoded.reserve( value.size() * 3 );

    for ( unsigned char ch : value )
    {
        if ( std::isalnum( ch ) || ch == '-' || ch == '_' || ch == '.' || ch == '~' )
        {
            encoded.push_back( static_cast<char> ( ch ) );
        }
        else
        {
            encoded.push_back( '%' );
            encoded.push_back( kHex[( ch >> 4 ) & 0x0F] );
            encoded.push_back( kHex[ch & 0x0F] );
        }
    }

    return encoded;
}

inline std::string with_query_params( const std::string& url, const QueryParams& query_params )
{
    if ( query_params.empty() )
    {
        return url;
    }

    std::string full_url = url;
    full_url.push_back( '?' );

    for ( std::size_t i = 0; i < query_params.size(); ++i )
    {
        if ( i > 0 )
        {
            full_url.push_back( '&' );
        }

        full_url += url_encode_component( query_params[i].first );
        full_url.push_back( '=' );
        full_url += url_encode_component( query_params[i].second );
    }

    return full_url;
}

inline std::vector<const char*> flatten_headers( const Headers& headers,
                                                 std::vector<std::string>* storage )
{
    std::vector<const char*> flat;
    flat.reserve( headers.size() * 2 + 1 );
    storage->reserve( headers.size() * 2 );

    for ( const auto& [key, value] : headers )
    {
        storage->push_back( key );
        storage->push_back( value );
        flat.push_back( storage->at( storage->size() - 2 ).c_str() );
        flat.push_back( storage->at( storage->size() - 1 ).c_str() );
    }
    flat.push_back( nullptr );
    return flat;
}

inline Response fetch_sync( const std::string& method,
                            const std::string& url,
                            const std::string& body,
                            const Headers& headers,
                            int timeout_ms )
{
    struct FetchSyncContext
    {
        Response response;
        bool done = false;
    };

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init( &attr );
    std::snprintf( attr.requestMethod, sizeof( attr.requestMethod ), "%s", method.c_str() );
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.timeoutMSecs = timeout_ms;

    FetchSyncContext context;
    attr.userData = &context;
    attr.onsuccess = [] ( emscripten_fetch_t* fetch )
    {
        auto* ctx = static_cast<FetchSyncContext*> ( fetch->userData );
        ctx->response.status_code = static_cast<int> ( fetch->status );
        if ( fetch->data != nullptr && fetch->numBytes > 0 )
        {
            ctx->response.body.assign ( fetch->data,
                                        static_cast<std::size_t> ( fetch->numBytes ) );
        }
        if ( fetch->status == 0 )
        {
            ctx->response.network_error = true;
            ctx->response.error_message =
                ( fetch->statusText[0] != '\0' ) ? fetch->statusText : "network error";
        }
        ctx->done = true;
        emscripten_fetch_close ( fetch );
    };
    attr.onerror = [] ( emscripten_fetch_t* fetch )
    {
        auto* ctx = static_cast<FetchSyncContext*> ( fetch->userData );
        ctx->response.status_code = static_cast<int> ( fetch->status );
        ctx->response.network_error = true;
        if ( fetch->statusText[0] != '\0' )
        {
            ctx->response.error_message = fetch->statusText;
        }
        else
        {
            ctx->response.error_message = "network error";
        }
        ctx->done = true;
        emscripten_fetch_close ( fetch );
    };

    std::vector<std::string> header_storage;
    std::vector<const char*> flat_headers = flatten_headers( headers, &header_storage );
    attr.requestHeaders = flat_headers.data();

    std::string request_body = body;
    if ( method == "POST" )
    {
        attr.requestData = request_body.empty() ? nullptr : request_body.c_str();
        attr.requestDataSize = request_body.size();
    }

    emscripten_fetch_t* fetch = emscripten_fetch( &attr, url.c_str() );

    if ( fetch == nullptr )
    {
        context.response.network_error = true;
        context.response.error_message = "emscripten_fetch returned nullptr";
        return context.response;
    }

    while ( !context.done )
    {
        emscripten_sleep ( 1 );
    }

    return context.response;
}

inline Response post( const std::string& url,
                      const std::string& body,
                      const Headers& headers,
                      int timeout_ms )
{
    return fetch_sync( "POST", url, body, headers, timeout_ms );
}

inline Response get( const std::string& url,
                     const QueryParams& query_params,
                     const Headers& headers,
                     int timeout_ms )
{
    const std::string full_url = with_query_params( url, query_params );
    return fetch_sync( "GET", full_url, std::string {}, headers, timeout_ms );
}

#endif

}  // namespace platform::http
