#include "weather_service.h"

#include "weather_client.h"
#include "weather_utils.h"

#include <cmath>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

WeatherInfo fetch_weather_info(std::string location, long long chat_id) {
    WeatherInfo info;
    location = normalize_location(location);

    cpr::Response response = fetch_weather_response(location + ",BY", chat_id);
    if (response.status_code != 200) {
        response = fetch_weather_response(location, chat_id);
    }
    if (response.status_code != 200) {
        return info;
    }

    try {
        const json data = json::parse(response.text);
        if (data.contains("sys") && data["sys"].contains("country")
            && data["sys"]["country"].get<std::string>() != "BY") {
            return info;
        }

        info.ok = true;
        info.name = data["name"].get<std::string>();
        info.description = data["weather"][0]["description"].get<std::string>();
        info.temp = static_cast<int>(std::round(data["main"]["temp"].get<double>()));
        info.feels_like = static_cast<int>(
            std::round(data["main"]["feels_like"].get<double>())
        );
        info.humidity = data["main"]["humidity"].get<int>();
        info.wind_speed = data["wind"]["speed"].get<double>();

        const std::string icon_code = data["weather"][0].contains("icon")
            ? data["weather"][0]["icon"].get<std::string>()
            : "";
        info.icon = weather_emoji_from_code(icon_code, info.description);
    } catch (...) {
        info.ok = false;
    }

    return info;
}

std::vector<WeatherForecastSlot> fetch_weather_forecast_slots(
    std::string location,
    long long chat_id,
    std::size_t limit
) {
    std::vector<WeatherForecastSlot> slots;
    location = normalize_location(location);

    cpr::Response response = fetch_weather_forecast_response(location + ",BY", chat_id);
    if (response.status_code != 200) {
        response = fetch_weather_forecast_response(location, chat_id);
    }
    if (response.status_code != 200) {
        return slots;
    }

    try {
        const json data = json::parse(response.text);
        if (!data.contains("list") || !data["list"].is_array()) {
            return slots;
        }
        if (data.contains("city") && data["city"].contains("country")
            && data["city"]["country"].get<std::string>() != "BY") {
            return slots;
        }

        for (const auto& item : data["list"]) {
            if (slots.size() >= limit) {
                break;
            }

            WeatherForecastSlot slot;
            const std::string timestamp = item.contains("dt_txt")
                ? item["dt_txt"].get<std::string>()
                : "";
            slot.time = timestamp.size() >= 16 ? timestamp.substr(11, 5) : "";
            slot.temp = static_cast<int>(std::round(item["main"]["temp"].get<double>()));
            slot.feels_like = static_cast<int>(
                std::round(item["main"]["feels_like"].get<double>())
            );
            slot.humidity = item["main"]["humidity"].get<int>();
            slot.wind_speed = item.contains("wind") && item["wind"].contains("speed")
                ? item["wind"]["speed"].get<double>()
                : 0.0;
            slot.pop = item.contains("pop")
                ? static_cast<int>(std::round(item["pop"].get<double>() * 100.0))
                : 0;

            if (item.contains("weather") && item["weather"].is_array()
                && !item["weather"].empty()) {
                slot.description = item["weather"][0].contains("description")
                    ? item["weather"][0]["description"].get<std::string>()
                    : "";
                const std::string icon_code = item["weather"][0].contains("icon")
                    ? item["weather"][0]["icon"].get<std::string>()
                    : "";
                slot.icon = weather_emoji_from_code(icon_code, slot.description);
            } else {
                slot.icon = "🌡️";
            }

            if (item.contains("rain") && item["rain"].contains("3h")) {
                slot.rain_mm = item["rain"]["3h"].get<double>();
            }
            if (item.contains("snow") && item["snow"].contains("3h")) {
                slot.snow_mm = item["snow"]["3h"].get<double>();
            }
            slots.push_back(slot);
        }
    } catch (...) {
        slots.clear();
    }

    return slots;
}
