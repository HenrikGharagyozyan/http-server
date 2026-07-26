#pragma once

#include "http/Request.hpp"

#include <string_view>
#include <memory_resource>


namespace http 
{

    // Parsing function. Throws std::invalid_argument if the format is invalid.
    Request parse_request(
        std::string_view raw_data, 
        std::pmr::memory_resource* mr = std::pmr::get_default_resource()
    );

} // namespace http