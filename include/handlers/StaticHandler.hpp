#pragma once

#include "http/Request.hpp"
#include "http/Response.hpp"

#include <string>


namespace handlers 
{

    // Initializes the cache by loading all files from the specified directory into memory
    void init_static_cache(const std::string& public_dir);

    http::Response handle_static_request(const http::Request& req);

}