#pragma once
#include "http/Request.hpp"
#include "http/Response.hpp"

namespace handlers 
{

    http::Response get_users(const http::Request& req);
    http::Response create_user(const http::Request& req);

}