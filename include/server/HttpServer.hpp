#pragma once

#include "server/TcpServer.hpp"
#include "server/Router.hpp"
#include <string>

namespace server 
{

    class HttpServer 
    {
    public:
        HttpServer() = default;

        // Прокси-методы для регистрации маршрутов
        void get(const std::string& uri, Handler handler);
        void post(const std::string& uri, Handler handler);

        // Главный метод запуска (содержит тот самый while loop)
        void listen(uint16_t port);

    private:
        TcpServer tcp_server_;
        Router router_;
    };

} // namespace server