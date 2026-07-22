#pragma once

#include "http/Request.hpp"

#include <string_view>

namespace http 
{

    // Parsing function. Throws std::invalid_argument if the format is invalid.
    Request parse_request(std::string_view raw_data);

} // namespace http