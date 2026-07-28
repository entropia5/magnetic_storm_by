#pragma once

#include <nlohmann/json_fwd.hpp>

#include <string>
#include <vector>

std::string trim_copy(const std::string& value);
std::string ascii_lower_copy(std::string value);
bool write_lines_atomic(const std::string& path, const std::vector<std::string>& lines);
bool write_json_atomic(const std::string& path, const nlohmann::json& data);
