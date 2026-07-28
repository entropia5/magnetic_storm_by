#include "telegram_input.h"

#include "utilities.h"

#include <nlohmann/json.hpp>

bool is_command(const std::string& text, const std::string& command) {
    std::string token = trim_copy(text);
    const std::size_t separator = token.find_first_of(" \t\r\n");
    if (separator != std::string::npos) {
        token = token.substr(0, separator);
    }
    return token == command || token.rfind(command + "@", 0) == 0;
}

std::string telegram_user_display_name(const nlohmann::json& message) {
    if (!message.contains("from") || !message["from"].is_object()) {
        return {};
    }
    const auto& sender = message["from"];
    std::string name;
    if (sender.contains("first_name") && sender["first_name"].is_string()) {
        name = sender["first_name"].get<std::string>();
    }
    if (sender.contains("last_name") && sender["last_name"].is_string()) {
        if (!name.empty()) name += ' ';
        name += sender["last_name"].get<std::string>();
    }
    name = trim_copy(name);
    if (!name.empty()) return name;
    if (sender.contains("username") && sender["username"].is_string()) {
        return "@" + sender["username"].get<std::string>();
    }
    return {};
}
