#pragma once

#include <ctime>
#include <string>

std::tm get_minsk_time(int offset_days = 0);
std::string get_weekday_name(int offset_days, long long chat_id);
