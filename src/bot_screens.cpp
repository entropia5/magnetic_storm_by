#include "bot_screens.h"

#include "geomagnetic_client.h"
#include "localization.h"
#include "storage.h"
#include "telegram_screen_service.h"
#include "text_format.h"
#include "time_utils.h"
#include "utilities.h"
#include "weather_client.h"
#include "weather_service.h"
#include "weather_utils.h"

#include <algorithm>

using namespace std;

string forecast_day_supplement(long long chat_id, const KpForecast& forecast);
string get_current_kp_guidance(double kp, long long chat_id);
bool kp_available(double kp);
string kp_short_label(double kp, long long chat_id);
string kp_unavailable_text(long long chat_id);
string storm_level_label(double kp);
string weather_supplement_text(long long chat_id, const WeatherInfo& weather, const vector<WeatherForecastSlot>& slots, bool saved_city);

void show_home_screen(long long chat_id, const string& user_name, bool force_new_message) {
    ScreenView view;
    view.kind = "home";
    string clean_name = trim_copy(user_name).empty()
        ? localize(chat_id, "пользователь", "карыстальнік", "user")
        : trim_copy(user_name);
    view.title = localize(chat_id,
        "Здравствуйте, " + clean_name,
        "Вітаю, " + clean_name,
        "Hello, " + clean_name);
    upsert_live_screen(chat_id, view, force_new_message);
}

void show_current_screen(long long chat_id) {
    ScreenView view;
    view.kind = "current";
    view.title = localize(chat_id, "Текущий индекс", "Бягучы індэкс", "Current index");
    view.kp = fetch_current_kp();
    if (!kp_available(view.kp)) {
        view.supplement = kp_unavailable_text(chat_id);
    } else {
        view.supplement = localize(chat_id,
            "**Текущий индекс магнитных бурь на сейчас.**",
            "**Бягучы індэкс магнітных бур цяпер.**",
            "**Current geomagnetic storm index right now.**");
        view.body = get_current_kp_guidance(view.kp, chat_id);
    }
    upsert_live_screen(chat_id, view);
}

void show_forecast_screen(long long chat_id, int page) {
    ScreenView view;
    view.kind = "forecast";
    view.title = localize(chat_id, "Прогноз на 3 дня", "Прагноз на 3 дні", "3-day forecast");
    vector<KpForecast> forecast = fetch_kp_forecast_3day(chat_id);
    if (forecast.empty()) {
        view.supplement = localize(chat_id,
            "Не удалось получить прогноз NOAA. Попробуйте обновить экран позже.",
            "Не атрымалася атрымаць прагноз NOAA. Паспрабуйце абнавіць экран пазней.",
            "Could not load the NOAA forecast. Try refreshing later.");
    } else {
        page = max(0, min(page, (int)forecast.size() - 1));
        view.forecast.push_back(forecast[page]);
        view.forecast_page = page;
        view.forecast_total = (int)forecast.size();
        view.supplement = forecast_day_supplement(chat_id, forecast[page]);
    }
    upsert_live_screen(chat_id, view);
}

void show_weather_prompt_screen(long long chat_id) {
    ScreenView view;
    view.kind = "weather_prompt";
    view.title = localize(chat_id, "Погода сейчас", "Надвор'е цяпер", "Weather now");
    view.subtitle = get_text(chat_id, "enter_city_weather");
    view.supplement = localize(chat_id,
        "Напишите населённый пункт...",
        "Напішыце населены пункт...",
        "Send a location...");
    upsert_live_screen(chat_id, view);
}

void show_city_prompt_screen(long long chat_id) {
    ScreenView view;
    view.kind = "city_prompt";
    view.title = localize(chat_id, "Изменить город", "Змяніць горад", "Change city");
    view.subtitle = get_text(chat_id, "enter_city");
    view.supplement = localize(chat_id,
        "Этот город будет использоваться для утреннего отчёта.",
        "Гэты горад будзе выкарыстоўвацца для ранішняй справаздачы.",
        "This city will be used for the morning report.");
    upsert_live_screen(chat_id, view);
}

