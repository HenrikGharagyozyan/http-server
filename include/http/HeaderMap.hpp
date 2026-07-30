#pragma once

#include <string>
#include <unordered_map>
#include <memory_resource>
#include <cctype>

namespace http 
{

    struct CaseInsensitiveHash 
    {
        using is_transparent = void;
        std::size_t operator()(std::string_view sv) const noexcept 
        {
            std::size_t hash = 14695981039346656037ULL;
            for (char c : sv) 
            {
                hash ^= static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(c)));
                hash *= 1099511628211ULL;
            }
            return hash;
        }
    };

    struct CaseInsensitiveEqual 
    {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const noexcept 
        {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) 
            {
                if (std::tolower(static_cast<unsigned char>(a[i])) !=
                    std::tolower(static_cast<unsigned char>(b[i]))) 
                {
                    return false;
                }
            }
            return true;
        }
    };

    using HeaderMap = std::pmr::unordered_map<
        std::pmr::string, 
        std::pmr::string, 
        CaseInsensitiveHash, 
        CaseInsensitiveEqual
    >;

} // namespace http