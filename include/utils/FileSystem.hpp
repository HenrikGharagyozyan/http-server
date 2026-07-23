#pragma once

#include <string>

namespace utils 
{

    // Читает файл в строку (бинарно). Возвращает false, если файла нет.
    bool read_file(const std::string& path, std::string& out_content);
    
    // Определяет Content-Type (MIME type) по расширению файла
    std::string get_mime_type(const std::string& path);

}