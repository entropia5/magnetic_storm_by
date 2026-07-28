#include "weather_client.h"

#include "localization.h"
#include "runtime_state.h"

#include <cpr/cpr.h>

namespace {

std::string weather_api_lang(long long chat_id) {
    const std::string language = lang_of(chat_id);
    if (language == "en") {
        return "en";
    }
    if (language == "be") {
        return "be";
    }
    return "ru";
}

}  // namespace

bool weather_configured() {
    return !WEATHER_API_KEY.empty();
}

cpr::Response fetch_weather_response(const std::string& query, long long chat_id) {
    cpr::Response empty;
    if (!weather_configured()) {
        empty.status_code = 0;
        return empty;
    }

    return cpr::Get(
        cpr::Url{"https://api.openweathermap.org/data/2.5/weather"},
        cpr::Parameters{
            {"q", query},
            {"units", "metric"},
            {"lang", weather_api_lang(chat_id)},
            {"appid", WEATHER_API_KEY}
        },
        cpr::Timeout{8000}
    );
}

cpr::Response fetch_weather_forecast_response(
    const std::string& query,
    long long chat_id
) {
    cpr::Response empty;
    if (!weather_configured()) {
        empty.status_code = 0;
        return empty;
    }

    return cpr::Get(
        cpr::Url{"https://api.openweathermap.org/data/2.5/forecast"},
        cpr::Parameters{
            {"q", query},
            {"units", "metric"},
            {"lang", weather_api_lang(chat_id)},
            {"appid", WEATHER_API_KEY}
        },
        cpr::Timeout{8000}
    );
}
