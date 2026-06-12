#pragma once

#include <map>
#include <string>

namespace template_engine {

std::string read_file_or_default(const std::string& path, const std::string& fallback);
std::string render(std::string text, const std::map<std::string, std::string>& values);

}
