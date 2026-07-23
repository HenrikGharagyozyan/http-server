#pragma once

#include <string>

namespace utils 
{

    // Reads a file into a string (binary). Returns false if the file does not exist.
    bool read_file(const std::string& path, std::string& out_content);
    
    // Determines the Content-Type (MIME type) based on file extension
    std::string get_mime_type(const std::string& path);

}