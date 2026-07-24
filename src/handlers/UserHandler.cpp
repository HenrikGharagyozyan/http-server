#include "handlers/UserHandler.hpp"
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace handlers 
{

    http::Response get_users(const http::Request& /*req*/) 
    {
        http::Response res;
        
        json response_data = {
            {"status", "success"},
            {"users", {
                {{"id", 1}, {"name", "Henrik"}, {"role", "developer"}},
                {{"id", 2}, {"name", "Arshavir"}, {"role", "collaborator"}}
            }}
        };

        res.status_code = http::StatusCode::OK;
        res.body = response_data.dump(4);
        res.headers["Content-Type"] = "application/json";
        res.headers["Content-Length"] = std::to_string(res.body.size());
        
        return res;
    }

    http::Response create_user(const http::Request& req) 
    {
        http::Response res;
        res.headers["Content-Type"] = "application/json";

        try 
        {
            json parsed_body = json::parse(req.body);

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

            json response_data = {
                {"status", "created"},
                {"message", "User created successfully"},
                {"user", {
                    {"id", 101}, 
                    {"name", name},
                    {"role", role}
                }}
            };

            res.status_code = http::StatusCode::OK;
            res.body = response_data.dump(4) + "\n";
        } 
        catch (const json::parse_error& e) 
        {
            res.status_code = http::StatusCode::BAD_REQUEST;
            res.body = json({
                {"error", "Invalid JSON format"},
                {"details", e.what()}
            }).dump() + "\n";
        }

        res.headers["Content-Length"] = std::to_string(res.body.size());
        return res;
    }

} // namespace handlers