void show_weather_result_screen(long long chat_id, const string& location, bool save_city) {
    string normalized = normalize_location(location);

    ScreenView view;
    view.kind = save_city ? "city_saved" : "weather";
    view.title = save_city
        ? localize(chat_id, "Город сохранён", "Горад захаваны", "City saved")
        : localize(chat_id, "Погода сейчас", "Надвор'е цяпер", "Weather now");

    if (!weather_configured()) {
        view.subtitle = get_text(chat_id, "weather_api_missing");
        view.supplement = localize(chat_id,
            "Администратору нужно задать переменную окружения OPENWEATHER_API_KEY и перезапустить бота.",
            "Адміністратару трэба задаць зменную асяроддзя OPENWEATHER_API_KEY і перазапусціць бота.",
            "Set the OPENWEATHER_API_KEY environment variable and restart the bot.");
        upsert_live_screen(chat_id, view);
        return;
    }

    WeatherInfo weather = fetch_weather_info(normalized, chat_id);

    if (weather.ok) {
        if (save_city) {
            save_user_city(chat_id, normalized);
        }
        view.weather = weather;
        view.show_weather = true;
        view.weather_slots = fetch_weather_forecast_slots(normalized, chat_id, 8);
        view.supplement = weather_supplement_text(chat_id, weather, view.weather_slots, save_city);
    } else {
        view.subtitle = get_text(chat_id, "city_not_found");
        view.supplement = localize(chat_id,
            "Проверьте название и попробуйте ещё раз.",
            "Праверце назву і паспрабуйце яшчэ раз.",
            "Check the name and try again.");
    }

    upsert_live_screen(chat_id, view);
}

void show_notifications_screen(long long chat_id, bool enabled) {
    ScreenView view;
    view.kind = "notifications";
    view.title = enabled
        ? localize(chat_id, "Уведомления включены", "Апавяшчэнні ўключаны", "Alerts enabled")
        : localize(chat_id, "Уведомления выключены", "Апавяшчэнні выключаны", "Alerts disabled");
    view.subtitle = enabled ? get_text(chat_id, "notifications_on") : get_text(chat_id, "notifications_off");
    view.supplement = enabled
        ? localize(chat_id,
            "**Уведомления включены.** Если Kp поднимется до 5.0 и выше, бот обновит live-экран алертом магнитной бури и даст рекомендации.",
            "**Апавяшчэнні ўключаны.** Калі Kp падымецца да 5.0 і вышэй, бот абновіць live-экран алертам магнітнай буры і дасць рэкамендацыі.",
            "**Alerts enabled.** If Kp rises to 5.0 or higher, the bot will update the live screen with a storm alert and recommendations.")
        : localize(chat_id,
            "**Уведомления выключены.** Бот не будет присылать алерты о магнитных бурях, пока вы снова их не включите.",
            "**Апавяшчэнні выключаны.** Бот не будзе дасылаць алерты пра магнітныя буры, пакуль вы зноў іх не ўключыце.",
            "**Alerts disabled.** The bot will not send magnetic storm alerts until you enable them again.");
    view.kp = fetch_current_kp();
    if (!kp_available(view.kp)) {
        view.supplement += "\n\n" + kp_unavailable_text(chat_id);
    }
    upsert_live_screen(chat_id, view);
}

void show_language_screen(long long chat_id) {
    ScreenView view;
    view.kind = "language";
    view.title = localize(chat_id, "Язык изменён", "Мова зменена", "Language changed");
    view.subtitle = get_text(chat_id, "language_changed");
    view.supplement = localize(chat_id,
        "Все кнопки, подписи и live-экраны теперь будут в выбранном языке.",
        "Усе кнопкі, подпісы і live-экраны цяпер будуць на выбранай мове.",
        "Buttons, labels and live screens now use the selected language.");
    upsert_live_screen(chat_id, view);
}

