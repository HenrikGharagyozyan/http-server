#pragma once

#include "http/Request.hpp"

#include <string_view>

namespace http 
{

    // Функция парсинга. Бросает std::invalid_argument, если формат неверный.
    Request parse_request(std::string_view raw_data);

} // namespace http