#pragma once

#include <string>

std::string weather_emoji_from_code(
    const std::string& icon_code,
    const std::string& description
);
std::string weather_icon_for_description(const std::string& description);
std::string normalize_location(const std::string& location);