void show_alert_screen(long long chat_id, double current_kp, bool force_new_message) {
    ScreenView view;
    view.kind = "alert";
    view.alert = current_kp >= 5.0;
    string storm_level = storm_level_label(current_kp);
    view.title = current_kp >= 5.0
        ? localize(chat_id,
            "Внимание. Магнитная буря " + storm_level,
            "Увага. Магнітная бура " + storm_level,
            "Warning. Geomagnetic storm " + storm_level)
        : localize(chat_id, "Буря затихла", "Бура сціхла", "Storm has calmed");
    view.subtitle = localize(chat_id,
        "Сейчас Kp " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id),
        "Цяпер Kp " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id),
        "Now Kp " + format_double_1(current_kp) + " - " + kp_short_label(current_kp, chat_id));
    view.kp = current_kp;
    view.supplement = view.title + "\n\n";
    view.supplement += localize(chat_id,
        "Сейчас Kp " + format_double_1(current_kp) + ".",
        "Цяпер Kp " + format_double_1(current_kp) + ".",
        "Current Kp is " + format_double_1(current_kp) + ".");
    view.supplement += "\n\n" + get_current_kp_guidance(current_kp, chat_id);
    upsert_live_screen(chat_id, view, force_new_message);
}

void send_morning_report(long long chat_id, int page) {
    tm ltm = get_minsk_time();

    string user_city_name = user_city_or_default(chat_id);

    ScreenView view;
    view.kind = "morning";
    view.page_callback = "morning";

    vector<KpForecast> forecast = fetch_kp_forecast_3day(chat_id);
    int total_pages = forecast.empty() ? 1 : (int)forecast.size() + 1;
    page = max(0, min(page, total_pages - 1));
    view.forecast_page = page;
    view.forecast_total = total_pages;

    string date_text = to_string(ltm.tm_mday) + " " + get_month_name(ltm.tm_mon + 1, chat_id) + " " +
        to_string(ltm.tm_year + 1900) + ", " + get_weekday_name(0, chat_id);

    if (page == 0) {
        view.title = localize(chat_id, "Доброе утро", "Добрай раніцы", "Good morning");
        view.subtitle = date_text;
        view.weather = fetch_weather_info(user_city_name, chat_id);
        view.show_weather = view.weather.ok;
        if (view.weather.ok) {
            view.weather_slots = fetch_weather_forecast_slots(user_city_name, chat_id, 8);
        }
        if (!forecast.empty()) {
            view.daily_storm_summary.push_back(forecast.front());
        }
        view.supplement = localize(chat_id,
            "**Утренняя сводка на " + date_text + ".**",
            "**Ранішняя зводка на " + date_text + ".**",
            "**Morning summary for " + date_text + ".**");
    } else {
        int forecast_index = page - 1;
        view.title = localize(chat_id, "Утренний прогноз", "Ранішні прагноз", "Morning forecast");
        view.subtitle = localize(chat_id,
            "День " + to_string(page) + " из " + to_string(max(1, total_pages - 1)),
            "Дзень " + to_string(page) + " з " + to_string(max(1, total_pages - 1)),
            "Day " + to_string(page) + " of " + to_string(max(1, total_pages - 1)));
        if (forecast_index >= 0 && forecast_index < (int)forecast.size()) {
            view.forecast.push_back(forecast[forecast_index]);
            view.supplement = forecast_day_supplement(chat_id, forecast[forecast_index]);
        } else {
            view.supplement = localize(chat_id,
                "Прогноз NOAA сейчас недоступен. Вернитесь к утренней сводке.",
                "Прагноз NOAA цяпер недаступны. Вярніцеся да ранішняй зводкі.",
                "NOAA forecast is currently unavailable. Return to the morning summary.");
        }
    }

    view.footer = localize(chat_id,
        "Утренний отчёт · " + user_city_name + " · страница " + to_string(page + 1) + " из " + to_string(total_pages),
        "Ранішняя справаздача · " + user_city_name + " · старонка " + to_string(page + 1) + " з " + to_string(total_pages),
        "Morning report · " + user_city_name + " · page " + to_string(page + 1) + " of " + to_string(total_pages));

    upsert_live_screen(chat_id, view);
}

