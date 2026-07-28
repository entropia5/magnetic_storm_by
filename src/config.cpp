#include "config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {

std::string trim(std::string value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

std::string unquote(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2) {
        const char first = value.front();
        const char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substr(1, value.size() - 2);
        }
    }
    return value;
}

}  // namespace

void load_env_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::string current = trim(line);
        if (current.empty() || current.front() == '#') {
            continue;
        }
        if (current.rfind("export ", 0) == 0) {
            current = trim(current.substr(7));
        }

        const std::size_t equals = current.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = trim(current.substr(0, equals));
        const std::string value = unquote(current.substr(equals + 1));
        if (!key.empty() && std::getenv(key.c_str()) == nullptr) {
            setenv(key.c_str(), value.c_str(), 0);
        }
    }
}

const char* first_env_value(std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const char* value = std::getenv(name);
        if (value != nullptr && *value != '\0') {
            return value;
        }
    }
    return nullptr;
}

long long parse_optional_chat_id(const char* value, const std::string& env_name) {
    if (value == nullptr || *value == '\0') {
        return 0;
    }

    const std::string current = trim(value);
    try {
        std::size_t parsed = 0;
        const long long chat_id = std::stoll(current, &parsed);
        if (parsed != current.size() || chat_id <= 0) {
            std::cerr << "⚠️ " << env_name
                      << " должен быть положительным числом, dev-команды отключены\n";
            return 0;
        }
        return chat_id;
    } catch (...) {
        std::cerr << "⚠️ " << env_name
                  << " не удалось прочитать как Telegram chat_id, dev-команды отключены\n";
        return 0;
    }
}
