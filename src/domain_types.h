#pragma once

#include <string>
#include <vector>

struct KpForecast {
    std::string date;
    double max_kp = 0.0;
    std::string status;
    std::vector<double> values;
};

struct WeatherInfo {
    bool ok = false;
    std::string name;
    std::string description;
    std::string icon;
    int temp = 0;
    int feels_like = 0;
    int humidity = 0;
    double wind_speed = 0.0;
};

struct WeatherForecastSlot {
    std::string time;
    std::string icon;
    std::string description;
    int temp = 0;
    int feels_like = 0;
    int humidity = 0;
    double wind_speed = 0.0;
    int pop = 0;
    double rain_mm = 0.0;
    double snow_mm = 0.0;
};

struct ScreenView {
    std::string kind = "status";
    std::string eyebrow;
    std::string title;
    std::string subtitle;
    std::string body;
    std::string footer;
    std::string supplement;
    std::string city;
    double kp = -1.0;
    bool alert = false;
    bool show_weather = false;
    int forecast_page = -1;
    int forecast_total = 0;
    std::string page_callback = "forecast";
    WeatherInfo weather;
    std::vector<WeatherForecastSlot> weather_slots;
    std::vector<KpForecast> forecast;
    std::vector<KpForecast> daily_storm_summary;
};
