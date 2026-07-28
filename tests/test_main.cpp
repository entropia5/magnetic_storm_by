#include "../src/domain_types.h"
#include "../src/localization.h"
#include "../src/presentation.h"
#include "../src/runtime_state.h"
#include "../src/screen_renderer.h"
#include "../src/screen_view_renderer.h"
#include "../src/telegram_input.h"
#include "../src/telegram_client.h"
#include "../src/text_format.h"
#include "../src/weather_utils.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

using namespace std;

int test_failures = 0;

void expect_true(bool condition, const string& name) {
    if (!condition) {
        cerr << "FAIL: " << name << endl;
        test_failures++;
    }
}

void expect_equal(const string& actual, const string& expected, const string& name) {
    if (actual != expected) {
        cerr << "FAIL: " << name << " | expected=\"" << expected
             << "\" actual=\"" << actual << "\"" << endl;
        test_failures++;
    }
}

void expect_contains(const string& actual, const string& needle, const string& name) {
    if (actual.find(needle) == string::npos) {
        cerr << "FAIL: " << name << " | expected substring=\"" << needle
             << "\" actual=\"" << actual << "\"" << endl;
        test_failures++;
    }
}

pair<int, int> jpeg_dimensions(const string& path) {
    ifstream in(path, ios::binary);
    if (!in) return {0, 0};

    unsigned char marker[2] = {0, 0};
    in.read(reinterpret_cast<char*>(marker), 2);
    if (!in || marker[0] != 0xFF || marker[1] != 0xD8) {
        return {0, 0};
    }

    while (in) {
        unsigned char prefix = 0;
        in.read(reinterpret_cast<char*>(&prefix), 1);
        if (!in) break;
        if (prefix != 0xFF) continue;

        unsigned char code = 0;
        do {
            in.read(reinterpret_cast<char*>(&code), 1);
        } while (in && code == 0xFF);
        if (!in || code == 0xD9 || code == 0xDA) break;

        unsigned char len_bytes[2] = {0, 0};
        in.read(reinterpret_cast<char*>(len_bytes), 2);
        if (!in) break;
        int length = (len_bytes[0] << 8) | len_bytes[1];
        if (length < 2) break;

        bool sof = (code >= 0xC0 && code <= 0xC3)
            || (code >= 0xC5 && code <= 0xC7)
            || (code >= 0xC9 && code <= 0xCB)
            || (code >= 0xCD && code <= 0xCF);
        if (sof) {
            unsigned char frame[5] = {0, 0, 0, 0, 0};
            in.read(reinterpret_cast<char*>(frame), 5);
            if (!in) break;
            int height = (frame[1] << 8) | frame[2];
            int width = (frame[3] << 8) | frame[4];
            return {width, height};
        }

        in.seekg(length - 2, ios::cur);
    }

    return {0, 0};
}

