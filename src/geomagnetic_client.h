#pragma once

#include "domain_types.h"

#include <vector>

double fetch_current_kp();
std::vector<KpForecast> fetch_kp_forecast_3day(long long chat_id);
