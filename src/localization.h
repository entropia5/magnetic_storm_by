#pragma once

#include <string>

std::string lang_of(long long chat_id);
std::string localize(
    long long chat_id,
    const std::string& russian,
    const std::string& belarusian,
    const std::string& english
);
std::string wind_unit(long long chat_id);
void save_user_language(long long chat_id, const std::string& language);
void load_languages();
std::string get_month_name(int month, long long chat_id);
std::string get_text(
    long long chat_id,
    const std::string& key,
    const std::string& first_argument = "",
    const std::string& second_argument = ""
);
