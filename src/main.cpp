#include "server/HttpServer.hpp"
#include "handlers/StaticHandler.hpp"
#include "handlers/UserHandler.hpp"
#include "utils/Logger.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <csignal>

using json = nlohmann::json;

// Глобальный указатель нужен для перехвата сигналов от ОС
server::HttpServer* g_app = nullptr;

// Обработчик сигналов (вызывается при нажатии Ctrl+C или команде kill)
void signal_handler(int signal_num) 
{
    LOG_INFO("Received signal {}. Initiating graceful shutdown...", signal_num);
    if (g_app) 
    {
        g_app->stop();
    }
}

int main() 
{
    // Регистрируем перехват сигналов
    std::signal(SIGINT, signal_handler);  // Ctrl+C
    std::signal(SIGTERM, signal_handler); // Сигнал завершения системы/docker

    try 
    {
        // 1. Читаем конфигурацию
        std::ifstream config_file("../config.json"); // Путь зависит от папки запуска (build)
        if (!config_file.is_open()) 
        {
            LOG_ERROR("Could not open config.json!");
            return 1;
        }
        
        json config = json::parse(config_file);
        uint16_t port = config["server"]["port"];
        size_t threads = config["server"]["threads"];

        // 2. Инициализируем сервер
        server::HttpServer app;
        g_app = &app; // Передаем ссылку глобальному указателю
        
        app.get("/api/users", handlers::get_users);
        app.post("/api/users", handlers::create_user);
        app.set_default_handler(handlers::handle_static_request);

        // 3. Запускаем сервер с параметрами из конфига
        app.listen(port, threads);
    } 
    catch (const json::parse_error& e) 
    {
        LOG_ERROR("Config parse error: {}", e.what());
        return 1;
    }
    catch (const std::exception& e) 
    {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}