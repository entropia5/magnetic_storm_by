#pragma once

#include <cpr/response.h>

#include <string>

bool weather_configured();
cpr::Response fetch_weather_response(const std::string& query, long long chat_id);
cpr::Response fetch_weather_forecast_response(const std::string& query, long long chat_id);
