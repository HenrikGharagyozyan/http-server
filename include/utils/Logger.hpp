#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace utils 
{

    // Получить глобальный потокобезопасный логгер
    inline std::shared_ptr<spdlog::logger>& get_logger() 
    {
        static auto logger = []() 
            {
                // Создаем цветной консольный логгер (stdout)
                auto l = spdlog::stdout_color_mt("SERVER_LOGGER");
                l->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [TID %t] %v");
                l->set_level(spdlog::level::info);
                return l;
            }();

        return logger;
    }
    
}

// Макросы для удобного использования во всем проекте
#define LOG_INFO(...)  ::utils::get_logger()->info(__VA_ARGS__)
#define LOG_WARN(...)  ::utils::get_logger()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::utils::get_logger()->error(__VA_ARGS__)
#define LOG_DEBUG(...) ::utils::get_logger()->debug(__VA_ARGS__)