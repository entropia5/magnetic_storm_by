#include "weather_utils.h"

#include "utilities.h"

#include <map>

std::string weather_emoji_from_code(
    const std::string& icon_code,
    const std::string& description
) {
    if (icon_code.rfind("01", 0) == 0) return "☀️";
    if (icon_code.rfind("02", 0) == 0) return "🌤️";
    if (icon_code.rfind("03", 0) == 0 || icon_code.rfind("04", 0) == 0) return "☁️";
    if (icon_code.rfind("09", 0) == 0) return "🌧️";
    if (icon_code.rfind("10", 0) == 0) return "🌦️";
    if (icon_code.rfind("11", 0) == 0) return "⛈️";
    if (icon_code.rfind("13", 0) == 0) return "❄️";
    if (icon_code.rfind("50", 0) == 0) return "🌫️";
    if (description.find("дожд") != std::string::npos) return "🌧️";
    if (description.find("снег") != std::string::npos) return "❄️";
    if (description.find("гроз") != std::string::npos) return "⛈️";
    return "🌡️";
}

std::string weather_icon_for_description(const std::string& description) {
    if (description.find("ясно") != std::string::npos
        || description.find("солнечно") != std::string::npos) return "☀️";
    if (description.find("облачно") != std::string::npos) return "☁️";
    if (description.find("дожд") != std::string::npos) return "🌧️";
    if (description.find("снег") != std::string::npos) return "❄️";
    if (description.find("туман") != std::string::npos) return "🌫️";
    if (description.find("гроз") != std::string::npos) return "⛈️";
    return "🌡️";
}

std::string normalize_location(const std::string& location) {
    const std::string cleaned = trim_copy(location);
    if (cleaned.empty()) return cleaned;

    const std::map<std::string, std::string> city_map = {
        {"гомель", "Гомель"}, {"гомел", "Гомель"}, {"homel", "Гомель"}, {"gomel", "Гомель"},
        {"Гомель", "Гомель"}, {"Гомел", "Гомель"},
        {"минск", "Минск"}, {"minsk", "Минск"}, {"менск", "Мінск"},
        {"Минск", "Минск"}, {"Менск", "Мінск"},
        {"брест", "Брест"}, {"brest", "Брест"}, {"брэст", "Брэст"},
        {"Брест", "Брест"}, {"Брэст", "Брэст"},
        {"витебск", "Витебск"}, {"vitebsk", "Витебск"}, {"віцебск", "Віцебск"},
        {"Витебск", "Витебск"}, {"Віцебск", "Віцебск"},
        {"гродно", "Гродно"}, {"grodno", "Гродно"}, {"гародня", "Гродна"},
        {"Гродно", "Гродно"}, {"Гародня", "Гродна"},
        {"могилёв", "Могилёв"}, {"могилев", "Могилёв"}, {"mogilev", "Могилёв"},
        {"магілёў", "Магілёў"}, {"Могилёв", "Могилёв"}, {"Могилев", "Могилёв"},
        {"Магілёў", "Магілёў"}, {"копыль", "Копыль"}, {"копыл", "Копыль"},
        {"капы́ль", "Копыль"}, {"kapyl", "Kapyl"}, {"kopyl", "Kapyl"},
        {"Копыль", "Копыль"}, {"Копыл", "Копыль"}
    };

    const std::string lower = ascii_lower_copy(cleaned);
    const auto lower_match = city_map.find(lower);
    if (lower_match != city_map.end()) return lower_match->second;
    const auto exact_match = city_map.find(cleaned);
    return exact_match != city_map.end() ? exact_match->second : cleaned;
}
