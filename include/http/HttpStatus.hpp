#pragma once

#include <string_view>


namespace http 
{
    
    enum class StatusCode 
    {
        OK                         = 200,
        CREATED                    = 201,
        BAD_REQUEST                = 400,
        FORBIDDEN                  = 403,
        NOT_FOUND                  = 404,
        METHOD_NOT_ALLOWED         = 405,
        REQUEST_TIMEOUT            = 408,
        PAYLOAD_TOO_LARGE          = 413,
        URI_TOO_LONG               = 414,
        HEADER_FIELDS_TOO_LARGE    = 431,
        INTERNAL_SERVER_ERROR      = 500,
        NOT_IMPLEMENTED            = 501,
        SERVICE_UNAVAILABLE        = 503,
        HTTP_VERSION_NOT_SUPPORTED = 505
    };

    inline std::string_view get_status_message(StatusCode code) 
    {
        switch (code) 
        {
            case StatusCode::OK:                         return "OK";
            case StatusCode::CREATED:                    return "Created";
            case StatusCode::BAD_REQUEST:                return "Bad Request";
            case StatusCode::FORBIDDEN:                  return "Forbidden";
            case StatusCode::NOT_FOUND:                  return "Not Found";
            case StatusCode::METHOD_NOT_ALLOWED:         return "Method Not Allowed";
            case StatusCode::REQUEST_TIMEOUT:            return "Request Timeout";
            case StatusCode::PAYLOAD_TOO_LARGE:          return "Payload Too Large";
            case StatusCode::URI_TOO_LONG:               return "URI Too Long";
            case StatusCode::HEADER_FIELDS_TOO_LARGE:    return "Request Header Fields Too Large";
            case StatusCode::INTERNAL_SERVER_ERROR:      return "Internal Server Error";
            case StatusCode::NOT_IMPLEMENTED:            return "Not Implemented";
            case StatusCode::SERVICE_UNAVAILABLE:        return "Service Unavailable";
            case StatusCode::HTTP_VERSION_NOT_SUPPORTED: return "HTTP Version Not Supported";
            default:                                     return "Unknown";
        }
    }

} // namespace http