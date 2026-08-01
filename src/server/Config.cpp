
#include "server/Config.hpp"
#include "utils/Logger.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdlib>
#include <stdexcept>

namespace server 
{

    Config Config::load(int argc, char* argv[]) 
    {
        Config config;
        
        // 1. Читаем путь к конфигу из argv[1] или используем дефолтный
        std::string config_path = (argc > 1) ? argv[1] : "config.json";
        
        // 2. Пытаемся загрузить JSON
        std::ifstream file(config_path);
        if (file.is_open()) 
        {
            try 
            {
                nlohmann::json j = nlohmann::json::parse(file);
                
                if (j.contains("server")) 
                {
                    if (j["server"].contains("port")) config.port = j["server"]["port"];
                    if (j["server"].contains("threads")) config.threads = j["server"]["threads"];
                    if (j["server"].contains("public_dir")) config.public_dir = j["server"]["public_dir"];
                }
                LOG_INFO("Loaded configuration from {}", config_path);
            } 
            catch (const std::exception& e) 
            {
                LOG_ERROR("Failed to parse {}: {}", config_path, e.what());
                throw std::runtime_error("Invalid config file format");
            }
        } 
        else if (argc > 1) 
        {
            // Если пользователь явно передал путь, но файл не найден — это ошибка
            LOG_ERROR("Could not open requested config file: {}", config_path);
            throw std::runtime_error("Config file not found");
        } 
        else 
        {
            LOG_INFO("No config.json found, using default settings.");
        }
        
        // 3. Environment variable override (очень важно для Docker/AWS/Heroku)
        if (const char* env_port = std::getenv("PORT")) 
        {
            config.port = static_cast<uint16_t>(std::stoi(env_port));
            LOG_INFO("Overriding port with ENV variable: {}", config.port);
        }
        
        // 4. Валидация (защита "от дурака")
        if (config.threads == 0) 
        {
            LOG_WARN("Threads cannot be 0. Setting to 1 to prevent deadlock.");
            config.threads = 1;
        }
        
        return config;
    }
    
}