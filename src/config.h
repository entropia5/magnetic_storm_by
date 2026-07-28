#pragma once

#include <initializer_list>
#include <string>

void load_env_file(const std::string& path);
const char* first_env_value(std::initializer_list<const char*> names);
long long parse_optional_chat_id(const char* value, const std::string& env_name);
