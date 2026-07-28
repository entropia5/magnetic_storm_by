#pragma once

#include "domain_types.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <vector>

std::string get_kp_status(double kp, long long chat_id);
std::string get_current_kp_guidance(double kp, long long chat_id);
bool kp_available(double kp);
std::string kp_unavailable_text(long long chat_id);
std::string kp_color(double kp);
std::string storm_level_label(double kp);
std::string kp_short_label(double kp, long long chat_id);
std::string format_precipitation(const WeatherForecastSlot& slot, long long chat_id);
std::string format_precipitation_compact(const WeatherForecastSlot& slot, long long chat_id);
std::string kp_slot_hour(std::size_t index);
std::string morning_kp_detail(long long chat_id, double current_kp, const std::vector<KpForecast>& forecast);
std::string morning_weather_detail(long long chat_id, const WeatherInfo& weather, const std::vector<WeatherForecastSlot>& slots);
std::string forecast_bursts_summary(const KpForecast& forecast, long long chat_id);
std::string forecast_day_supplement(long long chat_id, const KpForecast& forecast);
std::string weather_supplement_text(long long chat_id, const WeatherInfo& weather, const std::vector<WeatherForecastSlot>& slots, bool saved_city);
std::string current_minsk_datetime(long long chat_id);
nlohmann::json make_inline_keyboard(
    long long chat_id,
    int forecast_page = -1,
    int forecast_total = 0,
    const std::string& page_callback = "forecast"
);
