#pragma once

#include "domain_types.h"

#include <cstddef>
#include <string>
#include <vector>

WeatherInfo fetch_weather_info(std::string location, long long chat_id);
std::vector<WeatherForecastSlot> fetch_weather_forecast_slots(
    std::string location,
    long long chat_id,
    std::size_t limit = 8
);