int main() {
    {
        lock_guard<mutex> lock(state_mutex);
        user_language[1] = "ru";
        user_language[2] = "be";
        user_language[3] = "en";
    }

    expect_equal(normalize_location(" gomel "), "Гомель", "normalize latin Gomel");
    expect_equal(normalize_location("Менск"), "Мінск", "normalize Belarusian Minsk variant");
    expect_equal(normalize_location("Kopyl"), "Kapyl", "normalize Kopyl for OpenWeather");
    expect_equal(normalize_location("Несвиж"), "Несвиж", "keep unknown location");

    expect_true(kp_available(0.0), "Kp zero is available");
    expect_true(!kp_available(-1.0), "negative Kp is unavailable");
    expect_equal(kp_short_label(3.9, 1), "Спокойное геомагнитное поле", "Russian quiet Kp label");
    expect_equal(kp_short_label(6.1, 3), "Moderate geomagnetic storm G2", "English G2 Kp label");
    expect_equal(wind_unit(3), "m/s", "English wind unit");
    expect_equal(wind_unit(1), "м/с", "Russian wind unit");
    expect_true(is_command("/start", "/start"), "plain command matching");
    expect_true(is_command("/start@geomagnetic_belarus_bot arg", "/start"), "bot-addressed command matching");
    expect_true(!is_command("/starter", "/start"), "command prefix is not enough");

    WeatherForecastSlot slot;
    expect_equal(format_precipitation(slot, 1), "без осадков", "dry precipitation label");
    slot.pop = 35;
    expect_equal(format_precipitation(slot, 3), "35%", "precipitation probability label");
    slot.rain_mm = 1.25;
    expect_equal(format_precipitation(slot, 1), "1.2 мм", "precipitation mm label");

    KpForecast morning_fc;
    morning_fc.max_kp = 4.2;
    morning_fc.values = {3.0, 2.7, 3.4, 4.2};
    string morning_kp = morning_kp_detail(1, 3.3, {morning_fc});
    expect_contains(morning_kp, "Сейчас Kp 3.3", "morning Kp starts with current index");
    expect_contains(morning_kp, "минимум ожидается Kp 2.7", "morning Kp includes expected minimum");
    expect_contains(morning_kp, "максимум - Kp 4.2", "morning Kp includes expected maximum");

    WeatherInfo morning_weather;
    morning_weather.ok = true;
    morning_weather.name = "Копыль";
    morning_weather.description = "пасмурно";
    morning_weather.temp = 12;
    morning_weather.feels_like = 11;
    morning_weather.wind_speed = 4.4;
    WeatherForecastSlot morning_slot_a;
    morning_slot_a.time = "09:00";
    morning_slot_a.description = "пасмурно";
    morning_slot_a.temp = 11;
    morning_slot_a.wind_speed = 5.0;
    WeatherForecastSlot morning_slot_b;
    morning_slot_b.time = "12:00";
    morning_slot_b.description = "небольшой дождь";
    morning_slot_b.temp = 14;
    morning_slot_b.pop = 60;
    morning_slot_b.wind_speed = 6.0;
    string morning_weather_text = morning_weather_detail(1, morning_weather, {morning_slot_a, morning_slot_b});
    expect_contains(morning_weather_text, "Погода сейчас в городе Копыль", "morning weather includes current weather");
    expect_contains(morning_weather_text, "11...14°C", "morning weather includes expected temperature range");
    expect_contains(morning_weather_text, "осадки: 60%", "morning weather includes expected precipitation");

    expect_equal(html_escape("<b>&\"'\n"), "&lt;b&gt;&amp;&quot;&#39;<br>", "HTML escaping");
    expect_equal(markdown_to_telegram_html("**Kp** < 5 & ok"), "<b>Kp</b> &lt; 5 &amp; ok", "Telegram HTML markdown conversion");

    cpr::Response invalid_edit;
    invalid_edit.status_code = 400;
    invalid_edit.text = R"({"ok":false,"description":"Bad Request: message to edit not found"})";
    expect_true(telegram_edit_target_invalid(invalid_edit), "missing edit target is permanent");
    expect_true(!telegram_retryable_failure(invalid_edit), "missing edit target is not retryable");

    cpr::Response retryable_edit;
    retryable_edit.status_code = 429;
    retryable_edit.text = R"({"ok":false,"description":"Too Many Requests: retry later"})";
    expect_true(telegram_retryable_failure(retryable_edit), "429 is retryable");
    expect_true(!telegram_edit_target_invalid(retryable_edit), "429 does not reset live id");

    if (validate_screen_renderer()) {
        ScreenView view;
        view.title = "Render test";
        view.subtitle = "JPEG screen generation";
        view.body = "Body";
        view.kp = 3.3;
        string image_path = render_screen_image(1, view);
        expect_true(!image_path.empty() && filesystem::exists(image_path), "JPEG render output exists");
        if (!image_path.empty()) {
            filesystem::remove(image_path);
        }

        ScreenView weather_view;
        weather_view.kind = "weather";
        weather_view.title = "Погода сейчас";
        weather_view.weather.ok = true;
        weather_view.show_weather = true;
        weather_view.weather.name = "Очень Длинное Название Населенного Пункта Для Проверки Ширины";
        weather_view.weather.description = "продолжительный дождь с переменной облачностью";
        weather_view.weather.icon = "🌧️";
        weather_view.weather.temp = 12;
        weather_view.weather.feels_like = 9;
        weather_view.weather.humidity = 88;
        weather_view.weather.wind_speed = 7.4;
        for (int i = 0; i < 8; i++) {
            WeatherForecastSlot slot;
            slot.time = (i % 2 == 0) ? "09:00" : "12:00";
            slot.icon = "🌧️";
            slot.description = "продолжительный дождь с облачностью";
            slot.temp = 10 + i;
            slot.pop = 80;
            slot.wind_speed = 6.0 + i;
            weather_view.weather_slots.push_back(slot);
        }
        string weather_image_path = render_screen_image(1, weather_view);
        auto [weather_width, weather_height] = jpeg_dimensions(weather_image_path);
        expect_equal(to_string(weather_width), "1280", "weather JPEG keeps fixed width");
        expect_equal(to_string(weather_height), "1500", "weather JPEG keeps fixed height");
        if (!weather_image_path.empty()) {
            filesystem::remove(weather_image_path);
        }

        ScreenView morning_view;
        morning_view.kind = "morning";
        morning_view.title = "Доброе утро";
        morning_view.subtitle = "17 июня 2026, Среда";
        morning_view.weather = weather_view.weather;
        morning_view.show_weather = true;
        morning_view.weather_slots = weather_view.weather_slots;
        KpForecast morning_forecast;
        morning_forecast.date = "17 июня";
        morning_forecast.max_kp = 5.8;
        morning_forecast.values = {2.1, 2.4, 3.0, 5.8, 4.6, 3.8, 3.2, 2.7};
        morning_view.daily_storm_summary.push_back(morning_forecast);
        string morning_html = render_screen_html(1, morning_view);
        expect_contains(morning_html, "Пик дня", "morning storm summary includes peak label");
        expect_contains(morning_html, "09:00", "morning storm summary includes peak time");
        expect_contains(morning_html, "weather-slot-meta-compact", "morning weather uses compact slot metadata");
        string morning_image_path = render_screen_image(1, morning_view);
        auto [morning_width, morning_height] = jpeg_dimensions(morning_image_path);
        expect_equal(to_string(morning_width), "1800", "morning JPEG keeps fixed width");
        expect_equal(to_string(morning_height), "1500", "morning JPEG keeps fixed height");
        if (!morning_image_path.empty()) {
            filesystem::remove(morning_image_path);
        }
    }

    if (test_failures == 0) {
        cout << "All unit tests passed" << endl;
    }
    return test_failures == 0 ? 0 : 1;
}
