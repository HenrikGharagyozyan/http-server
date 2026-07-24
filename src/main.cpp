#include "server/HttpServer.hpp"
#include "http/Response.hpp"
#include "utils/FileSystem.hpp"

#include <iostream>

#include <nlohmann/json.hpp>

using json = nlohmann::json;


http::Response handle_static_request(const http::Request& req) 
{
    http::Response res;
    
    // Protect against escaping the public folder (directory traversal)
    if (req.uri.find("..") != std::string_view::npos) 
    {
        res.status_code = http::StatusCode::FORBIDDEN;
        res.body = "<h1>403 Forbidden</h1>";
        res.headers["Content-Type"] = "text/html";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
    }

    // Build the file path
    // (Use std::string(req.uri) because uri is now std::string_view)
    std::string filepath = "../public"; // "../" because we run from build/
    if (req.uri == "/") 
    {
        filepath += "/index.html";
    } 
    else 
    {
        filepath += std::string(req.uri); 
    }

    std::string file_content;
    if (utils::read_file(filepath, file_content)) 
    {
        res.status_code = http::StatusCode::OK;
        res.body = std::move(file_content); // std::move for optimization!
        res.headers["Content-Type"] = utils::get_mime_type(filepath);
    } 
    else 
    {
        // If the file is not found
        res.status_code = http::StatusCode::NOT_FOUND;
        res.body = "<h1>404 - File Not Found</h1>";
        res.headers["Content-Type"] = "text/html";
    }

    res.headers["Content-Length"] = std::to_string(res.body.size());
    return res;
}


int main() 
{
    try 
    {
        server::HttpServer app;
        
        // 1. GET /api/users — Возвращает список пользователей в формате JSON
        app.get("/api/users", [](const http::Request& /*req*/) 
            {
                http::Response res;
                
                // Создаем JSON объект легко и красиво
                json response_data = {
                        {"status", "success"},
                        {"users", {
                            {{"id", 1}, {"name", "Henrik"}, {"role", "developer"}},
                            {{"id", 2}, {"name", "Arshavir"}, {"role", "collaborator"}}
                        }}
                    };

                res.status_code = http::StatusCode::OK;
                res.body = response_data.dump(4); // dump(4) красивый форматированный вывод с отступами
                res.headers["Content-Type"] = "application/json";
                res.headers["Content-Length"] = std::to_string(res.body.size());
                return res;
            });

        // 2. POST /api/users — Создание нового пользователя с валидацией JSON
        app.post("/api/users", [](const http::Request& req) 
            {
                http::Response res;
                res.headers["Content-Type"] = "application/json";

                try 
                {
                    // Парсим входящее тело
                    json parsed_body = json::parse(req.body);

                    // Проверяем обязательные поля
                    if (!parsed_body.contains("name") || !parsed_body.contains("role")) 
                    {
                        res.status_code = http::StatusCode::BAD_REQUEST;
                        res.body = json({
                            {"error", "Bad Request"},
                            {"message", "Missing 'name' or 'role' fields"}
                        }).dump();
                        res.headers["Content-Length"] = std::to_string(res.body.size());
                        return res;
                    }

                    std::string name = parsed_body["name"];
                    std::string role = parsed_body["role"];

                    // Формируем ответ об успешном создании ресурса
                    json response_data = {
                        {"status", "created"},
                        {"message", "User created successfully"},
                        {"user", {
                            {"id", 101}, // Имитация сгенерированного ID
                            {"name", name},
                            {"role", role}
                        }}
                    };

                    res.status_code = http::StatusCode::OK;
                    res.body = response_data.dump(4);

                } 
                catch (const json::parse_error& e) 
                {
                    // Ошибка парсинга JSON (невалидный синтаксис от клиента)
                    res.status_code = http::StatusCode::BAD_REQUEST;
                    res.body = json({
                        {"error", "Invalid JSON format"},
                        {"details", e.what()}
                    }).dump();
                }

                res.headers["Content-Length"] = std::to_string(res.body.size());
                return res;
            });

        // Static file handler (fallback)
        app.set_default_handler(handle_static_request);

        // Run server
        app.listen(8080);
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}