#include "geomagnetic_client.h"
#include "time_utils.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <ctime>
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

std::vector<KpForecast> fetch_kp_forecast_3day(long long chat_id) {
    std::vector<KpForecast> forecast;
    const cpr::Response response = cpr::Get(
        cpr::Url{"https://services.swpc.noaa.gov/text/3-day-geomag-forecast.txt"},
        cpr::Timeout{10000}
    );
    if (response.status_code != 200) {
        return forecast;
    }

    try {
        std::vector<std::vector<double>> day_values(3);
        std::vector<std::string> dates;
        for (int offset = 0; offset < 3; ++offset) {
            const std::tm day = get_minsk_time(offset);
            std::ostringstream date;
            date << std::setw(2) << std::setfill('0') << day.tm_mday << '.'
                 << std::setw(2) << std::setfill('0') << day.tm_mon + 1;
            dates.push_back(date.str());
        }

        std::istringstream input(response.text);
        std::string line;
        while (std::getline(input, line)) {
            if (line.find("UT") == std::string::npos
                || line.find("UTC") != std::string::npos) {
                continue;
            }

            std::vector<double> values;
            std::istringstream line_input(line);
            std::string token;
            while (line_input >> token) {
                if (token.find("UT") != std::string::npos
                    || token.find('-') != std::string::npos) {
                    continue;
                }
                try {
                    const double value = std::stod(token);
                    if (value >= 0.0 && value <= 10.0) values.push_back(value);
                } catch (...) {
                }
            }
            if (values.size() >= 3) {
                for (int day = 0; day < 3; ++day) {
                    if (day_values[day].size() < 8) day_values[day].push_back(values[day]);
                }
            }
        }

        for (int day = 0; day < 3; ++day) {
            if (day_values[day].empty()) continue;
            KpForecast item;
            item.date = dates[day];
            item.values = day_values[day];
            for (const double value : item.values) {
                if (value > item.max_kp) item.max_kp = value;
            }
            item.status = kp_short_label(item.max_kp, chat_id);
            forecast.push_back(std::move(item));
        }
    } catch (const std::exception& error) {
        std::cerr << "Ошибка парсинга прогноза: " << error.what() << '\n';
    }
    return forecast;
}
