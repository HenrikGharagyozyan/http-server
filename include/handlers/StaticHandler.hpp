#pragma once

#include "http/Request.hpp"
#include "http/Response.hpp"


namespace handlers 
{

    http::Response handle_static_request(const http::Request& req);

}