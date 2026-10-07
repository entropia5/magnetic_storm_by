#include "../src/domain_types.h"
#include "../src/geomagnetic_client.h"
#include "../src/localization.h"
#include "../src/presentation.h"
#include "../src/runtime_state.h"
#include "../src/screen_renderer.h"
#include "../src/screen_view_renderer.h"
#include "../src/telegram_input.h"
#include "../src/telegram_client.h"
#include "../src/telegram_screen_service.h"
#include "../src/text_format.h"
#include "../src/weather_utils.h"
#include "../src/weather_service.h"

#include <algorithm>
#include <cstdlib>
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

string render_test_image(long long chat_id, const ScreenView& view) {
    const auto path = render_screen_image(chat_id, view);
    if (std::getenv("GEOBOT_KEEP_TEST_IMAGES") && !path.empty()) {
        const string name = "bot_screens/preview_" + view.kind + "_" + to_string(chat_id);
        filesystem::copy_file(path, name + ".jpg", filesystem::copy_options::overwrite_existing);
        ofstream(name + ".html") << render_screen_html(chat_id, view);
    }
    return path;
}

int main() {
    if (std::getenv("GEOBOT_LIVE_DATA_CHECK")) {
        if (const char* key = std::getenv("OPENWEATHER_API_KEY")) WEATHER_API_KEY = key;
        auto forecast = fetch_kp_forecast_3day(1);
        const double current = fetch_current_kp();
        auto weather = fetch_weather_info("Минск", 1);
        auto slots = fetch_weather_forecast_slots("Минск", 1, 8);
        expect_true(forecast.size() == 3, "live NOAA forecast available");
        expect_true(kp_available(current), "live current Kp available");
        expect_true(weather.ok && slots.size() == 8, "live Minsk weather and eight forecast slots available");
        cout << "Live data: Kp=" << current << ", NOAA days=" << forecast.size() << ", weather=" << weather.ok << ", slots=" << slots.size() << endl;
        if (!slots.empty()) cout << "Minsk forecast begins: " << slots.front().time << endl;
        return test_failures ? 1 : 0;
    }
    if (const char* mock_url = std::getenv("GEOBOT_TRANSPORT_TEST_URL")) {
        ScreenView view;
        view.kind = "home";
        view.title = "Transport regression";
        for (const string scenario : {"force-fail", "force-ok", "edit-missing", "edit-retry", "text-fallback"}) {
            API_URL = string(mock_url) + "/" + scenario;
            live_message_id[1] = 42;
            supplement_message_id.clear();
            screen_renderer_available = scenario != "text-fallback";
            upsert_live_screen(1, view, scenario.rfind("force-", 0) == 0);
            const bool replacement_expected = scenario == "force-ok" || scenario == "edit-missing" || scenario == "text-fallback";
            expect_true(live_message_id[1] == (replacement_expected ? 100 : 42), scenario + " preserves or replaces message ID correctly");
        }
        return test_failures ? 1 : 0;
    }
    std::string forecast_fixture = ":Issued: 2026 Oct 05 2205 UTC\n             Oct 06 Oct 07 Oct 08\n";
    for (int i = 0; i < 8; ++i) {
        char interval[16];
        std::snprintf(interval, sizeof(interval), "%02d-%02dUT", i * 3, (i * 3 + 3) % 24);
        forecast_fixture += std::string(interval) + " 4.0 2.0 1.0\n";
    }
    auto parsed = parse_kp_forecast(forecast_fixture, 1);
    expect_true(parsed.size() == 3, "NOAA complete table yields three days");
    if (!parsed.empty()) {
        expect_contains(parsed.front().date, "6 ", "NOAA date comes from source, not local clock");
        expect_true(parsed.front().values.size() == 8, "NOAA has all eight intervals");
    }
    auto year_fixture = forecast_fixture;
    const auto table_start = year_fixture.find("Oct 06 Oct 07 Oct 08");
    year_fixture.replace(table_start, string("Oct 06 Oct 07 Oct 08").size(), "Dec 31 Jan 01 Jan 02");
    auto rollover = parse_kp_forecast(year_fixture, 1);
    expect_true(rollover.size() == 3 && rollover[1].date.find("2027") != string::npos, "NOAA forecast crosses New Year correctly");
    expect_true(parse_kp_forecast(":Issued: 2026 Oct 05 2205 UTC\nOct 06 Oct 07 Oct 08\n00-03UT 4 2 1\n", 1).empty(), "incomplete NOAA forecast is rejected");
    const bool keep_test_images = std::getenv("GEOBOT_KEEP_TEST_IMAGES") != nullptr;
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

    ScreenView cold_view;
    cold_view.kind = "weather";
    cold_view.show_weather = true;
    cold_view.weather.ok = true;
    cold_view.weather.temp = -8;
    cold_view.weather.feels_like = 32;
    auto temperature_html = render_screen_html(1, cold_view);
    expect_contains(temperature_html, "weather-temp' style='color:#83c5ff'", "cold temperature uses blue");
    expect_contains(temperature_html, "<b style='color:#ffad77'>32", "hot apparent temperature uses warm orange");
    ScreenView missing_view;
    missing_view.kind = "forecast";
    missing_view.supplement = "NOAA temporarily unavailable";
    expect_contains(render_screen_html(1, missing_view), "<section class='body'>NOAA temporarily unavailable", "API failure is visible inside image");

    if (validate_screen_renderer()) {
        for (long long lang_chat : {1LL, 2LL, 3LL}) {
            ScreenView home;
            home.kind = "home";
            home.title = localize(lang_chat, "Здравствуйте, Александр", "Вітаю, Аляксандр", "Hello, Alexander");
            home.subtitle = localize(lang_chat, "Погода и геомагнитная обстановка Беларуси", "Надвор’е і геамагнітная абстаноўка Беларусі", "Weather and geomagnetic activity in Belarus");
            home.body = localize(lang_chat, "Выберите раздел кнопками под карточкой.\n\nТекущий индекс Kp · прогноз на 3 дня · погода", "Выберыце раздзел кнопкамі пад карткай.\n\nБягучы індэкс Kp · прагноз на 3 дні · надвор’е", "Choose a section using the buttons below.\n\nCurrent Kp index · 3-day forecast · weather");
            auto home_path = render_test_image(lang_chat, home);
            expect_true(!home_path.empty(), "home renders in each language");
            if (!keep_test_images && !home_path.empty()) filesystem::remove(home_path);
        }
        ScreenView view;
        view.kind = "current";
        view.title = "Магнитные бури сейчас";
        view.subtitle = "Состояние геомагнитного поля";
        view.body = "**Спокойная геомагнитная обстановка.** Можно сохранять обычный режим дня.";
        view.kp = 3.3;
        string image_path = render_test_image(1, view);
        expect_true(!image_path.empty() && filesystem::exists(image_path), "JPEG render output exists");
        if (!keep_test_images && !image_path.empty()) {
            filesystem::remove(image_path);
        }

        ScreenView weather_view;
        weather_view.kind = "weather";
        weather_view.title = "Погода сейчас:";
        weather_view.weather.ok = true;
        weather_view.show_weather = true;
        weather_view.weather.name = "Заславль";
        weather_view.weather.settlement_kind = "town";
        weather_view.weather.description = "ясно";
        weather_view.weather.icon = "☀️";
        weather_view.weather.temp = 20;
        weather_view.weather.feels_like = 19;
        weather_view.weather.humidity = 61;
        weather_view.weather.wind_speed = 3.4;
        const vector<string> weather_times = {
            "00:00", "03:00", "06:00", "09:00",
            "12:00", "15:00", "18:00", "21:00"
        };
        for (int i = 0; i < 8; i++) {
            WeatherForecastSlot slot;
            slot.time = weather_times[i];
            slot.icon = i < 6 ? "☀️" : "🌦️";
            slot.description = i < 6 ? "ясно" : "небольшой дождь";
            slot.temp = 15 + i;
            slot.pop = i < 6 ? 0 : 40;
            slot.rain_mm = i < 6 ? 0.0 : 0.4;
            slot.wind_speed = 3.0 + (i % 3);
            weather_view.weather_slots.push_back(slot);
        }
        string weather_image_path = render_test_image(1, weather_view);
        auto [weather_width, weather_height] = jpeg_dimensions(weather_image_path);
        expect_equal(to_string(weather_width), "1280", "weather JPEG keeps fixed width");
        expect_true(weather_height >= 1500 && weather_height <= 2400, "weather JPEG includes all content within readable height");
        if (!keep_test_images && !weather_image_path.empty()) {
            filesystem::remove(weather_image_path);
        }

        ScreenView forecast_view;
        forecast_view.kind = "forecast";
        forecast_view.title = "Прогноз магнитных бурь на 3 дня";
        KpForecast public_forecast;
        public_forecast.date = "28 июля 2026";
        public_forecast.max_kp = 4.6;
        public_forecast.values = {2.3, 2.7, 3.0, 3.7, 4.6, 4.0, 3.3, 2.7};
        forecast_view.forecast.push_back(public_forecast);
        string forecast_image_path = render_test_image(1, forecast_view);
        auto [forecast_width, forecast_height] = jpeg_dimensions(forecast_image_path);
        expect_equal(to_string(forecast_width), "1280", "forecast JPEG keeps fixed width");
        expect_equal(to_string(forecast_height), "1500", "forecast JPEG keeps fixed height");
        if (!keep_test_images && !forecast_image_path.empty()) {
            filesystem::remove(forecast_image_path);
        }

        ScreenView alert_view;
        alert_view.kind = "alert";
        alert_view.title = "Магнитная буря";
        alert_view.subtitle = "Повышенная геомагнитная активность";
        alert_view.body = "**Уровень G2.** Снизьте нагрузку и следите за самочувствием.";
        alert_view.kp = 6.7;
        alert_view.alert = true;
        string alert_image_path = render_test_image(1, alert_view);
        expect_true(!alert_image_path.empty() && filesystem::exists(alert_image_path), "alert JPEG render output exists");
        if (!keep_test_images && !alert_image_path.empty()) {
            filesystem::remove(alert_image_path);
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
        string morning_image_path = render_test_image(1, morning_view);
        auto [morning_width, morning_height] = jpeg_dimensions(morning_image_path);
        expect_equal(to_string(morning_width), "1800", "morning JPEG keeps fixed width");
        expect_true(morning_height >= 1500 && morning_height <= 2600, "morning weather JPEG fits Telegram aspect ratio");
        ScreenView morning_forecast_view;
        morning_forecast_view.kind = "morning";
        morning_forecast_view.title = "Прогноз магнитных бурь на 3 дня";
        morning_forecast_view.subtitle = "День прогноза 1 из 3";
        morning_forecast_view.forecast.push_back(morning_forecast);
        auto morning_forecast_image = render_test_image(2, morning_forecast_view);
        auto morning_forecast_size = jpeg_dimensions(morning_forecast_image);
        expect_true(morning_forecast_size.first == 1800 && morning_forecast_size.second >= 1100, "morning forecast page renders");
        if (!keep_test_images && !morning_forecast_image.empty()) filesystem::remove(morning_forecast_image);
        if (!keep_test_images && !morning_image_path.empty()) {
            filesystem::remove(morning_image_path);
        }
    }

    if (test_failures == 0) {
        cout << "All unit tests passed" << endl;
    }
    return test_failures == 0 ? 0 : 1;
}
