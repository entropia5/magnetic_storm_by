#include "geomagnetic_client.h"
#include "localization.h"
#include "time_utils.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <ctime>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

std::string kp_short_label(double kp, long long chat_id);

double fetch_current_kp() {
    const cpr::Response response = cpr::Get(
        cpr::Url{"https://services.swpc.noaa.gov/json/planetary_k_index_1m.json"},
        cpr::Timeout{8000}
    );

    if (response.status_code == 200) {
        try {
            const nlohmann::json data = nlohmann::json::parse(response.text);
            if (data.is_array() && !data.empty()) {
                const auto& last = data.back();
                if (last.contains("estimated_kp")) {
                    return last["estimated_kp"].get<double>();
                }
                if (last.contains("kp_index")) {
                    return last["kp_index"].get<double>();
                }
            }
        } catch (const std::exception& error) {
            std::cerr << "Ошибка парсинга текущего Kp: " << error.what() << '\n';
        }
    }
    std::cerr << "Не удалось получить текущий Kp: HTTP "
              << response.status_code << '\n';
    return -1.0;
}


std::vector<KpForecast> parse_kp_forecast(const std::string& text, long long chat_id) {
    const std::vector<std::string> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::istringstream input(text);
    std::string line;
    int year = 0;
    int issued_month = 0;
    std::vector<KpForecast> result;
    while (std::getline(input, line)) {
        if (line.rfind(":Issued:", 0) == 0) {
            std::istringstream issued(line.substr(8));
            std::string issued_month_name;
            issued >> year >> issued_month_name;
            auto issued_found = std::find(months.begin(), months.end(), issued_month_name);
            if (issued_found != months.end()) issued_month = static_cast<int>(issued_found - months.begin()) + 1;
        }
        std::istringstream header(line);
        std::string month;
        int day = 0;
        std::vector<KpForecast> dates;
        int header_year = year;
        int previous_month = issued_month;
        while (header >> month >> day) {
            auto found = std::find(months.begin(), months.end(), month);
            if (found == months.end() || day < 1 || day > 31 || year < 2000) break;
            int month_number = static_cast<int>(found - months.begin()) + 1;
            if (previous_month == 12 && month_number == 1) ++header_year;
            previous_month = month_number;
            KpForecast fc;
            fc.date = std::to_string(day) + " " + get_month_name(month_number, chat_id) + " " + std::to_string(header_year);
            dates.push_back(fc);
        }
        if (dates.size() == 3) { result = std::move(dates); continue; }
        if (result.size() != 3) continue;
        std::istringstream row(line);
        std::string interval;
        double values[3];
        if (!(row >> interval >> values[0] >> values[1] >> values[2])) continue;
        const size_t index = result.front().values.size();
        if (index >= 8) continue;
        char expected[16];
        std::snprintf(expected, sizeof(expected), "%02d-%02dUT", static_cast<int>(index * 3), static_cast<int>((index * 3 + 3) % 24));
        if (interval != expected) continue;
        for (double value : values) if (!std::isfinite(value) || value < 0 || value > 9) return {};
        for (int i = 0; i < 3; ++i) {
            result[i].values.push_back(values[i]);
            result[i].max_kp = std::max(result[i].max_kp, values[i]);
        }
    }
    if (result.size() != 3 || result.front().values.size() != 8) return {};
    for (auto& fc : result) fc.status = kp_short_label(fc.max_kp, chat_id);
    return result;
}

std::vector<KpForecast> fetch_kp_forecast_3day(long long chat_id) {
    const auto response = cpr::Get(cpr::Url{"https://services.swpc.noaa.gov/text/3-day-geomag-forecast.txt"}, cpr::Timeout{10000});
    if (response.status_code != 200) {
        std::cerr << "Прогноз NOAA недоступен: HTTP " << response.status_code << '\n';
        return {};
    }
    auto forecast = parse_kp_forecast(response.text, chat_id);
    if (forecast.empty()) std::cerr << "Некорректная или неполная таблица прогноза NOAA\n";
    return forecast;
}
