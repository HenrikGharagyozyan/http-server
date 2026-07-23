#include "utils/FileSystem.hpp"
#include <fstream>
#include <sstream>

namespace utils 
{

    bool read_file(const std::string& path, std::string& out_content) 
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) 
            return false;
        
        std::ostringstream ss;
        ss << file.rdbuf(); // Fast read of the entire file
        out_content = ss.str();
        return true;
    }

    std::string get_mime_type(const std::string& path) 
    {
        size_t dot_pos = path.find_last_of('.');
        if (dot_pos == std::string::npos) 
            return "application/octet-stream";
        
        std::string ext = path.substr(dot_pos);
        if (ext == ".html") return "text/html";
        if (ext == ".css") return "text/css";
        if (ext == ".js") return "application/javascript";
        if (ext == ".png") return "image/png";
        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".ico") return "image/x-icon";
        
        return "application/octet-stream";
    }

} // namespace utils