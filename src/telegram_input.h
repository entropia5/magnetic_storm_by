#pragma once

#include <nlohmann/json_fwd.hpp>

#include <string>

bool is_command(const std::string& text, const std::string& command);
std::string telegram_user_display_name(const nlohmann::json& message);
