#include "template_engine.h"

#include <fstream>
#include <sstream>

namespace template_engine {

std::string read_file_or_default(const std::string& path, const std::string& fallback) {
    std::ifstream input(path);
    if (!input) {
        return fallback;
    }

    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

static void replace_all(std::string& text, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return;
    }

    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string render(std::string text, const std::map<std::string, std::string>& values) {
    for (const auto& [key, value] : values) {
        replace_all(text, "{{" + key + "}}", value);
    }
    return text;
}

}
